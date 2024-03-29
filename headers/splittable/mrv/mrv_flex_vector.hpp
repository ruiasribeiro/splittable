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

using chunk_t = WSTM::WVar<uint>;
using chunks_t = immer::flex_vector<std::shared_ptr<chunk_t>>;

enum class balance_strategy_t { none, random, minmax, all };

class mrv_flex_vector : public mrv,
                        public std::enable_shared_from_this<mrv_flex_vector> {
 private:
  // 16 bits for aborts, 16 bits for commits
  std::atomic_uint32_t status_counters;

  uint id;
  std::atomic<std::shared_ptr<chunks_t>> chunks;
  static std::function<void(WSTM::WAtomic&, chunks_t)> balance_strategy;

 public:
  // TODO: this is not private because of make_shared, need to revise that later
  mrv_flex_vector(uint value);

  auto static new_instance(uint value) -> std::shared_ptr<mrv_flex_vector>;
  auto static delete_instance(std::shared_ptr<mrv_flex_vector>) -> void;

  auto static set_balance_strategy(balance_strategy_t strategy) -> void;

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
};

}  // namespace splittable::mrv