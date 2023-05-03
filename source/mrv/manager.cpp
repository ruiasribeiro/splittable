#include "splittable/mrv/manager.hpp"

namespace splittable::mrv {

zmqpp::context manager::context{};

manager::manager() : server(context, zmqpp::socket_type::pull) {
  // the ZeroMQ guide states that no threads are needed for inter-thread
  // communications
  context.set(zmqpp::context_option::io_threads, 0);

  server.bind(MESSAGE_URL);

  // TODO: start threads
  std::thread(
      // metadata collector
      [this]() {
        zmqpp::message message;

        while (true) {
          try {
            server.receive(message);
          } catch (...) {
            // this should only happen when the application is shutdown down, so
            // we can safely end this thread
            break;
          }

          int status;
          uint id;
          message >> status >> id;

          {
            // we don't need an exclusive lock here, the commit/abort values are
            // only updated by this thread; the lock is only here to avoid
            // problems when adding/removing MRV
            std::shared_lock lock(values_mutex);

            switch (status) {
              case txn_status::aborted:
                values.at(id).aborts++;
                break;
              case txn_status::completed:
                values.at(id).commits++;
                break;
              default:
                std::unreachable();
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
  values[mrv->get_id()] = metadata{.value = mrv, .aborts = 0, .commits = 0};
};

auto manager::report_txn(txn_status::txn_status status, uint mrv_id) -> void {
  zmqpp::socket client(context, zmqpp::socket_type::push);
  client.connect(MESSAGE_URL);

  zmqpp::message message;
  message << status << mrv_id;
  client.send(message);
}

}  // namespace splittable::mrv