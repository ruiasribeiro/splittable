#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "cppzmq/zmq.hpp"
#include "splittable/mrv/mrv.hpp"

namespace splittable::mrv {

enum txn_status { txn_status_aborted, txn_status_completed };

struct txn_message {
  txn_status status;
  uint id;
};

class manager {
 private:
  inline static const auto ADJUST_INTERVAL = std::chrono::milliseconds(1000);
  inline static const auto BALANCE_INTERVAL = std::chrono::milliseconds(100);

  inline static const std::string MESSAGE_URL = "inproc://mrv";
  zmq::context_t context;
  zmq::socket_t server;

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

  auto report_txn(txn_status status, uint mrv_id) -> void;
};

}  // namespace splittable::mrv
