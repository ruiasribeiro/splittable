#pragma once

#include "splittable/splittable.hpp"

namespace splittable::pr {

enum split_operation { split_operation_addsub };

class pr : public splittable {
 public:
  auto register_thread() -> void;
  // auto unregister_thread() -> void;
  auto static set_num_threads(uint num) -> void;

  auto try_transisiton(double abort_rate, uint waiting,
                       uint aborts_for_no_stock) -> void;
  auto reconcile() -> void;
  auto split(split_operation op) -> void;
};

}  // namespace splittable::pr
