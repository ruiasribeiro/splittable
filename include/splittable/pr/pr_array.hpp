#pragma once

#include <array>
#include <variant>

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

  uint id;
  WSTM::WVar<std::variant<Single, Splitted>> value;

  static thread_local uint thread_id;
  static uint num_threads;

 public:
  auto read(WSTM::WAtomic& at) -> uint;
  // auto inconsistent_read(WSTM::WInconsistent& inc) -> uint;
  auto add(WSTM::WAtomic& at, uint value) -> void;
  auto sub(WSTM::WAtomic& at, uint value) -> void;

  auto register_thread() -> void;
  // auto unregister_thread() -> void;
  auto static set_num_threads(uint num) -> void;

  auto try_transisiton(double abort_rate, uint waiting,
                       uint aborts_for_no_stock) -> void;
  auto reconcile() -> void;
  auto split(split_operation op) -> void;
};

}  // namespace splittable::pr