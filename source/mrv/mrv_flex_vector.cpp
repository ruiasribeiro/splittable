#include "splittable/mrv/mrv_flex_vector.hpp"

namespace splittable::mrv {

std::function<void(WSTM::WAtomic&, chunks_t)> mrv_flex_vector::balance_strategy;

mrv_flex_vector::mrv_flex_vector(uint value) : status_counters(0) {
  this->id = mrv::id_counter.fetch_add(1, std::memory_order_relaxed);

  // const auto size = 2;
  // auto new_value = value / size;
  // auto remainder = value % size;

  // immer::flex_vector_transient<std::shared_ptr<chunk_t>> transient_chunks;
  // transient_chunks.push_back(std::make_shared<chunk_t>(new_value + remainder));
  // for (size_t i = 1; i < size; ++i) {
  //   transient_chunks.push_back(std::make_shared<chunk_t>(new_value));
  // }
  // auto chunks = transient_chunks.persistent();

  auto chunks = chunks_t{std::make_shared<chunk_t>(value)};
  this->chunks = std::make_shared<chunks_t>(chunks);
}

auto mrv_flex_vector::new_instance(uint value)
    -> std::shared_ptr<mrv_flex_vector> {
  auto obj = std::make_shared<mrv_flex_vector>(value);
  manager::get_instance().register_mrv(obj);
  return obj;
}

auto mrv_flex_vector::delete_instance(std::shared_ptr<mrv_flex_vector> obj)
    -> void {
  manager::get_instance().deregister_mrv(obj);
}

auto mrv_flex_vector::get_id() -> uint { return this->id; }

auto mrv_flex_vector::get_avg_adjust_interval() -> std::chrono::nanoseconds {
  return manager::get_instance().get_avg_adjust_interval();
}

auto mrv_flex_vector::get_avg_balance_interval() -> std::chrono::nanoseconds {
  return manager::get_instance().get_avg_balance_interval();
}

auto mrv_flex_vector::get_avg_phase_interval() -> std::chrono::nanoseconds {
  return std::chrono::nanoseconds(0);
}

auto mrv_flex_vector::reset_global_stats() -> void {
  splittable::reset_global_stats();
  manager::get_instance().reset_global_stats();
}

auto mrv_flex_vector::add_aborts(uint count) -> void {
  this->status_counters.fetch_add(count << 16, std::memory_order_relaxed);
}

auto mrv_flex_vector::add_commits(uint count) -> void {
  this->status_counters.fetch_add(count, std::memory_order_relaxed);
}

auto mrv_flex_vector::fetch_and_reset_status() -> status {
  auto counters =
      this->status_counters.fetch_and(0u, std::memory_order_relaxed);

  auto commits = (counters & 0x0000FFFF);
  auto aborts = (counters & 0xFFFF0000) >> 16;

  return {.aborts = aborts, .commits = commits};
}

auto mrv_flex_vector::read(WSTM::WAtomic& at) -> uint {
  setup_transaction_tracking(at);

  // a read does not count for the purposes of the abort rate, so no lambdas are
  // needed here

  uint sum = 0;
  auto chunks = *std::atomic_load(&this->chunks).get();
  {
    // this will improve performance since we are reading a lot of variables in
    // one go
    WSTM::WReadLockGuard<WSTM::WAtomic> lock(at);

    for (auto&& value : chunks) {
      sum += value->Get(at);
    }
  }

  return sum;
}

auto mrv_flex_vector::add(WSTM::WAtomic& at, uint value) -> void {
  setup_transaction_tracking(at);

  at.OnFail([this]() { this->add_aborts(1u); });
  at.After([ptr = weak_from_this()]() {
    // this weak_ptr was needed to avoid a segfault in Vacation
    if (auto value = ptr.lock()) {
      value->add_commits(1u);
    }
  });

  auto chunks = *std::atomic_load(&this->chunks).get();
  auto index = utils::random_index(0, chunks.size() - 1);

  auto current_value = chunks[index]->Get(at);
  assert(current_value <= (current_value + value));  // wraparound check
  current_value += value;
  chunks[index]->Set(current_value, at);
}

auto mrv_flex_vector::sub(WSTM::WAtomic& at, uint value) -> void {
  setup_transaction_tracking(at);

  auto no_stock = std::make_shared<bool>(false);

  at.OnFail([this, no_stock]() {
    // if this function is called and `no_stock` is true, we know that the
    // transaction failed due to no stock, so we do not need to increment the
    // abort counter
    if (*no_stock.get()) {
      return;
    }
    this->add_aborts(1u);
  });
  at.After([this]() { this->add_commits(1u); });

  auto chunks = *std::atomic_load(&this->chunks).get();
  auto size = chunks.size();
  auto start = utils::random_index(0, size - 1);
  auto index = start;
  auto success = false;

  while (true) {
    auto current_chunk = chunks[index]->Get(at);

    if (current_chunk > value) {
      chunks[index]->Set(current_chunk - value, at);
      success = true;
      break;
    } else if (current_chunk != 0) {
      value -= current_chunk;
      chunks[index]->Set(0, at);
    }

    index = (index + 1) % size;
    if (index == start) {
      break;
    }
  }

  if (!success) {
    *no_stock = true;
    throw exception();
  }
}

auto mrv_flex_vector::add_nodes(double abort_rate) -> void {
  auto chunks = *std::atomic_load(&this->chunks).get();
  auto size = chunks.size();

  // TODO: this is not exactly like the original impl, revise later
  if (size >= MAX_NODES) {
    return;
  }

  auto to_add =
      std::min((size_t)std::lround(1 + size * abort_rate), MAX_NODES - size);

  if (to_add < 1) {
    return;
  }

  auto t = chunks.transient();
  for (auto i = 0u; i < to_add; ++i) {
    t.push_back(std::make_shared<chunk_t>(0u));
  }

  std::atomic_store(&this->chunks, std::make_shared<chunks_t>(t.persistent()));

#ifdef SPLITTABLE_DEBUG
  auto new_size = size + to_add;
  std::cout << "increased id=" << id << " w/abort " << abort_rate
            << " | new size: " << new_size << "\n";
#endif
}

auto mrv_flex_vector::remove_node() -> void {
  auto chunks = *std::atomic_load(&this->chunks).get();
  auto size = chunks.size();

  // TODO: check if removing does not go below min nodes
  if (size < 2) {
    return;
  }

  auto last_chunk = chunks[size - 1];
  auto absorber = chunks[utils::random_index(0, size - 2)];
  auto new_chunks = std::make_shared<chunks_t>(chunks.take(size - 1));

  WSTM::Atomically(
      [&](WSTM::WAtomic& at) {
        auto last_chunk_value = last_chunk->Get(at);

        // this ensures that other threads reading/writing to the last_chunk
        // will conflict
        last_chunk->Set(0, at);

        if (last_chunk_value > 0) {
          absorber->Set(absorber->Get(at) + last_chunk_value, at);
        }

        std::atomic_store(&this->chunks, new_chunks);
      },
      // this should make the transaction irrevocable; if it doesn't,
      // there's no problem anyway
      WSTM::WMaxConflicts(0, WSTM::WConflictResolution::RUN_LOCKED));

#ifdef SPLITTABLE_DEBUG
  std::cout << "decreased id=" << this->id << " by one\n";
#endif
}

auto mrv_flex_vector::balance() -> void {
  try {
    WSTM::Atomically([&](WSTM::WAtomic& at) {
      auto chunks = *std::atomic_load(&this->chunks).get();
      balance_strategy(at, chunks);
    });
  } catch (exception& ex) {
    // there is no problem if an exception is thrown, this will be tried again
    // in the next balance phase
  }
}

auto balance_all(WSTM::WAtomic& at, chunks_t chunks) -> void {
  auto size = chunks.size();

  if (size < 2) {
    throw exception();
  }

  uint64_t total = 0;
  {
    WSTM::WReadLockGuard<WSTM::WAtomic> lock(at);
    for (auto i = 0u; i < size; ++i) {
      total += chunks[i]->Get(at);
    }
  }

  // TODO: check for unneeded balances; not sure if it is feasible here, we
  // would have to make many comparisons just to check if we need to abort,
  // seems like it would incur in a big overhead

  auto new_value = total / size;
  auto remainder = total % size;

  chunks[0]->Set(new_value + remainder, at);
  for (auto i = 1u; i < size; ++i) {
    chunks[i]->Set(new_value, at);
  }
}

auto balance_minmax(WSTM::WAtomic& at, chunks_t chunks) -> void {
  auto size = chunks.size();

  if (size < 2) {
    throw exception();
  }

  auto min_i = 0u;
  auto min_v = UINT_MAX;
  auto max_i = 0u;
  auto max_v = 0u;

  uint read_value;
  for (auto i = 0u; i < size; ++i) {
    read_value = chunks[i]->Get(at);

    if (read_value > max_v) {
      max_v = read_value;
      max_i = i;
    }

    if (read_value <= min_v) {
      min_v = read_value;
      min_i = i;
    }
  }

  if (min_i == max_i || max_v - min_v <= MIN_BALANCE_DIFF) {
    throw exception();
  }

  auto new_value = (max_v + min_v) / 2;
  auto remainder = (max_v + min_v) % 2;

  chunks[min_i]->Set(new_value + remainder, at);
  chunks[max_i]->Set(new_value, at);
}

auto compare_less(const std::pair<uint, uint>& a,
                  const std::pair<uint, uint>& b) -> bool {
  return a.second < b.second;
};

auto compare_greater(const std::pair<uint, uint>& a,
                     const std::pair<uint, uint>& b) -> bool {
  return a.second > b.second;
};

auto constexpr calculate_k(size_t num_records) -> size_t {
  if (num_records < 4) {
    return 1;
  }

  if (num_records <= 16) {
    return 2;
  }

  if (num_records < 64) {
    return num_records / 8;
  }

  return num_records / 16;
}

auto balance_minmax_with_k(WSTM::WAtomic& at, chunks_t chunks) -> void {
  auto size = chunks.size();

  if (size < 2) {
    throw exception();
  }

  const uint k = calculate_k(size);

  std::priority_queue<std::pair<uint, uint>, std::vector<std::pair<uint, uint>>,
                      std::function<bool(const std::pair<uint, uint>& a,
                                         const std::pair<uint, uint>& b)>>
      smallest_numbers(compare_less);
  std::priority_queue<std::pair<uint, uint>, std::vector<std::pair<uint, uint>>,
                      std::function<bool(const std::pair<uint, uint>& a,
                                         const std::pair<uint, uint>& b)>>
      largest_numbers(compare_greater);

  uint read_value;
  for (auto i = 0u; i < size; ++i) {
    read_value = chunks[i]->Get(at);

    if (smallest_numbers.size() < k) {
      smallest_numbers.push(std::make_pair(i, read_value));
    } else if (read_value < smallest_numbers.top().second) {
      smallest_numbers.pop();
      smallest_numbers.push(std::make_pair(i, read_value));
    }

    if (largest_numbers.size() < k) {
      largest_numbers.push(std::make_pair(i, read_value));
    } else if (read_value > largest_numbers.top().second) {
      largest_numbers.pop();
      largest_numbers.push(std::make_pair(i, read_value));
    }
  }

  std::vector<uint> indexes;
  indexes.reserve(k + k);

  uint64_t total = 0;
  while (!smallest_numbers.empty()) {
    auto small = smallest_numbers.top();
    smallest_numbers.pop();
    indexes.push_back(small.first);
    total += small.second;
  }
  while (!largest_numbers.empty()) {
    auto large = largest_numbers.top();
    largest_numbers.pop();
    indexes.push_back(large.first);
    total += large.second;
  }

  // TODO: check for unneeded balances
  // if (min_i == max_i || max_v - min_v <= MIN_BALANCE_DIFF) {
  //   throw exception();
  // }

  auto new_value = total / (k + k);
  auto remainder = total % (k + k);

  for (auto& i : indexes) {
    if (i == indexes[0]) {
      chunks[i]->Set(new_value + remainder, at);
    } else {
      chunks[i]->Set(new_value, at);
    }
  }
}

auto balance_none(WSTM::WAtomic&, chunks_t) -> void {}

auto balance_random(WSTM::WAtomic& at, chunks_t chunks) -> void {
  auto size = chunks.size();

  if (size < 2) {
    throw exception();
  }

  auto i = utils::random_index(0, size - 1);
  auto j = utils::random_index(0, size - 1);

  if (i == j) {
    j = (i + 1) % size;
  }

  auto i_val = chunks[i]->Get(at);
  auto j_val = chunks[j]->Get(at);

  if (i_val == j_val || (i_val > j_val && i_val - j_val <= MIN_BALANCE_DIFF) ||
      (i_val < j_val && j_val - i_val <= MIN_BALANCE_DIFF)) {
    throw exception();
  }

  auto new_value = (i_val + j_val) / 2;
  auto remainder = (i_val + j_val) % 2;

  chunks[i]->Set(new_value + remainder, at);
  chunks[j]->Set(new_value, at);
}

auto mrv_flex_vector::set_balance_strategy(balance_strategy_t strategy)
    -> void {
  switch (strategy) {
    case balance_strategy_t::none:
      balance_strategy = balance_none;
      break;
    case balance_strategy_t::random:
      balance_strategy = balance_random;
      break;
    case balance_strategy_t::minmax:
      balance_strategy = balance_minmax;
      break;
    case balance_strategy_t::all:
      balance_strategy = balance_all;
      break;
    default:
      assert(false);
  }
}

}  // namespace splittable::mrv
