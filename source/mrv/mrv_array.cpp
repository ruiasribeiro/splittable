#include "splittable/mrv/mrv_array.hpp"

namespace splittable::mrv {

std::atomic_uint mrv_array::id_counter{0};

mrv_array::mrv_array(uint size) {
  this->id = mrv_array::id_counter.fetch_add(1, std::memory_order_relaxed);
  this->chunks.reserve(size);
  for (auto _ : std::ranges::iota_view{0u, size}) {
    this->chunks.push_back(WSTM::WVar<uint>{0});
  }
}

auto mrv_array::new_mrv(uint size) -> std::shared_ptr<mrv> {
  auto obj = std::make_shared<mrv_array>(size);
  manager::get_instance().register_mrv(obj);
  return obj;
}

auto mrv_array::delete_mrv(std::shared_ptr<mrv>) -> void {
  // TODO
}

auto mrv_array::get_id() -> uint { return this->id; }

auto setup_actions(WSTM::WAtomic& at, uint id) {
  at.OnFail(
      [id]() { manager::get_instance().report_txn(txn_status::aborted, id); });
  at.After([id]() {
    manager::get_instance().report_txn(txn_status::completed, id);
  });
}

auto mrv_array::read(WSTM::WAtomic& at) -> uint {
  setup_actions(at, this->id);

  uint sum = 0;

  for (auto&& var : this->chunks) {
    sum += var.Get(at);
  }

  return sum;
}

auto mrv_array::add(WSTM::WAtomic& at, uint value) -> void {
  setup_actions(at, this->id);

  auto index = utils::random_index(0, this->chunks.size() - 1);

  auto current_value = this->chunks.at(index).Get(at);
  current_value += value;
  this->chunks.at(index).Set(current_value, at);
}

auto mrv_array::sub(WSTM::WAtomic& at, uint value) -> void {
  setup_actions(at, this->id);

  auto size = this->chunks.size();
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

auto mrv_array::add_nodes(double abort_rate) -> void {}

auto mrv_array::remove_node() -> void {}

auto mrv_array::balance() -> void {}

}  // namespace splittable::mrv
