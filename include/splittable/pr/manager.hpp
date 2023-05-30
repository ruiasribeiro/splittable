#pragma once

#include <iostream>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "splittable/pr/pr.hpp"

namespace splittable::pr {

class manager {
 private:
  inline static const auto PHASE_INTERVAL = std::chrono::milliseconds(20);

  // {ID -> PR}
  // TODO: I could change this to a concurrent_hash_map fron oneTBB, check if it
  // is worth it
  std::unordered_map<uint, std::shared_ptr<pr>> values;
  // exclusive -> when the main manager thread is adding/removing PRs;
  // shared -> when the secondary workers are iterating through the map
  std::shared_mutex values_mutex;

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
};

}  // namespace splittable::pr