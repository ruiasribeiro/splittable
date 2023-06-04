#include "splittable/mrv/manager.hpp"

namespace splittable::mrv {

manager::manager() {
  std::thread(
      // balance worker
      [this]() {
        while (true) {
          std::this_thread::sleep_for(BALANCE_INTERVAL);

          values_type values;
          {
            // the structure is immutable, we only need the lock to fetch it
            std::lock_guard<std::mutex> lock(this->values_mutex);
            values = this->values;
          }

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

          values_type values;
          {
            // the structure is immutable, we only need the lock to fetch it
            std::lock_guard<std::mutex> lock(this->values_mutex);
            values = this->values;
          }

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

  values_type values;
  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
    values = this->values;
  }

  values = values.set(mrv->get_id(), mrv);

  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
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
  }

  values = values.erase(mrv->get_id());

  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
    this->values = values;
  }
}

}  // namespace splittable::mrv