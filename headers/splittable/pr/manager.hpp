#pragma once

#include <chrono>
#include <execution>
#include <immer/map.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "splittable/pr/pr.hpp"

namespace splittable::pr {

class manager {
 private:
  inline static const auto PHASE_INTERVAL = std::chrono::milliseconds(20);

  using values_type = immer::map<uint, std::shared_ptr<pr>>;

  values_type values;
  std::mutex values_mutex;  // needed to prevent data races on the assignment

  std::chrono::nanoseconds total_phase_time;
  uint64_t phase_iterations;
  std::mutex phase_time_mutex;

  std::vector<std::jthread> workers;

  // the constructor is private to make this class a singleton
  manager();
  ~manager();

 public:
  // prevent copy
  manager(manager const&) = delete;
  manager& operator=(manager const&) = delete;

  auto static get_instance() -> manager&;

  auto register_pr(std::shared_ptr<pr> pr) -> void;
  auto deregister_pr(std::shared_ptr<pr> pr) -> void;

  auto get_avg_phase_interval() -> std::chrono::nanoseconds;

  auto reset_global_stats() -> void;
};

}  // namespace splittable::pr