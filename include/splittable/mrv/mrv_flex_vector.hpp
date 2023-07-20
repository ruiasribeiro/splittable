#pragma once

#include <atomic>
#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

#include "immer/algorithm.hpp"
#include "immer/flex_vector.hpp"
#include "immer/flex_vector_transient.hpp"
#include "splittable/mrv/manager.hpp"
#include "splittable/mrv/mrv.hpp"
#include "splittable/utils/random.hpp"
#include "wstm/stm.h"

namespace splittable::mrv {

class mrv_flex_vector : public mrv, public std::enable_shared_from_this<mrv_flex_vector> {
 private:
  // 16 bits for aborts, 16 bits for commits
  std::atomic_uint32_t status_counters;

  uint id;

  using chunks_t = immer::flex_vector<std::shared_ptr<WSTM::WVar<uint>>>;
  // this pointer is only accessed with atomic instructions
  std::shared_ptr<chunks_t> chunks;

 public:
  // TODO: this is not private because of make_shared, need to revise that later
  mrv_flex_vector(uint value);

  auto static new_instance(uint value) -> std::shared_ptr<mrv_flex_vector>;
  auto static delete_instance(std::shared_ptr<mrv_flex_vector>) -> void;

  auto get_id() -> uint;

  auto add_aborts(uint count) -> void;
  auto add_commits(uint count) -> void;
  auto fetch_and_reset_status() -> status;
  auto fetch_total_status() -> status;

  auto read(WSTM::WAtomic& at) -> uint;
  // auto inconsistent_read(WSTM::WInconsistent& inc) -> uint;
  auto add(WSTM::WAtomic& at, uint value) -> void;
  auto sub(WSTM::WAtomic& at, uint value) -> void;

  auto add_nodes(double abort_rate) -> void;
  auto remove_node() -> void;
  auto balance() -> void;
};

}  // namespace splittable::mrv