#pragma once

#include <atomic>

#include "splittable/splittable.hpp"

namespace splittable::mrv {

struct status {
  uint64_t aborts;
  uint64_t commits;
};

const double MIN_ABORT_RATE = 0.1;
const double MAX_ABORT_RATE = 0.5;

const uint MAX_NODES = 1024;
const uint MIN_BALANCE_DIFF = 5;

class mrv : public splittable {
 protected:
  static std::atomic_uint id_counter;

 public:
  // auto virtual static new_mrv(uint size) -> std::shared_ptr<mrv> = 0;
  // auto virtual static delete_mrv(std::shared_ptr<mrv>) -> void = 0;

  auto static thread_init() -> void;
  auto static global_init(uint num_threads) -> void;

  auto virtual get_id() -> uint = 0;

  auto virtual add_aborts(uint count) -> void = 0;
  auto virtual add_commits(uint count) -> void = 0;
  auto virtual fetch_and_reset_status() -> status = 0;

  auto virtual add_nodes(double abort_rate) -> void = 0;
  auto virtual remove_node() -> void = 0;
  auto virtual balance() -> void = 0;
};

}  // namespace splittable::mrv
