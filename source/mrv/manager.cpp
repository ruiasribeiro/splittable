#include "splittable/mrv/manager.hpp"

namespace splittable::mrv {

manager::manager() {
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

}  // namespace splittable::mrv