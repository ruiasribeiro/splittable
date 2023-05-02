#pragma once

#include <memory>
#include <numeric>
#include <vector>

#include "splittable/mrv/mrv.hpp"
#include "splittable/utils/random.hpp"
#include "wstm/stm.h"

namespace splittable::mrv {

class mrv_array : public mrv {
 private:
  uint id;
  std::vector<WSTM::WVar<uint>> chunks;

 public:
  auto read(WSTM::WAtomic& at) -> std::expected<uint, error>;
  auto add(WSTM::WAtomic& at, uint value) -> std::expected<void, error>;
  auto sub(WSTM::WAtomic& at, uint value) -> std::expected<void, error>;
};

}  // namespace splittable::mrv