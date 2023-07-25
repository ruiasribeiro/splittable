#pragma once

#include <wstm/stm.h>

#include <BS_thread_pool.hpp>
#include <atomic>
#include <exception>
#include <memory>
#include <string>

namespace splittable {

enum class error { insufficient_value, other };

struct exception : public std::exception {
  error err;

  exception() : err(error::other) {}
  exception(error err) : err(err) {}

  // char* what() {
  //   switch (err) {
  //     case error::insufficient_value:
  //       return "could not perform the subtraction";
  //     case error::other:
  //     default:
  //       return "unknown error";
  //   }
  // }
};

struct status {
  uint64_t aborts;
  uint64_t commits;
};

class splittable {
 private:
  static std::atomic_uint64_t total_aborts;
  static std::atomic_uint64_t total_commits;

  static std::shared_ptr<BS::thread_pool> pool;

 protected:

  auto static setup_transaction_tracking(WSTM::WAtomic& at) -> void;

 public:
  auto static initialise_pool(uint count) -> void;
  auto static get_pool() -> std::shared_ptr<BS::thread_pool>;
  
  auto static reset_global_stats() -> void;
  auto static get_global_stats() -> status;

  auto virtual read(WSTM::WAtomic& at) -> uint = 0;
  // auto virtual inconsistent_read(WSTM::WInconsistent& inc) -> uint = 0;
  auto virtual add(WSTM::WAtomic& at, uint value) -> void = 0;
  auto virtual sub(WSTM::WAtomic& at, uint value) -> void = 0;
};

}  // namespace splittable
