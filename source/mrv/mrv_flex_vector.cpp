#include "splittable/mrv/mrv_flex_vector.hpp"

namespace splittable::mrv {

mrv_flex_vector::mrv_flex_vector(uint size) {
  this->id = mrv::id_counter.fetch_add(1, std::memory_order_relaxed);

  auto t = immer::flex_vector_transient<std::shared_ptr<WSTM::WVar<uint>>>{};
  for (auto i = 0u; i < size; ++i) {
    t.push_back(std::make_shared<WSTM::WVar<uint>>(0u));
  }
  this->chunks = t.persistent();
}

auto mrv_flex_vector::new_mrv(uint size) -> std::shared_ptr<mrv> {
  auto obj = std::make_shared<mrv_flex_vector>(size);
  manager::get_instance().register_mrv(obj);
  return obj;
}

auto mrv_flex_vector::delete_mrv(std::shared_ptr<mrv> obj) -> void {
  manager::get_instance().deregister_mrv(obj);
}

auto mrv_flex_vector::get_id() -> uint { return this->id; }

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
  // a read does not count for the purposes of the abort rate, so no lambdas are
  // needed here

  uint sum = 0;

  {
    // this will improve performance since we are reading a lot of variables in
    // one go
    WSTM::WReadLockGuard<WSTM::WAtomic> lock(at);

    auto chunks = this->chunks;
    for (auto&& value : chunks) {
      sum += value->Get(at);
    }
  }

  return sum;
}

auto mrv_flex_vector::add(WSTM::WAtomic& at, uint value) -> void {
  at.OnFail([this]() { this->add_aborts(1u); });
  at.After([this]() { this->add_commits(1u); });

  auto chunks = this->chunks;
  auto index = utils::random_index(0, chunks.size() - 1);

  auto current_value = chunks.at(index)->Get(at);
  current_value += value;
  chunks.at(index)->Set(current_value, at);
}

auto mrv_flex_vector::sub(WSTM::WAtomic& at, uint value) -> void {
  at.OnFail([this]() { this->add_aborts(1u); });
  at.After([this]() { this->add_commits(1u); });

  auto chunks = this->chunks;
  auto size = chunks.size();
  auto start = utils::random_index(0, size - 1);
  auto index = start;
  auto success = false;

  while (!success) {
    auto current_chunk = chunks.at(index)->Get(at);

    if (current_chunk > value) {
      chunks.at(index)->Set(current_chunk - value, at);
      success = true;
    } else if (current_chunk != 0) {
      value -= current_chunk;
      chunks.at(index)->Set(0, at);
    }

    index = (index + 1) % size;
    if (index == start) {
      break;
    }
  }

  if (!success) {
    throw exception();
  }
}

auto mrv_flex_vector::add_nodes(double abort_rate) -> void {
  auto chunks = this->chunks;
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

  auto new_size = size + to_add;
  auto t = chunks.transient();
  for (auto i = 0u; i < to_add; ++i) {
    t.push_back(std::make_shared<WSTM::WVar<uint>>(0u));
  }
  this->chunks = t.persistent();

#ifdef SPLITTABLE_DEBUG
  std::cout << "increased id=" << id << " w/abort " << abort_rate
            << " | new size: " << new_size << "\n";
#endif
}

auto mrv_flex_vector::remove_node() -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "attempt to decrease id=" << this->id << "\n";
#endif

  auto chunks = this->chunks;
  auto size = chunks.size();

  // TODO: check if removing does not go below min nodes
  if (size < 2) {
    return;
  }

  WSTM::Atomically([&](WSTM::WAtomic& at) {
    // move data to existing record
    auto last_chunk = chunks.at(size - 1)->Get(at);
    auto absorber_index = utils::random_index(0, size - 2);
    auto absorber = chunks.at(absorber_index);
    absorber->Set(absorber->Get(at) + last_chunk, at);
  });

  this->chunks = chunks.take(size - 1);
}

auto mrv_flex_vector::balance() -> void {
  auto chunks = this->chunks;
  auto size = chunks.size();

  if (size < 2) {
    return;
  }

  auto i = utils::random_index(0, size - 1);
  auto j = utils::random_index(0, size - 1);

  if (i == j) {
    j = (i + 1) % size;
  }

  try {
    WSTM::Atomically([&](WSTM::WAtomic& at) {
      auto ci = chunks.at(i)->Get(at);
      auto cj = chunks.at(j)->Get(at);

      if (ci <= cj + MIN_BALANCE_DIFF && cj <= ci + MIN_BALANCE_DIFF) {
        throw exception();
      }

      auto new_value = (ci + cj) / 2;
      auto remainder = (ci + cj) % 2;

      chunks[i]->Set(new_value + remainder, at);
      chunks[j]->Set(new_value, at);
    });
  } catch (...) {
    // there is no problem if an exception is thrown, this will be tried again
    // in the next balance phase
  }
}

}  // namespace splittable::mrv
