#include "splittable/single/single.hpp"

namespace splittable::single {
single::single(uint value) : value(value) {}

auto single::thread_init() -> void {}

auto single::global_init(uint) -> void {}

auto single::new_instance(uint value) -> std::shared_ptr<single> {
  return std::make_shared<single>(value);
}

auto single::delete_instance(std::shared_ptr<single>) -> void {}

auto single::get_avg_adjust_interval() -> std::chrono::nanoseconds {
  return std::chrono::nanoseconds(0);
}

auto single::get_avg_balance_interval() -> std::chrono::nanoseconds {
  return std::chrono::nanoseconds(0);
}

auto single::get_avg_phase_interval() -> std::chrono::nanoseconds {
  return std::chrono::nanoseconds(0);
}

auto single::reset_global_stats() -> void { splittable::reset_global_stats(); }

auto single::read(WSTM::WAtomic& at) -> uint {
  setup_transaction_tracking(at);
  return this->value.Get(at);
}

auto single::add(WSTM::WAtomic& at, uint value) -> void {
  setup_transaction_tracking(at);

  auto current = this->value.Get(at);
  current += value;
  this->value.Set(current, at);
}

auto single::sub(WSTM::WAtomic& at, uint value) -> void {
  setup_transaction_tracking(at);

  auto current = this->value.Get(at);

  if (current < value) {
    // TODO: change this to a better exception
    throw std::exception();
  }

  current -= value;
  this->value.Set(current, at);
}
}  // namespace splittable::single
