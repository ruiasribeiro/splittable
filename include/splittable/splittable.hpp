#pragma once

#include <concepts>
#include <exception>
#include <string>

#include "wstm/stm.h"

namespace splittable {

enum class error { insufficient_value };

struct exception : public std::exception {};

class splittable {
 public:
  auto virtual read(WSTM::WAtomic& at) -> uint = 0;
  auto virtual add(WSTM::WAtomic& at, uint value) -> void = 0;
  auto virtual sub(WSTM::WAtomic& at, uint value) -> void = 0;
};

}  // namespace splittable
