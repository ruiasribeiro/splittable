#pragma once

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <variant>

#include "splittable/pr/manager.hpp"
#include "splittable/pr/pr.hpp"
#include "wstm/stm.h"

namespace splittable::pr {

class pr_array : public pr {
 private:
  struct splitted {
    // each of the values needs to be a WVar, I think, to allow transactions to
    // rollback if needed. if they're not WVars, even if each thread only
    // accesses its chunk, there could be some data inconsistency
    std::vector<WSTM::WVar<uint>> chunks;
    split_operation op;
  };

  using Single = uint;
  using Splitted = splitted;

  // 16 bits for each of the counters: aborts, aborts_for_no_stock, commits,
  // waiting
  std::atomic_uint64_t status_counters;

  uint id;

  // I would like to use a std::variant here but it doesn't seem to play nicely
  // with WyattSTM
  WSTM::WVar<Single> single_value;
  WSTM::WVar<std::shared_ptr<Splitted>> splitted_value;

  WSTM::WVar<bool> is_splitted;

  static std::atomic_uint thread_id_counter;
  static thread_local uint thread_id;
  static uint num_threads;

 public:
  // TODO: this is not private because of make_shared, need to revise that later
  pr_array(uint value);

  auto static new_instance(uint value) -> std::shared_ptr<pr_array>;
  auto static delete_instance(std::shared_ptr<pr_array>) -> void;

  auto get_id() -> uint;

  auto add_aborts(uint count) -> void;
  auto add_aborts_for_no_stock(uint count) -> void;
  auto add_waiting(uint count) -> void;
  auto add_commits(uint count) -> void;
  auto fetch_and_reset_status() -> status;

  auto read(WSTM::WAtomic& at) -> uint;
  // auto inconsistent_read(WSTM::WInconsistent& inc) -> uint;
  auto add(WSTM::WAtomic& at, uint value) -> void;
  auto sub(WSTM::WAtomic& at, uint value) -> void;

  auto register_thread() -> void;
  // auto unregister_thread() -> void;
  auto static set_num_threads(uint num) -> void;

  auto try_transition(double abort_rate, uint waiting, uint aborts_for_no_stock)
      -> void;
  auto split(WSTM::WAtomic& at, split_operation op) -> void;
  auto reconcile(WSTM::WAtomic& at) -> void;
};

}  // namespace splittable::pr