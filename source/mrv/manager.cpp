#include "splittable/mrv/manager.hpp"

namespace splittable::mrv {

manager::manager() {
  workers.emplace_back(
      // balance worker
      [this](std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
          std::this_thread::sleep_until(std::chrono::steady_clock::now() +
                                        BALANCE_INTERVAL);

          values_type values;
          {
            // the structure is immutable, we only need the lock to fetch it
            std::lock_guard<std::mutex> lock(this->values_mutex);
            values = this->values;
          }

          std::for_each(std::execution::par_unseq, values.begin(), values.end(),
                        [](std::pair<uint, std::shared_ptr<mrv>> pair) {
                          pair.second->balance();
                        });
        }
      });

  workers.emplace_back(
      // adjust worker
      [this](std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
          std::this_thread::sleep_until(std::chrono::steady_clock::now() +
                                        ADJUST_INTERVAL);

          values_type values;
          {
            // the structure is immutable, we only need the lock to fetch it
            std::lock_guard<std::mutex> lock(this->values_mutex);
            values = this->values;
          }

          std::for_each(std::execution::par_unseq, values.begin(), values.end(),
                        [](std::pair<uint, std::shared_ptr<mrv>> pair) {
                          auto counters = pair.second->fetch_and_reset_status();

                          auto commits = counters.commits;
                          auto aborts = counters.aborts;

                          if (commits == 0u) {
                            pair.second->remove_node();
                            return;
                          }

                          auto abort_rate =
                              (double)aborts / (double)(aborts + commits);

                          if (abort_rate < MIN_ABORT_RATE) {
                            pair.second->remove_node();
                          } else if (abort_rate > MAX_ABORT_RATE) {
                            pair.second->add_nodes(abort_rate);
                          }
                        });
        }
      });
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

  values_type values;
  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
    values = this->values;
    values = values.set(mrv->get_id(), mrv);
    this->values = values;
  }
}

auto manager::deregister_mrv(std::shared_ptr<mrv> mrv) -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "deregistering " << mrv->get_id() << "\n";
#endif

  values_type values;
  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
    values = this->values;
    values = values.erase(mrv->get_id());
    this->values = values;
  }
}

}  // namespace splittable::mrv