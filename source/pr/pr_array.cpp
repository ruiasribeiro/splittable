#include "splittable/pr/pr_array.hpp"

namespace splittable::pr {

pr_array::pr_array(uint value) : status_counters(0) {
  this->id = id_counter.fetch_add(1, std::memory_order_relaxed);
  this->single_value = WSTM::WVar<uint>(value);
  this->is_splitted = WSTM::WVar<bool>(false);
}

auto pr_array::new_instance(uint value) -> std::shared_ptr<pr_array> {
  auto obj = std::make_shared<pr_array>(value);
  manager::get_instance().register_pr(obj);
  return obj;
}

auto pr_array::delete_instance(std::shared_ptr<pr_array> obj) -> void {
  manager::get_instance().deregister_pr(obj);
}

auto pr_array::get_id() -> uint { return this->id; }

auto pr_array::add_aborts(uint count) -> void {
  this->status_counters.fetch_add(((ulong)count) << 48,
                                  std::memory_order_relaxed);
}

auto pr_array::add_aborts_for_no_stock(uint count) -> void {
  this->status_counters.fetch_add(((ulong)count) << 32,
                                  std::memory_order_relaxed);
}

auto pr_array::add_commits(uint count) -> void {
  this->status_counters.fetch_add(((ulong)count) << 16,
                                  std::memory_order_relaxed);
}

auto pr_array::add_waiting(uint count) -> void {
  this->status_counters.fetch_add((ulong)count, std::memory_order_relaxed);
}

auto pr_array::fetch_and_reset_status() -> status {
  auto counters =
      this->status_counters.fetch_and(0u, std::memory_order_relaxed);

  uint waiting = (counters & 0x000000000000FFFF);
  uint commits = (counters & 0x00000000FFFF0000) >> 16;
  uint aborts_for_no_stock = (counters & 0x0000FFFF00000000) >> 32;
  uint aborts = (counters & 0xFFFF000000000000) >> 48;

  return {.aborts = aborts,
          .aborts_for_no_stock = aborts_for_no_stock,
          .commits = commits,
          .waiting = waiting};
}

auto pr_array::setup_status_tracking(WSTM::WAtomic& at) -> void {
  at.OnFail([this]() { this->add_aborts(1); });
  at.After([ptr = weak_from_this()]() {
    if (auto value = ptr.lock()) {
      value->add_commits(1u);
    }
  });
}

auto pr_array::read(WSTM::WAtomic& at) -> uint {
  setup_transaction_tracking(at);
  setup_status_tracking(at);

  if (this->is_splitted.Get(at)) {
    this->add_waiting(1);
    WSTM::Retry(at);
  }

  return this->single_value.Get(at);
}

auto pr_array::add(WSTM::WAtomic& at, uint to_add) -> void {
  setup_transaction_tracking(at);
  setup_status_tracking(at);

  if (this->is_splitted.Get(at)) {
    auto splitted = this->splitted_value.Get(at);
    auto chunk = &splitted->at(this->thread_id);
    auto current_chunk = chunk->Get(at);
    chunk->Set(current_chunk + to_add, at);
  } else {
    auto current_value = this->single_value.Get(at);
    this->single_value.Set(current_value + to_add, at);
  }
}

auto pr_array::sub(WSTM::WAtomic& at, uint to_sub) -> void {
  setup_transaction_tracking(at);
  setup_status_tracking(at);

  if (this->is_splitted.Get(at)) {
    auto splitted = this->splitted_value.Get(at);
    auto chunk = &splitted->at(this->thread_id);
    auto current_chunk = chunk->Get(at);

    if (current_chunk < to_sub) {
      this->add_aborts_for_no_stock(1);
      // TODO: change this to a better exception
      throw std::exception();
    }

    chunk->Set(current_chunk - to_sub, at);
  } else {
    auto current_value = this->single_value.Get(at);

    if (current_value < to_sub) {
      this->add_aborts_for_no_stock(1);
      // TODO: change this to a better exception
      throw std::exception();
    }

    this->single_value.Set(current_value - to_sub, at);
  }
}

auto pr_array::try_transition(double abort_rate, uint waiting,
                              uint aborts_for_no_stock) -> void {
  try {
    WSTM::Atomically(
        [ptr = weak_from_this(), waiting, aborts_for_no_stock,
         abort_rate](WSTM::WAtomic& at) {
          if (auto value = ptr.lock()) {
            auto is_splitted = value->is_splitted.Get(at);

            // TODO: add "total ratio of clients" condition
            if (is_splitted && (waiting > 0 || aborts_for_no_stock > 0)) {
              value->reconcile(at);
            } else if (!is_splitted && abort_rate > 0.65) {
              value->split(at);  // there's only one op
            } else {
              throw exception();
            }
          }
        },
        WSTM::WMaxConflicts(0, WSTM::WConflictResolution::RUN_LOCKED));
  } catch (...) {
    // there is no problem if an exception is thrown, this will be tried again
    // later
  }
}

auto pr_array::split(WSTM::WAtomic& at) -> void {
  if (this->is_splitted.Get(at)) {
    throw std::exception();
  }

  auto new_splitted = std::make_shared<splitted_t>();

  auto current_value = this->single_value.Get(at);
  auto chunk_val = current_value / num_threads;
  auto remainder = current_value % num_threads;

  new_splitted->reserve(num_threads);
  new_splitted->emplace_back(chunk_val + remainder);
  for (auto i = 1u; i < num_threads; ++i) {
    new_splitted->emplace_back(chunk_val);
  }

  this->splitted_value.Set(new_splitted, at);
  this->is_splitted.Set(true, at);

#ifdef SPLITTABLE_DEBUG
  at.After([id = this->id]() { std::cout << "splitted id=" << id << "\n"; });
#endif
}

auto pr_array::reconcile(WSTM::WAtomic& at) -> void {
  if (!this->is_splitted.Get(at)) {
    throw std::exception();
  }

  auto splitted = this->splitted_value.Get(at);

  auto new_value = 0u;

  {
    // this will improve performance since we are reading a lot of variables in
    // one go
    WSTM::WReadLockGuard<WSTM::WAtomic> lock(at);

    for (auto i = 0u; i < num_threads; ++i) {
      new_value += splitted->at(i).Get(at);
    }
  }

  this->single_value.Set(new_value, at);
  this->is_splitted.Set(false, at);

#ifdef SPLITTABLE_DEBUG
  at.After(
      [id = this->id]() { std::cout << "reconcillied id=" << id << "\n"; });
#endif
}

}  // namespace splittable::pr
