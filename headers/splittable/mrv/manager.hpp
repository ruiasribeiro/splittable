#pragma once

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
