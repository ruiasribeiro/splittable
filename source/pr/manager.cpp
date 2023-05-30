#include "splittable/pr/manager.hpp"

namespace splittable::pr {

manager::manager() {
  std::thread(
      // phase worker
      [this]() {
        while (true) {
          std::this_thread::sleep_for(PHASE_INTERVAL);

          // TODO: improve this
          // I don't like this lock here, could affect performance negatively
          std::shared_lock lock(values_mutex);
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
  std::unique_lock lock(values_mutex);
  values[pr->get_id()] = pr;
}

auto manager::deregister_pr(std::shared_ptr<pr> pr) -> void {
#ifdef SPLITTABLE_DEBUG
  std::cout << "deregistering " << pr->get_id() << "\n";
#endif
  std::unique_lock lock(values_mutex);
  values.erase(pr->get_id());
}

}  // namespace splittable::pr