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
  static std::atomic_uint id_counter;

  uint id;
  // std::vector<WSTM::WVar<uint>> chunks;

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

  auto read(WSTM::WAtomic& at) -> uint;
  auto add(WSTM::WAtomic& at, uint value) -> void;
  auto sub(WSTM::WAtomic& at, uint value) -> void;

  auto add_nodes(double abort_rate) -> void;
  auto remove_node() -> void;
  auto balance() -> void;
};

}  // namespace splittable::mrv