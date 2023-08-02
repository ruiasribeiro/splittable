#include "splittable/pr/manager.hpp"

namespace splittable::pr {

manager::manager() : total_phase_time(0), phase_iterations(0) {
  workers.emplace_back(
      // phase worker
      [this](std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
          std::this_thread::sleep_for(PHASE_INTERVAL);

          values_type values;
          {
            // the structure is immutable, we only need the lock to fetch it
            std::lock_guard<std::mutex> lock(this->values_mutex);
            values = this->values;
          }

          auto start = std::chrono::steady_clock::now();

          std::for_each(
              std::execution::par_unseq, values.begin(), values.end(),
              [](std::pair<uint, std::shared_ptr<pr>> pair) {
                auto counters = pair.second->fetch_and_reset_status();

                double abort_rate = 0;
                if (counters.commits > 0) {
                  abort_rate = (double)counters.aborts /
                               (double)(counters.aborts + counters.commits);
                }

                pair.second->try_transition(abort_rate, counters.waiting,
                                            counters.aborts_for_no_stock);
              });

          auto end = std::chrono::steady_clock::now();

          {
            std::lock_guard<std::mutex> lock(this->phase_time_mutex);

            this->total_phase_time += end - start;
            ++this->phase_iterations;
          }
        }
      });
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
    values = values.set(pr->get_id(), pr);
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
    values = values.erase(pr->get_id());
    this->values = values;
  }
}

auto manager::get_avg_phase_interval() -> std::chrono::nanoseconds {
  {
    std::lock_guard<std::mutex> lock(this->phase_time_mutex);

    if (phase_iterations > 0) {
      return this->total_phase_time / phase_iterations;
    }
    
    return std::chrono::nanoseconds(0);
  }
}

auto manager::reset_global_stats() -> void {
  {
    std::scoped_lock lock(this->phase_time_mutex);

    this->total_phase_time = std::chrono::nanoseconds{0};
    this->phase_iterations = 0;
  }
}

}  // namespace splittable::pr