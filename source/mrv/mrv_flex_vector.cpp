#include "splittable/mrv/mrv_flex_vector.hpp"

namespace splittable::mrv {

mrv_flex_vector::mrv_flex_vector(uint size) {
  this->id = mrv::id_counter.fetch_add(1, std::memory_order_relaxed);

  auto t = immer::flex_vector_transient<std::shared_ptr<WSTM::WVar<uint>>>{};
  for (auto i = 0u; i < size; ++i) {
    t.push_back(std::make_shared<WSTM::WVar<uint>>(0u));
  }
  this->chunks = chunks_type(t.persistent());
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
  auto chunks = this->chunks.Get(at);

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
  at.OnFail([this]() { this->add_aborts(1u); });
  at.After([this]() { this->add_commits(1u); });

  auto chunks = this->chunks.Get(at);
  auto index = utils::random_index(0, chunks.size() - 1);

  auto current_value = chunks.at(index)->Get(at);
  current_value += value;
  chunks.at(index)->Set(current_value, at);
}

auto mrv_flex_vector::sub(WSTM::WAtomic& at, uint value) -> void {
  at.OnFail([this]() { this->add_aborts(1u); });
  at.After([this]() { this->add_commits(1u); });

  auto chunks = this->chunks.Get(at);
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
  try {
    auto new_size = WSTM::Atomically([&](WSTM::WAtomic& at) -> ulong {
      auto chunks = this->chunks.Get(at);
      auto size = chunks.size();

      // TODO: this is not exactly like the original impl, revise later
      if (size >= MAX_NODES) {
        throw std::exception();
      }

      auto to_add = std::min((size_t)std::lround(1 + size * abort_rate),
                             MAX_NODES - size);

      if (to_add < 1) {
        throw std::exception();
      }

      auto new_size = size + to_add;
      auto t = chunks.transient();
      for (auto i = 0u; i < to_add; ++i) {
        t.push_back(std::make_shared<WSTM::WVar<uint>>(0u));
      }
      this->chunks = chunks_type(t.persistent());

      return new_size;
    });

#ifdef SPLITTABLE_DEBUG
    std::cout << "increased id=" << id << " w/abort " << abort_rate
              << " | new size: " << new_size << "\n";
#endif
  } catch (...) {
    // there is no problem if an exception is thrown, this will be tried again
    // in the next adjustment phase
  }
}

auto mrv_flex_vector::remove_node() -> void {
  try {
    WSTM::Atomically([&](WSTM::WAtomic& at) {
      auto chunks = this->chunks.Get(at);
      auto size = chunks.size();

      // TODO: check if removing does not go below min nodes
      if (size < 2) {
        throw std::exception();
      }

      // move data to existing record
      auto last_chunk = chunks.at(size - 1)->Get(at);
      auto absorber_index = utils::random_index(0, size - 2);
      auto absorber = chunks.at(absorber_index);
      absorber->Set(absorber->Get(at) + last_chunk, at);

      this->chunks = chunks_type(chunks.take(size - 1));
    });

#ifdef SPLITTABLE_DEBUG
    std::cout << "decreased id=" << this->id << " by one\n";
#endif
  } catch (...) {
    // there is no problem if an exception is thrown, this will be tried again
    // in the next adjustment phase
  }
}

auto mrv_flex_vector::balance() -> void {
  try {
    WSTM::Atomically([&](WSTM::WAtomic& at) {
      auto chunks = this->chunks.Get(at);
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
        read_value = chunks.at(i)->Get(at);

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

      chunks.at(min_i)->Set(new_value + remainder, at);
      chunks.at(max_i)->Set(new_value, at);
    });
  } catch (...) {
    // there is no problem if an exception is thrown, this will be tried again
    // in the next balance phase
  }
}

}  // namespace splittable::mrv