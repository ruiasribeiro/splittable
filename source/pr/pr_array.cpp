#include "splittable/pr/pr_array.hpp"

namespace splittable::pr {

auto pr_array::read(WSTM::WAtomic& at) -> uint {
  auto current_value = &this->value.Get(at);

  if (!std::holds_alternative<Single>(*current_value)) {
    WSTM::Retry(at);
  }

  return std::get<Single>(*current_value);
}

auto pr_array::add(WSTM::WAtomic& at, uint to_add) -> void {
  auto value = this->value.Get(at);

  if (auto val = std::get_if<Single>(&value)) {
    this->value.Set(*val + to_add, at);
  } else if (auto val = std::get_if<Splitted>(&value)) {
    auto chunk = &val->chunks.at(this->thread_id);
    auto current_value = chunk->Get(at);
    chunk->Set(current_value + to_add, at);
  }
}

}  // namespace splittable::pr