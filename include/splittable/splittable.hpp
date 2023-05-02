#pragma once

#include <concepts>
#include <expected>
#include <string>

#include "wstm/stm.h"

namespace splittable {

enum error { insufficient_value };

class splittable {
 public:
  auto virtual read(WSTM::WAtomic& at) -> std::expected<uint, error> = 0;

  auto virtual add(WSTM::WAtomic& at, uint value)
      -> std::expected<void, error> = 0;
      
  auto virtual sub(WSTM::WAtomic& at, uint value)
      -> std::expected<void, error> = 0;
};

}  // namespace splittable
