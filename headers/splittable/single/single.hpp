#pragma once

#include <wstm/stm.h>

#include "splittable/splittable.hpp"

namespace splittable::single {

class single : public splittable {
 private:
  WSTM::WVar<uint> value;

 public:
  // TODO: this is not private because of make_shared, need to revise that later
  single(uint value);

  auto static thread_init() -> void;
  auto static global_init(uint num_threads) -> void;

  auto static new_instance(uint value) -> std::shared_ptr<single>;
  auto static delete_instance(std::shared_ptr<single>) -> void;

  auto static get_avg_adjust_interval() -> std::chrono::nanoseconds;
  auto static get_avg_balance_interval() -> std::chrono::nanoseconds;
  auto static get_avg_phase_interval() -> std::chrono::nanoseconds;
  auto static reset_global_stats() -> void;

  auto read(WSTM::WAtomic& at) -> uint;
  // auto inconsistent_read(WSTM::WInconsistent& inc) -> uint;
  auto add(WSTM::WAtomic& at, uint value) -> void;
  auto sub(WSTM::WAtomic& at, uint value) -> void;
};

}  // namespace splittable::single