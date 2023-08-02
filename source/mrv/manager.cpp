#include "splittable/mrv/manager.hpp"

namespace splittable::mrv {

manager::manager()
    : total_adjust_time(0),
      adjust_iterations(0),
      total_balance_time(0),
      balance_iterations(0) {
  // workers.emplace_back(
  //     // balance worker
  //     [this](std::stop_token stop_token) {
  //       while (!stop_token.stop_requested()) {
  //         std::this_thread::sleep_until(std::chrono::steady_clock::now() +
  //                                       BALANCE_INTERVAL);

  //         values_type values;
  //         {
  //           // the structure is immutable, we only need the lock to fetch it
  //           std::lock_guard<std::mutex> lock(this->values_mutex);
  //           values = this->values;
  //         }

  //         auto start = std::chrono::steady_clock::now();

  //         std::for_each(std::execution::par_unseq, values.begin(),
  //         values.end(),
  //                       [](std::pair<uint, std::shared_ptr<mrv>> pair) {
  //                         pair.second->balance();
  //                       });

  //         auto end = std::chrono::steady_clock::now();

  //         {
  //           std::lock_guard<std::mutex> lock(this->balance_time_mutex);

  //           this->total_balance_time += end - start;
  //           ++this->balance_iterations;
  //         }
  //       }
  //     });

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

          auto start = std::chrono::steady_clock::now();

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

          auto end = std::chrono::steady_clock::now();

          {
            std::lock_guard<std::mutex> lock(this->adjust_time_mutex);

            this->total_adjust_time += end - start;
            ++this->adjust_iterations;
          }
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

auto manager::get_avg_adjust_interval() -> std::chrono::nanoseconds {
  {
    std::lock_guard<std::mutex> lock(this->adjust_time_mutex);

    if (adjust_iterations > 0) {
      return this->total_adjust_time / adjust_iterations;
    }

    return std::chrono::nanoseconds(0);
  }
}

auto manager::get_avg_balance_interval() -> std::chrono::nanoseconds {
  {
    std::lock_guard<std::mutex> lock(this->balance_time_mutex);

    if (balance_iterations > 0) {
      return this->total_balance_time / balance_iterations;
    }

    return std::chrono::nanoseconds(0);
  }
}

auto manager::reset_global_stats() -> void {
  {
    std::scoped_lock lock(this->adjust_time_mutex, this->balance_time_mutex);

    this->total_adjust_time = std::chrono::nanoseconds{0};
    this->adjust_iterations = 0;

    this->total_balance_time = std::chrono::nanoseconds{0};
    this->balance_iterations = 0;
  }
}

}  // namespace splittable::mrv