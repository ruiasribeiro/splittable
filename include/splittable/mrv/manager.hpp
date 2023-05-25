#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "splittable/mrv/mrv.hpp"

namespace splittable::mrv {
  
class manager {
 private:
  inline static const auto ADJUST_INTERVAL = std::chrono::milliseconds(1000);
  inline static const auto BALANCE_INTERVAL = std::chrono::milliseconds(100);

  // {ID -> MRV}
  // TODO: I could change this to a concurrent_hash_map fron oneTBB, check if it
  // is worth it
  std::unordered_map<uint, std::shared_ptr<mrv>> values;
  // exclusive -> when the main manager thread is adding/removing MRVs;
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

  auto register_mrv(std::shared_ptr<mrv> mrv) -> void;
  auto deregister_mrv(std::shared_ptr<mrv> mrv) -> void;
};

}  // namespace splittable::mrv
