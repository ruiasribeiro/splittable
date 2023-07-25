#include "splittable/mrv/manager.hpp"

namespace splittable::mrv {

manager::manager() {
  workers.emplace_back(
      // balance worker
      [this](std::stop_token stop_token) {
        auto task = [](std::shared_ptr<mrv> mrv) { mrv->balance(); };

        auto pool = splittable::splittable::get_pool();
        while (pool == nullptr) {
          std::this_thread::sleep_until(std::chrono::steady_clock::now() +
                                        std::chrono::seconds(1));
          pool = splittable::splittable::get_pool();
        }

        while (!stop_token.stop_requested()) {
          std::this_thread::sleep_until(std::chrono::steady_clock::now() +
                                        BALANCE_INTERVAL);

          values_type values;
          {
            // the structure is immutable, we only need the lock to fetch it
            std::lock_guard<std::mutex> lock(this->values_mutex);
            values = this->values;
          }

          for (auto&& [_id, value] : values) {
            pool->push_task(task, value);
          }
        }
      });

  workers.emplace_back(
      // adjust worker
      [this](std::stop_token stop_token) {
        auto task = [](std::shared_ptr<mrv> mrv) {
          auto counters = mrv->fetch_and_reset_status();

          auto commits = counters.commits;
          auto aborts = counters.aborts;

          if (commits == 0u) {
            mrv->remove_node();
            return;
          }

          auto abort_rate = (double)aborts / (double)(aborts + commits);

          if (abort_rate < MIN_ABORT_RATE) {
            mrv->remove_node();
          } else if (abort_rate > MAX_ABORT_RATE) {
            mrv->add_nodes(abort_rate);
          }
        };

        auto pool = splittable::splittable::get_pool();
        while (pool == nullptr) {
          std::this_thread::sleep_until(std::chrono::steady_clock::now() +
                                        std::chrono::seconds(1));
          pool = splittable::splittable::get_pool();
        }

        while (!stop_token.stop_requested()) {
          std::this_thread::sleep_until(std::chrono::steady_clock::now() +
                                        ADJUST_INTERVAL);

          values_type values;
          {
            // the structure is immutable, we only need the lock to fetch it
            std::lock_guard<std::mutex> lock(this->values_mutex);
            values = this->values;
          }

          for (auto&& [_id, value] : values) {
            pool->push_task(task, value);
          }
        }
      });
}

manager::~manager() {}

auto manager::get_instance() -> manager& {
  static manager instance;
  return instance;
}

auto manager::shutdown() -> void {
  workers.clear();  // shuts the workers down upon destruction
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