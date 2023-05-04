#pragma once

#include <concepts>
#include <iostream>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <zmqpp/zmqpp.hpp>

#include "splittable/mrv/mrv.hpp"

namespace splittable::mrv {

struct metadata {
  std::shared_ptr<mrv> value;
  uint aborts;
  uint commits;
};

namespace txn_status {
enum txn_status { aborted, completed };
}

class manager {
 private:
  inline static const auto ADJUST_INTERVAL = std::chrono::milliseconds(1000);
  inline static const auto BALANCE_INTERVAL = std::chrono::milliseconds(100);

  inline static const std::string MESSAGE_URL = "inproc://mrv";
  static zmqpp::context context;
  zmqpp::socket server;

  // {ID -> Metadata}
  std::unordered_map<uint, metadata> values;
  // exclusive -> when the main manager thread is adding/removing MRVs;
  // shared -> when the secondary workers are iterating through the map
  std::shared_mutex values_mutex;

  // the constructor/destructor are private to make this class a singleton
  manager();
  ~manager();

 public:
  // prevent copy
  manager(manager const&) = delete;
  manager& operator=(manager const&) = delete;

  auto static get_instance() -> manager&;

  auto register_mrv(std::shared_ptr<mrv> mrv) -> void;
  auto deregister_mrv(std::shared_ptr<mrv> mrv) -> void;

  auto report_txn(txn_status::txn_status status, uint mrv_id) -> void;
};

}  // namespace splittable::mrv
