#pragma once

#include <atomic>
#include <execution>
#include <immer/map.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "splittable/mrv/mrv.hpp"

namespace splittable::mrv {

class manager {
 private:
  inline static const auto ADJUST_INTERVAL = std::chrono::milliseconds(1000);
  inline static const auto BALANCE_INTERVAL = std::chrono::milliseconds(100);

  using values_type = immer::map<uint, std::shared_ptr<mrv>>;

  values_type values;
  std::mutex values_mutex;  // needed to prevent data races on the assignment

  std::chrono::nanoseconds total_adjust_time;
  uint64_t adjust_iterations;
  std::mutex adjust_time_mutex;

  std::chrono::nanoseconds total_balance_time;
  uint64_t balance_iterations;
  std::mutex balance_time_mutex;

  std::vector<std::jthread> workers;

  // the constructor is private to make this class a singleton
  manager();
  ~manager();

 public:
  // prevent copy
  manager(manager const&) = delete;
  manager& operator=(manager const&) = delete;

  auto static get_instance() -> manager&;

  auto register_mrv(std::shared_ptr<mrv> mrv) -> void;
  auto deregister_mrv(std::shared_ptr<mrv> mrv) -> void;

  auto get_avg_adjust_interval() -> std::chrono::nanoseconds;
  auto get_avg_balance_interval() -> std::chrono::nanoseconds;

  auto reset_global_stats() -> void;
};

}  // namespace splittable::mrv
