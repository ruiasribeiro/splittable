#include "splittable/mrv/mrv_array.hpp"

namespace splittable::mrv {

auto mrv_array::read(WSTM::WAtomic& at) -> std::expected<uint, error> {
  return std::accumulate(this->chunks.begin(), this->chunks.end(), 0,
                         [&at](uint acc, const WSTM::WVar<uint>& b) -> uint {
                           return acc + b.Get(at);
                         });
}

auto mrv_array::add(WSTM::WAtomic& at, uint value)
    -> std::expected<void, error> {
  auto index = utils::random_index(0, this->chunks.size());

  auto current_value = this->chunks.at(index).Get(at);
  current_value += value;
  this->chunks.at(index).Set(current_value, at);

  return {};
}

auto mrv_array::sub(WSTM::WAtomic& at, uint value)
    -> std::expected<void, error> {
  auto size = this->chunks.size();
  auto start = utils::random_index(0, size);
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
    return std::unexpected(error::insufficient_value);
  }

  return {};
}

}  // namespace splittable::mrv
