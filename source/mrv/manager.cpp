#include "splittable/mrv/manager.hpp"

namespace splittable::mrv {

zmq::context_t manager::context{};

manager::manager() : server(context, zmq::socket_type::pull) {
  // the ZeroMQ guide states that no threads are needed for inter-thread
  // communications
  context.set(zmq::ctxopt::io_threads, 0);

  server.bind(MESSAGE_URL);

  std::thread(
      // metadata collector
      [this]() {
        zmq::message_t message;

        while (true) {
          try {
            server.recv(message);
          } catch (...) {
            // this should only happen when the application is shutdown down, so
            // we can safely end this thread
            break;
          }

          auto data = message.data<txn_message>();

          {
            // we don't need an exclusive lock here, the commit/abort values are
            // only updated by this thread; the lock is only here to avoid
            // problems when adding/removing MRV
            std::shared_lock lock(values_mutex);

            switch (data->status) {
              case txn_status::aborted:
                values.at(data->id).aborts++;
                break;
              case txn_status::completed:
                values.at(data->id).commits++;
                break;
              default:
                std::unreachable();
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
            value.value->balance();
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
          for (auto&& [id, metadata] : values) {
            if (metadata.commits == 0) {
              metadata.value->remove_node();
              continue;
            }

            auto abort_rate = (double)metadata.aborts /
                              (double)(metadata.aborts + metadata.commits);

            // no thread synchonization here but it shouldn't be a problem
            metadata.aborts = 0;
            metadata.commits = 0;

            if (abort_rate < MIN_ABORT_RATE) {
              metadata.value->remove_node();
            } else if (abort_rate > MAX_ABORT_RATE) {
              metadata.value->add_nodes(abort_rate);
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
  std::cout << "registering " << mrv->get_id() << "\n";
  std::unique_lock lock(values_mutex);
  values[mrv->get_id()] = metadata{.value = mrv, .aborts = 0, .commits = 0};
};

auto manager::deregister_mrv(std::shared_ptr<mrv> mrv) -> void {
  std::cout << "deregistering " << mrv->get_id() << "\n";
  std::unique_lock lock(values_mutex);
  values.erase(mrv->get_id());
};

auto manager::report_txn(txn_status::txn_status status, uint mrv_id) -> void {
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