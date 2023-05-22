#pragma once

#include "splittable/splittable.hpp"

namespace splittable::pr {

enum split_operation { split_operation_addsub };

class pr : public splittable {
 public:
  auto virtual register_thread() -> void = 0;
  // auto unregister_thread() -> void = 0;
  auto static set_num_threads(uint num) -> void;

  auto virtual try_transisiton(double abort_rate, uint waiting,
                       uint aborts_for_no_stock) -> void = 0;
  auto virtual split(WSTM::WAtomic& at, split_operation op) -> void = 0;
  auto virtual reconcile(WSTM::WAtomic& at) -> void = 0;
};

}  // namespace splittable::pr
