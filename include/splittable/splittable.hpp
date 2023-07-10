#pragma once

#include <exception>
#include <string>

#include "wstm/stm.h"

namespace splittable {

enum class error { insufficient_value, other };

struct exception : public std::exception {
  error err;

  exception() : err(error::other) {}
  exception(error err) : err(err) {}

  char* what() {
    switch (err) {
      case error::insufficient_value:
        return "could not perform the subtraction";
      case error::other:
      default:
        return "unknown error";
    }
  }
};

class splittable {
 public:
  auto virtual read(WSTM::WAtomic& at) -> uint = 0;
  // auto virtual inconsistent_read(WSTM::WInconsistent& inc) -> uint = 0;
  auto virtual add(WSTM::WAtomic& at, uint value) -> void = 0;
  auto virtual sub(WSTM::WAtomic& at, uint value) -> void = 0;
};

}  // namespace splittable
