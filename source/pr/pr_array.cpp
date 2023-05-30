#include "splittable/pr/pr_array.hpp"

namespace splittable::pr {

std::atomic_uint pr_array::thread_id_counter{0};
// needs to be set with register_thread()
thread_local uint pr_array::thread_id;
// needs to be set with set_num_threads()
uint pr_array::num_threads;

pr_array::pr_array() {
  this->id = id_counter.fetch_add(1, std::memory_order_relaxed);
  this->single_value = WSTM::WVar<uint>(0);
  this->is_splitted = WSTM::WVar<bool>(false);
}

auto pr_array::new_pr() -> std::shared_ptr<pr> {
  auto obj = std::make_shared<pr_array>();
  manager::get_instance().register_pr(obj);
  return obj;
}

auto pr_array::delete_pr(std::shared_ptr<pr> obj) -> void {
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

auto pr_array::read(WSTM::WAtomic& at) -> uint {
  at.OnFail([this]() { this->add_aborts(1); });
  at.After([this]() { this->add_commits(1); });

  if (this->is_splitted.Get(at)) {
    this->add_waiting(1);
    WSTM::Retry(at);
  }

  return this->single_value.Get(at);
}

auto pr_array::add(WSTM::WAtomic& at, uint to_add) -> void {
  at.OnFail([this]() { this->add_aborts(1); });
  at.After([this]() { this->add_commits(1); });

  if (this->is_splitted.Get(at)) {
    auto splitted = this->splitted_value.Get(at);

    if (splitted->op != split_operation_addsub) {
      this->add_waiting(1);
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
  at.OnFail([this]() { this->add_aborts(1); });
  at.After([this]() { this->add_commits(1); });

  if (this->is_splitted.Get(at)) {
    auto splitted = this->splitted_value.Get(at);

    if (splitted->op != split_operation_addsub) {
      this->add_waiting(1);
      WSTM::Retry(at);
    }

    auto chunk = &splitted->chunks[this->thread_id];
    auto current_chunk = chunk->Get(at);

    if (current_chunk < to_sub) {
      // TODO: change this to a better exception
      this->add_aborts_for_no_stock(1);
      throw std::exception();
    }

    chunk->Set(current_chunk - to_sub, at);
  } else {
    auto current_value = this->single_value.Get(at);

    if (current_value < to_sub) {
      // TODO: change this to a better exception
      this->add_aborts_for_no_stock(1);
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

auto pr_array::try_transition(double abort_rate, uint waiting,
                              uint aborts_for_no_stock) -> void {
  auto is_splitted = this->is_splitted.GetReadOnly();

  // std::cout << "ar=" << abort_rate;
  // TODO: add "total ratio of clients" condition
  if (is_splitted && (waiting > 0 || aborts_for_no_stock > 0)) {
    try {
      WSTM::Atomically(
          [this](WSTM::WAtomic& at) { this->reconcile(at); },
          WSTM::WMaxConflicts(0, WSTM::WConflictResolution::RUN_LOCKED));
    } catch (...) {
      // there is no problem if an exception is thrown, this will be tried again
      // later
    }
  } else if (!is_splitted && abort_rate > 0.65) {
    try {
      WSTM::Atomically(
          [this](WSTM::WAtomic& at) {
            this->split(at, split_operation_addsub);  // there's only one op
          },
          WSTM::WMaxConflicts(0, WSTM::WConflictResolution::RUN_LOCKED));
    } catch (...) {
      // there is no problem if an exception is thrown, this will be tried again
      // later
    }
  }
}

auto pr_array::split(WSTM::WAtomic& at, split_operation op) -> void {
  if (this->is_splitted.Get(at)) {
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
#ifdef SPLITTABLE_DEBUG
  at.After([id = this->id]() { std::cout << "splitted id=" << id << "\n"; });
#endif
}

auto pr_array::reconcile(WSTM::WAtomic& at) -> void {
  if (!this->is_splitted.Get(at)) {
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
#ifdef SPLITTABLE_DEBUG
  at.After(
      [id = this->id]() { std::cout << "reconcillied id=" << id << "\n"; });
#endif
}

}  // namespace splittable::pr
