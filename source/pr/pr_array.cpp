#include "splittable/pr/pr_array.hpp"

namespace splittable::pr {

thread_local uint pr_array::thread_id;

auto pr_array::read(WSTM::WAtomic& at) -> uint {
  if (this->is_splitted.Get(at)) {
    WSTM::Retry(at);
  }

  return this->single_value.Get(at);
}

auto pr_array::add(WSTM::WAtomic& at, uint to_add) -> void {
  if (this->is_splitted.Get(at)) {
    auto splitted = this->splitted_value.Get(at);

    if (splitted->op != split_operation_addsub) {
      WSTM::Retry(at);
    }

    auto chunk = &splitted->chunks[this->thread_id];
    auto current_chunk = chunk->Get(at);
    chunk->Set(current_chunk + to_add, at);
  } else {
    auto current_value = this->single_value.Get(at);
    this->single_value.Set(current_value + to_add, at);
  }
}

auto pr_array::sub(WSTM::WAtomic& at, uint to_sub) -> void {
  if (this->is_splitted.Get(at)) {
    auto splitted = this->splitted_value.Get(at);

    if (splitted->op != split_operation_addsub) {
      WSTM::Retry(at);
    }

    auto chunk = &splitted->chunks[this->thread_id];
    auto current_chunk = chunk->Get(at);

    if (current_chunk < to_sub) {
      // TODO: change this to a better exception
      throw std::exception();
    }

    chunk->Set(current_chunk - to_sub, at);
  } else {
    auto current_value = this->single_value.Get(at);

    if (current_value < to_sub) {
      // TODO: change this to a better exception
      throw std::exception();
    }

    this->single_value.Set(current_value - to_sub, at);
  }
}

}  // namespace splittable::pr
