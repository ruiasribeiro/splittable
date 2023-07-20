#pragma once

#include <atomic>

#include "splittable/splittable.hpp"

namespace splittable::pr {

struct status {
  uint64_t aborts;
  uint64_t aborts_for_no_stock;
  uint64_t commits;
  uint64_t waiting;
};

enum split_operation { split_operation_addsub };

class pr : public splittable {
 protected:
  static std::atomic_uint id_counter;

 public:
  auto virtual get_id() -> uint = 0;

  auto virtual add_aborts(uint count) -> void = 0;
  auto virtual add_aborts_for_no_stock(uint count) -> void = 0;
  auto virtual add_waiting(uint count) -> void = 0;
  auto virtual add_commits(uint count) -> void = 0;
  auto virtual fetch_and_reset_status() -> status = 0;

  auto virtual register_thread() -> void = 0;
  // auto unregister_thread() -> void = 0;
  auto static set_num_threads(uint num) -> void;

  auto virtual try_transition(double abort_rate, uint waiting,
                              uint aborts_for_no_stock) -> void = 0;
  auto virtual split(WSTM::WAtomic& at, split_operation op) -> void = 0;
  auto virtual reconcile(WSTM::WAtomic& at) -> void = 0;
};

}  // namespace splittable::pr
