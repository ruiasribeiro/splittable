#pragma once

#include <concepts>
#include <memory>

#include "splittable/splittable.hpp"

namespace splittable::mrv {

const double MIN_ABORT_RATE = 0.01;
const double MAX_ABORT_RATE = 0.05;

const uint MAX_NODES = 1024;
const uint MIN_BALANCE_DIFF = 5;

class mrv : public splittable
{
public:
  auto virtual add_nodes(double abort_rate) -> void = 0;
  auto virtual remove_node() -> void = 0;
  auto virtual balance() -> void = 0;
};

struct metadata
{
  std::shared_ptr<mrv> value;
  uint aborts;
  uint commits;
};

struct metadata_message
{
  uint id;
  uint retries;
};

} // namespace splittable::mrv
