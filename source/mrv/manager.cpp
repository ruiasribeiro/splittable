#include "splittable/mrv/manager.hpp"

namespace splittable::mrv {

manager::manager() : context() {
  // the ZeroMQ guide states that no threads are needed for inter-thread
  // communications
  context.set(zmq::ctxopt::io_threads, 0);

  server = zmq::socket_t(context, zmq::socket_type::pull);
  server.bind(MESSAGE_URL);

  std::thread(
      // metadata collector
      [this]() {
        zmq::message_t message;
        zmq::recv_result_t recv_result;

        while (true) {
          try {
            recv_result = server.recv(message);
          } catch (...) {
            // this should only happen when the application is shutdown, so
            // we can safely end this thread
            break;
          }

          if (!recv_result) {
            break;
          }

          auto data = message.data<txn_message>();

          {
            // we don't need an exclusive lock here, the commit/abort values are
            // only updated by this thread; the lock is only here to avoid
            // problems when adding/removing MRV from the map itself
            std::shared_lock lock(values_mutex);

            switch (data->status) {
              case txn_status_aborted:
                values[data->id]->add_aborts(1u);
                break;
              case txn_status_completed:
                values[data->id]->add_commits(1u);
                break;
              default:
                // std::unreachable();
                break;
            }
          }
        }
      })
      .detach();

  std::thread(
      // balance worker
      [this]() {
        while (true) {
          std::this_thread::sleep_for(BALANCE_INTERVAL);

          // TODO: improve this
          // I don't like this lock here, could affect performance negatively
          std::shared_lock lock(values_mutex);
          for (auto&& [_, value] : values) {
            value->balance();
          }
        }
      })
      .detach();

  std::thread(
      // adjust worker
      [this]() {
        while (true) {
          std::this_thread::sleep_for(ADJUST_INTERVAL);

          // TODO: improve this
          // I don't like this lock here, could affect performance negatively
          std::shared_lock lock(values_mutex);
          for (auto&& [id, value] : values) {
            auto counters = value->fetch_and_reset_status();

            auto commits = counters.commits;
            auto aborts = counters.aborts;

            if (commits == 0u) {
              value->remove_node();
              continue;
            }

            auto abort_rate = (double)aborts / (double)(aborts + commits);

            if (abort_rate < MIN_ABORT_RATE) {
              value->remove_node();
            } else if (abort_rate > MAX_ABORT_RATE) {
              value->add_nodes(abort_rate);
            }
          }
        }
      })
      .detach();
}

manager::~manager() {}

auto manager::get_instance() -> manager& {
  static manager instance;
  return instance;
}

auto manager::register_mrv(std::shared_ptr<mrv> mrv) -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "registering " << mrv->get_id() << "\n";
#endif
  std::unique_lock lock(values_mutex);
  values[mrv->get_id()] = mrv;
}

auto manager::deregister_mrv(std::shared_ptr<mrv> mrv) -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "deregistering " << mrv->get_id() << "\n";
#endif
  std::unique_lock lock(values_mutex);
  values.erase(mrv->get_id());
}

auto manager::report_txn(txn_status status, uint mrv_id) -> void {
  // needs to be static to be reused, so that the OS does not complain about
  // "too many open files"
  static thread_local zmq::socket_t client(context, zmq::socket_type::push);

  static thread_local bool is_client_connected = false;
  if (!is_client_connected) {
    client.connect(MESSAGE_URL);
    is_client_connected = true;
  }

  auto size = sizeof(txn_message);
  zmq::message_t message(size);
  txn_message content{.status = status, .id = mrv_id};
  memcpy(message.data(), &content, size);
  client.send(message, zmq::send_flags::none);
}

}  // namespace splittable::mrv