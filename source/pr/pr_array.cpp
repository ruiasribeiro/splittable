#include "splittable/pr/pr_array.hpp"

namespace splittable::pr {

std::atomic_uint pr_array::thread_id_counter{0};
// needs to be set with register_thread()
thread_local uint pr_array::thread_id;
// needs to be set with set_num_threads()
uint pr_array::num_threads;

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

// should only be called once, at the start of the thread. it is assumed that
// once a thread is registered it runs until the end
auto pr_array::register_thread() -> void {
  thread_id = thread_id_counter.fetch_add(1, std::memory_order_relaxed);
}

// should only be called once, at the start of the program
auto pr_array::set_num_threads(uint num) -> void { num_threads = num; }

auto pr_array::try_transisiton(double abort_rate, uint waiting,
                               uint aborts_for_no_stock) -> void {
  auto is_splitted = this->is_splitted.GetReadOnly();

  // TODO: add "total ratio of clients" condition
  if (is_splitted && (waiting > 0 || aborts_for_no_stock > 0)) {
    WSTM::Atomically([this](WSTM::WAtomic& at) { this->reconcile(at); });
  } else if (!is_splitted && abort_rate > 0.65) {
    // TODO: make it run locked
    WSTM::Atomically([this](WSTM::WAtomic& at) {
      this->split(at, split_operation_addsub);  // there's only one op
    });
  }
}

auto pr_array::split(WSTM::WAtomic& at, split_operation op) -> void {
  if (this->is_splitted.Get(at)) {
    // TODO: throw a better exception
    throw std::exception();
  }

  switch (op) {
    case split_operation_addsub: {
      auto new_splitted = std::make_shared<Splitted>();

      new_splitted->op = op;

      auto current_value = this->single_value.Get(at);
      auto chunk_val = current_value / num_threads;
      auto remainder = current_value % num_threads;

      new_splitted->chunks.reserve(num_threads);

      new_splitted->chunks[0] = WSTM::WVar<uint>(chunk_val + remainder);
      for (auto i = 1u; i < num_threads; ++i) {
        new_splitted->chunks[i] = WSTM::WVar<uint>(chunk_val);
      }

      this->splitted_value.Set(new_splitted, at);

      break;
    }
    default:
      // TODO: unexpected case, deal with it
      break;
  }

  this->is_splitted.Set(true, at);
}

auto pr_array::reconcile(WSTM::WAtomic& at) -> void {
  if (!this->is_splitted.Get(at)) {
    // TODO: throw a better exception
    throw std::exception();
  }

  auto splitted = this->splitted_value.Get(at);

  switch (splitted->op) {
    case split_operation_addsub: {
      auto new_value = 0u;

      for (auto i = 0u; i < num_threads; ++i) {
        new_value += splitted->chunks[i].Get(at);
      }

      this->single_value.Set(new_value, at);

      break;
    }
    default:
      // TODO: unexpected case, deal with it
      break;
  }

  this->is_splitted.Set(false, at);
}

}  // namespace splittable::pr
