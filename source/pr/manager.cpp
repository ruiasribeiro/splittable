#include "splittable/pr/manager.hpp"

namespace splittable::pr {

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
            // problems when adding/removing PR from the map itself
            std::shared_lock lock(values_mutex);

            switch (data->status) {
              case txn_status_aborted:
                // std::cerr << "aborted++\n";
                values.at(data->id).aborts++;
                break;
              case txn_status_waiting:
                // std::cerr << "waiting++\n";
                // TODO: check on the paper if this also counts as an abort
                values.at(data->id).waiting++;
                break;
              case txn_status_insufficient_value:
                // std::cerr << "no_stock++\n";
                // TODO: check if this also counts as an abort
                values.at(data->id).aborts_for_no_stock++;
                break;
              case txn_status_completed:
                // std::cerr << "completed++\n";
                values.at(data->id).commits++;
                break;
              default:
                std::cerr << "this shouldn't happen\n";
                // std::unreachable();
                break;
            }
          }
        }
      })
      .detach();

  std::thread(
      // phase worker
      [this]() {
        while (true) {
          std::this_thread::sleep_for(PHASE_INTERVAL);

          // TODO: improve this
          // I don't like this lock here, could affect performance negatively
          std::shared_lock lock(values_mutex);
          for (auto&& [_, md] : values) {
            // std::cout << "a=" << md.aborts << " c=" << md.commits << "\n";
            double abort_rate = 0;

            if (md.commits > 0) {
              abort_rate = (double)md.aborts / (double)(md.aborts + md.commits);
            }

            md.value->try_transition(abort_rate, md.waiting,
                                     md.aborts_for_no_stock);

            // no thread synchonization here but it shouldn't be a problem
            md.aborts = 0;
            md.aborts_for_no_stock = 0;
            md.commits = 0;
            md.waiting = 0;
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

auto manager::register_pr(std::shared_ptr<pr> pr) -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "registering " << pr->get_id() << "\n";
#endif
  std::unique_lock lock(values_mutex);
  values[pr->get_id()] = metadata{.value = pr,
                                  .aborts = 0,
                                  .aborts_for_no_stock = 0,
                                  .commits = 0,
                                  .waiting = 0};
}

auto manager::deregister_pr(std::shared_ptr<pr> pr) -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "deregistering " << pr->get_id() << "\n";
#endif
  std::unique_lock lock(values_mutex);
  values.erase(pr->get_id());
}

auto manager::report_txn(txn_status status, uint pr_id) -> void {
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
  txn_message content{.status = status, .id = pr_id};
  memcpy(message.data(), &content, size);
  client.send(message, zmq::send_flags::none);
}

}  // namespace splittable::pr