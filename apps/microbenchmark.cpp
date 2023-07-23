#include <wstm/stm.h>

#include <atomic>
#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

#include "splittable/mrv/mrv_flex_vector.hpp"
#include "splittable/pr/pr_array.hpp"
#include "splittable/single/single.hpp"
#include "splittable/utils/random.hpp"

using std::chrono::seconds;
using std::chrono::steady_clock;

using namespace std::chrono_literals;

struct result_t {
  uint writes;
  uint reads;
  double abort_rate;
};

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

template <typename S>
result_t run(size_t workers, size_t read_percentage, seconds duration,
             size_t time_padding, size_t num_threads_pool) {
  auto total_reads = std::atomic_uint(0);

  auto value = S::new_instance(0);
  S::global_init(workers, num_threads_pool);

  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      S::thread_init();

      double val{};
      uint val2{};
      size_t reads = 0;

      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        auto does_read =
            splittable::utils::random_index(1, 100) <= read_percentage;

        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(time_padding);

          if (does_read) {
            val2 = value->read(at);
          } else {
            value->add(at, 1);
          }

          val = waste_time(time_padding);
        });

        if (does_read) {
          reads++;
        }
      }

      volatile auto avoid_optimisation __attribute__((unused)) = val;
      volatile auto avoid_optimisation2 __attribute__((unused)) = val2;
      total_reads.fetch_add(reads);
    });
  }

  bar.wait();

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }

  auto writes =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });
  auto stats = splittable::splittable::get_global_stats();
  auto abort_rate =
      static_cast<double>(stats.aborts) / (stats.aborts + stats.commits);

  return {
      .writes = writes, .reads = total_reads.load(), .abort_rate = abort_rate};
}

int main(int argc, char const* argv[]) {
  if (argc < 7) {
    std::cerr << "requires 6 positional arguments: benchmark, number of "
                 "workers, read percentage (%), execution time (s), padding, "
                 "number of pool workers\n";
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

  size_t read_percentage;
  try {
    read_percentage = std::stoul(argv[3]);
  } catch (...) {
    std::cerr << "could not convert \"" << argv[3] << "\" to an integer\n";
    return 1;
  }

  if (read_percentage > 100) {
    std::cerr << "read percentage should be between 0 and 100, got "
              << read_percentage << "\n";
    return 1;
  }

  seconds duration;
  try {
    duration = seconds{std::stoi(argv[4])};
  } catch (...) {
    std::cerr << "could not convert \"" << argv[4] << "\" to an integer\n";
    return 1;
  }

  if (duration.count() < 1) {
    std::cerr << "minimum of 1 second is required\n";
    return 1;
  }

  size_t padding;
  try {
    padding = std::stoul(argv[5]);
  } catch (...) {
    std::cerr << "could not convert \"" << argv[5] << "\" to an integer\n";
    return 1;
  }

  size_t pool_workers;
  try {
    pool_workers = std::stoul(argv[6]);
  } catch (...) {
    std::cerr << "could not convert \"" << argv[6] << "\" to an integer\n";
    return 1;
  }

  std::string benchmark(argv[1]);

  result_t result;
  if (benchmark == "single") {
    using splittable_t = splittable::single::single;
    result = run<splittable_t>(workers, read_percentage, duration, padding,
                               pool_workers);
  } else if (benchmark == "mrv-flex-vector") {
    using splittable_t = splittable::mrv::mrv_flex_vector;
    result = run<splittable_t>(workers, read_percentage, duration, padding,
                               pool_workers);
  } else if (benchmark == "pr-array") {
    using splittable_t = splittable::pr::pr_array;
    result = run<splittable_t>(workers, read_percentage, duration, padding,
                               pool_workers);
  } else {
    std::cerr << "could not find a benchmark with name \"" << benchmark
              << "\"; try\"single\", \"mrv-flex-vector\", \"pr-array\"\n";
    return 1;
  }

  // CSV: benchmark, workers, execution time, read percentage, writes, reads,
  // write throughput (ops/s), read throughput (ops/s), abort rate
  std::cout << benchmark << "," << workers << "," << duration.count() << ","
            << read_percentage << "," << result.writes << "," << result.reads
            << "," << result.writes / duration.count() << ","
            << result.reads / duration.count() << "," << result.abort_rate
            << "\n";

  // return 0;
  quick_exit(0);
}
