#include <wstm/stm.h>

#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

#include "splittable/mrv/mrv_flex_vector.hpp"
#include "splittable/pr/pr_array.hpp"
#include "splittable/single/single.hpp"

#define CACHE_LINE_SIZE 64
#define VALUE_SIZE 8
#define PADDING (CACHE_LINE_SIZE / VALUE_SIZE)

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::steady_clock;

using namespace std::chrono_literals;

struct result_t {
  uint operations;
  double abort_rate;
};

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

result_t bm_single(size_t workers, seconds duration, int time_padding) {
  auto value = splittable::single::single::new_instance(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      double val;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(time_padding);
          value->add(at, 1);
          val = waste_time(time_padding);
        });
      }

      volatile auto avoid_optimisation = val;
    });
  }

  bar.wait();

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }

  auto operations =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });
  auto stats = splittable::splittable::get_global_stats();
  auto abort_rate =
      static_cast<double>(stats.aborts) / (stats.aborts + stats.commits);

  return {.operations = operations, .abort_rate = abort_rate};
}

result_t bm_mrv_flex_vector(size_t workers, seconds duration,
                            int time_padding) {
  auto value = splittable::mrv::mrv_flex_vector::new_instance(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      double val;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(time_padding);
          value->add(at, 1);
          val = waste_time(time_padding);
        });
      }

      volatile auto avoid_optimisation = val;
    });
  }

  bar.wait();

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }

  auto operations =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });
  auto stats = splittable::splittable::get_global_stats();
  auto abort_rate =
      static_cast<double>(stats.aborts) / (stats.aborts + stats.commits);

  return {.operations = operations, .abort_rate = abort_rate};
}

result_t bm_pr_array(size_t workers, seconds duration, int time_padding) {
  splittable::pr::pr_array::set_num_threads(workers);
  auto value = splittable::pr::pr_array::new_instance(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      value->register_thread();

      double val;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(time_padding);
          value->add(at, 1);
          val = waste_time(time_padding);
        });
      }

      volatile auto avoid_optimisation = val;
    });
  }

  bar.wait();

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }

  auto operations =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });
  auto stats = splittable::splittable::get_global_stats();
  auto abort_rate =
      static_cast<double>(stats.aborts) / (stats.aborts + stats.commits);

  return {.operations = operations, .abort_rate = abort_rate};
}

int main(int argc, char const* argv[]) {
  if (argc < 5) {
    std::cerr << "requires 4 positional arguments: benchmark, number of "
                 "workers, execution time (s), padding\n";
    return 1;
  }

  int workers;
  try {
    workers = std::stoi(argv[2]);
  } catch (...) {
    std::cerr << "could not convert \"" << argv[2] << "\" to an integer\n";
    return 1;
  }

  if (workers < 1) {
    std::cerr << "minimum of 1 worker is required\n";
    return 1;
  }

  seconds execution_time;
  try {
    execution_time = seconds{std::stoi(argv[3])};
  } catch (...) {
    std::cerr << "could not convert \"" << argv[3] << "\" to an integer\n";
    return 1;
  }

  if (execution_time.count() < 1) {
    std::cerr << "minimum of 1 second is required\n";
    return 1;
  }

  int padding;
  try {
    padding = std::stoi(argv[4]);
  } catch (...) {
    std::cerr << "could not convert \"" << argv[4] << "\" to an integer\n";
    return 1;
  }

  if (padding < 0) {
    std::cerr << "the padding must be a non-negative integer\n";
    return 1;
  }

  std::string benchmark(argv[1]);

  result_t result;
  if (benchmark == "single") {
    result = bm_single(workers, execution_time, padding);
  } else if (benchmark == "mrv-flex-vector") {
    result = bm_mrv_flex_vector(workers, execution_time, padding);
  } else if (benchmark == "pr-array") {
    result = bm_pr_array(workers, execution_time, padding);
  } else {
    std::cerr << "could not find a benchmark with name \"" << benchmark
              << "\"; try \"single\", \"mrv-flex-vector\", \"pr-array\"\n";
    return 1;
  }

  // CSV header: benchmark, workers, execution time, padding, # of commited
  // operations, throughput (ops/s), abort rate
  std::cout << benchmark << "," << workers << "," << execution_time.count()
            << "," << padding << "," << result.operations << ","
            << result.operations / execution_time.count() << ","
            << result.abort_rate << "\n";

  // return 0;
  quick_exit(0);
}
