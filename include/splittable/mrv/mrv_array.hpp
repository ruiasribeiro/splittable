#pragma once

#include <oneapi/tbb/concurrent_vector.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

#include "splittable/mrv/manager.hpp"
#include "splittable/mrv/mrv.hpp"
#include "splittable/utils/random.hpp"
#include "wstm/stm.h"

namespace splittable::mrv {

class mrv_array : public mrv {
 private:
  // 16 bits for aborts, 16 bits for commits
  std::atomic_uint32_t status_counters;

  uint id;

  oneapi::tbb::concurrent_vector<WSTM::WVar<uint>> chunks;
  // because of the concurrent vector, we can't remove items. that's why we use
  // a separate counter to determine the number of chunks in use
  WSTM::WVar<size_t> valid_chunks;

 public:
  // TODO: this is not private because of make_shared, need to revise that later
  mrv_array(uint size);

  auto static new_mrv(uint size) -> std::shared_ptr<mrv>;
  auto static delete_mrv(std::shared_ptr<mrv>) -> void;

  auto get_id() -> uint;

  auto add_aborts(uint count) -> void;
  auto add_commits(uint count) -> void;
  auto fetch_and_reset_status() -> status;

  auto read(WSTM::WAtomic& at) -> uint;
  // auto inconsistent_read(WSTM::WInconsistent& inc) -> uint;
  auto add(WSTM::WAtomic& at, uint value) -> void;
  auto sub(WSTM::WAtomic& at, uint value) -> void;

  auto add_nodes(double abort_rate) -> void;
  auto remove_node() -> void;
  auto balance() -> void;
};

}  // namespace splittable::mrv