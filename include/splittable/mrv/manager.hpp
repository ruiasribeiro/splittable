#pragma once

#include <concepts>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "splittable/mrv/mrv.hpp"

namespace splittable::mrv
{

class manager
{
private:
  uint id_counter;

  // {ID -> MRV*}
  std::unordered_map<uint, std::shared_ptr<mrv>> values;
  // exclusive -> when the main manager thread is adding/removing MRVs;
  // shared -> when the secondary workers are iterating through the map
  std::shared_mutex values_mutex;

  // the constructor is private to make this class a singleton
  explicit manager();

public:
  manager(manager const&) = delete;
  manager& operator=(manager const&) = delete;

  ~manager();

  auto static get_instance() -> manager&;

  auto new_mrv() -> std::shared_ptr<mrv>;
  auto delete_mrv(std::shared_ptr<mrv>) -> void;
};

}  // namespace splittable::mrv
