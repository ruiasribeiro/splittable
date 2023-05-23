#include "splittable/mrv/mrv_array.hpp"

namespace splittable::mrv {

std::atomic_uint mrv_array::id_counter{0};

mrv_array::mrv_array(uint size) {
  this->id = mrv_array::id_counter.fetch_add(1, std::memory_order_relaxed);

  this->chunks.reserve(size);
  for (auto i = 0u; i < size; ++i) {
    this->chunks.push_back(WSTM::WVar<uint>{0});
  }

  this->valid_chunks = WSTM::WVar<size_t>{size};
}

auto mrv_array::new_mrv(uint size) -> std::shared_ptr<mrv> {
  auto obj = std::make_shared<mrv_array>(size);
  manager::get_instance().register_mrv(obj);
  return obj;
}

auto mrv_array::delete_mrv(std::shared_ptr<mrv> obj) -> void {
  manager::get_instance().deregister_mrv(obj);
}

auto mrv_array::get_id() -> uint { return this->id; }

auto setup_actions(WSTM::WAtomic& at, uint id) {
  at.OnFail(
      [id]() { manager::get_instance().report_txn(txn_status_aborted, id); });
  at.After(
      [id]() { manager::get_instance().report_txn(txn_status_completed, id); });
}

auto mrv_array::read(WSTM::WAtomic& at) -> uint {
  setup_actions(at, this->id);

  uint sum = 0;
  auto size = this->valid_chunks.Get(at);

  for (size_t i = 0; i < size; ++i) {
    sum += this->chunks.at(i).Get(at);
  }

  return sum;
}

auto mrv_array::add(WSTM::WAtomic& at, uint value) -> void {
  setup_actions(at, this->id);

  auto size = this->valid_chunks.Get(at);
  auto index = utils::random_index(0, size - 1);

  auto current_value = this->chunks.at(index).Get(at);
  current_value += value;
  this->chunks.at(index).Set(current_value, at);
}

auto mrv_array::sub(WSTM::WAtomic& at, uint value) -> void {
  setup_actions(at, this->id);

  auto size = this->valid_chunks.Get(at);
  auto start = utils::random_index(0, size - 1);
  auto index = start;
  auto success = false;

  while (!success) {
    auto current_chunk = this->chunks.at(index).Get(at);

    if (current_chunk > value) {
      this->chunks.at(index).Set(current_chunk - value, at);
      success = true;
    } else if (current_chunk != 0) {
      value -= current_chunk;
      this->chunks.at(index).Set(0, at);
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

auto mrv_array::add_nodes(double abort_rate) -> void {
  try {
    WSTM::Atomically([&](WSTM::WAtomic& at) {
      auto size = this->valid_chunks.Get(at);

      // TODO: this is not exactly like the original impl, revise later
      if (size >= MAX_NODES) {
        throw exception();
      }

      auto to_add = std::min((size_t)std::lround(1 + size * abort_rate),
                             MAX_NODES - size);

      if (to_add < 1) {
        throw exception();
      }

      auto new_size = size + to_add;
      this->valid_chunks.Set(new_size, at);

#ifdef SPLITTABLE_DEBUG
      at.After([abort_rate, new_size, id = this->id]() {
        std::cout << "increased id=" << id << " w/abort " << abort_rate
                  << " | new size: " << new_size << "\n";
      });
#endif

      auto capacity = this->chunks.size();
      if (capacity >= new_size) {
        return;
      }

      for (auto pushes = new_size - capacity; pushes > 0; --pushes) {
        chunks.push_back(WSTM::WVar<uint>{0});
      }
    });
  } catch (...) {
    // there is no problem if an exception is thrown, this will be tried again
    // in the next adjustment phase
  }
}

auto mrv_array::remove_node() -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "attempt to decrease id=" << this->id << "\n";
#endif

  try {
    WSTM::Atomically([&](WSTM::WAtomic& at) {
      auto size = this->valid_chunks.Get(at);

      // TODO: check if removing does not go below min nodes
      if (size < 2) {
        throw exception();
      }

      // move data to existing record
      auto last_chunk = chunks.at(size - 1).Get(at);
      auto absorber_index = utils::random_index(0, size - 2);
      auto& absorber = chunks.at(absorber_index);
      absorber.Set(absorber.Get(at) + last_chunk, at);

      // reduce chunks size
      this->valid_chunks.Set(size - 1, at);
    });
  } catch (...) {
    // there is no problem if an exception is thrown, this will be tried again
    // in the next adjustment phase
  }
}

auto mrv_array::balance() -> void {
  auto num_chunks = chunks.size();

  if (num_chunks < 2) {
    return;
  }

  auto i = utils::random_index(0, num_chunks - 1);
  auto j = utils::random_index(0, num_chunks - 1);

  if (i == j) {
    j = (i + 1) % num_chunks;
  }

  WSTM::Atomically([&](WSTM::WAtomic& at) {
    auto ci = chunks.at(i).Get(at);
    auto cj = chunks.at(j).Get(at);

    if (ci <= cj + MIN_BALANCE_DIFF && cj <= ci + MIN_BALANCE_DIFF) {
      // the difference isn't enough to balance, abort
      // TODO: use an abort?
      return;
    }

    auto new_value = (ci + cj) / 2;
    auto remainder = (ci + cj) % 2;

    chunks[i].Set(new_value + remainder, at);
    chunks[j].Set(new_value, at);

    // at.After([=]() {
    //   std::cout << "Balance: " << ci << " " << cj << " -> "
    //             << new_value + remainder << " " << new_value << "\n";
    // });
  });
}

}  // namespace splittable::mrv
