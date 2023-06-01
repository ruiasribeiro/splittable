#include "splittable/pr/manager.hpp"

namespace splittable::pr {

manager::manager() {
  std::thread(
      // phase worker
      [this]() {
        while (true) {
          std::this_thread::sleep_for(PHASE_INTERVAL);

          values_type values;
          {
            // the structure is immutable, we only need the lock to fetch it
            std::lock_guard<std::mutex> lock(this->values_mutex);
            values = this->values;
          }

          for (auto&& [_, value] : values) {
            auto counters = value->fetch_and_reset_status();

            double abort_rate = 0;
            if (counters.commits > 0) {
              abort_rate = (double)counters.aborts /
                           (double)(counters.aborts + counters.commits);
            }

            value->try_transition(abort_rate, counters.waiting,
                                  counters.aborts_for_no_stock);
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

  values_type values;
  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
    values = this->values;
  }

  values = values.set(pr->get_id(), pr);

  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
    this->values = values;
  }
}

auto manager::deregister_pr(std::shared_ptr<pr> pr) -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "deregistering " << pr->get_id() << "\n";
#endif

  values_type values;
  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
    values = this->values;
  }

  values = values.erase(pr->get_id());

  {
    std::lock_guard<std::mutex> lock(this->values_mutex);
    this->values = values;
  }
}

}  // namespace splittable::pr