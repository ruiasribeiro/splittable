#pragma once

#include <iostream>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "cppzmq/zmq.hpp"
#include "splittable/pr/pr.hpp"

namespace splittable::pr {

struct metadata {
  std::shared_ptr<pr> value;
  uint aborts;
  uint aborts_for_no_stock;
  uint commits;
  uint waiting;
};

enum txn_status {
  txn_status_aborted,
  txn_status_waiting,
  txn_status_insufficient_value,
  txn_status_completed
};

struct txn_message {
  txn_status status;
  uint id;
};

class manager {
 private:
  inline static const auto PHASE_INTERVAL = std::chrono::milliseconds(20);

  inline static const std::string MESSAGE_URL = "inproc://pr";
  zmq::context_t context;
  zmq::socket_t server;

  // {ID -> Metadata}
  // TODO: I could change this to a concurrent_hash_map fron oneTBB, check if it
  // is worth it
  std::unordered_map<uint, metadata> values;
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

  auto register_pr(std::shared_ptr<pr> pr) -> void;
  auto deregister_pr(std::shared_ptr<pr> pr) -> void;

  auto report_txn(txn_status status, uint pr_id) -> void;
};

}  // namespace splittable::pr