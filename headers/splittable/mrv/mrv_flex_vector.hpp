#pragma once

#include <wstm/stm.h>

#include <atomic>
#include <immer/algorithm.hpp>
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <queue>
#include <vector>

#include "splittable/mrv/manager.hpp"
#include "splittable/mrv/mrv.hpp"
#include "splittable/utils/random.hpp"

namespace splittable::mrv {

class mrv_flex_vector : public mrv,
                        public std::enable_shared_from_this<mrv_flex_vector> {
 private:
  // 16 bits for aborts, 16 bits for commits
  std::atomic_uint32_t status_counters;

  uint id;

  // this struct allows for explicit alignment of the transactional variables,
  // useful for avoiding false sharing
  struct alignas(std::hardware_destructive_interference_size) chunk_t
      : public WSTM::WVar<uint> {
    // inherit all of the constructors
    using WSTM::WVar<uint>::WVar;
  };

  using chunks_t = immer::flex_vector<std::shared_ptr<chunk_t>>;
  // this pointer is only accessed with atomic instructions
  std::shared_ptr<chunks_t> chunks;
  // this mutex protects adjust operations (add/remove nodes), so that in cases
  // multiple threads try to perform them, they do not create consistency
  // problems
  std::mutex resizing_mutex;

 public:
  // TODO: this is not private because of make_shared, need to revise that later
  mrv_flex_vector(uint value);

  auto static new_instance(uint value) -> std::shared_ptr<mrv_flex_vector>;
  auto static delete_instance(std::shared_ptr<mrv_flex_vector>) -> void;

  auto get_id() -> uint;

  auto static get_avg_adjust_interval() -> std::chrono::nanoseconds;
  auto static get_avg_balance_interval() -> std::chrono::nanoseconds;
  auto static get_avg_phase_interval() -> std::chrono::nanoseconds;
  auto static reset_global_stats() -> void;

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
  auto balance_minmax() -> void;
  auto balance_minmax_with_k() -> void;
  auto balance_random() -> void;
};

}  // namespace splittable::mrv