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

#define TIME_PADDING 100000

#define CACHE_LINE_SIZE 64
#define VALUE_SIZE 8
#define PADDING (CACHE_LINE_SIZE / VALUE_SIZE)

using std::chrono::duration;
using std::chrono::duration_cast;
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

result_t bm_single(size_t workers, size_t read_percentage, seconds duration) {
  auto total_reads = std::atomic_uint(0);
  auto value = splittable::single::single::new_instance(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
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
          val = waste_time(TIME_PADDING);

          if (does_read) {
            val2 = value->read(at);
          } else {
            value->add(at, 1);
          }

          val = waste_time(TIME_PADDING);
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

result_t bm_mrv_flex_vector(size_t workers, size_t read_percentage,
                            seconds duration) {
  auto total_reads = std::atomic_uint(0);
  auto value = splittable::mrv::mrv_flex_vector::new_instance(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
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
          val = waste_time(TIME_PADDING);

          if (does_read) {
            val2 = value->read(at);
          } else {
            value->add(at, 1);
          }

          val = waste_time(TIME_PADDING);
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

result_t bm_pr_array(size_t workers, size_t read_percentage, seconds duration) {
  splittable::pr::pr_array::set_num_threads(workers);
  auto total_reads = std::atomic_uint(0);
  auto value = splittable::pr::pr_array::new_instance(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      value->register_thread();

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
          val = waste_time(TIME_PADDING);

          if (does_read) {
            val2 = value->read(at);
          } else {
            value->add(at, 1);
          }

          val = waste_time(TIME_PADDING);
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
  if (argc < 5) {
    std::cerr << "requires 4 positional arguments: benchmark, number of "
                 "workers, read percentage (%), execution time (s)\n";
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

  seconds execution_time;
  try {
    execution_time = seconds{std::stoi(argv[4])};
  } catch (...) {
    std::cerr << "could not convert \"" << argv[4] << "\" to an integer\n";
    return 1;
  }

  if (execution_time.count() < 1) {
    std::cerr << "minimum of 1 second is required\n";
    return 1;
  }

  std::string benchmark(argv[1]);

  result_t result;
  if (benchmark == "single") {
    result = bm_single(workers, read_percentage, execution_time);
  } else if (benchmark == "mrv-flex-vector") {
    result = bm_mrv_flex_vector(workers, read_percentage, execution_time);
  } else if (benchmark == "pr-array") {
    result = bm_pr_array(workers, read_percentage, execution_time);
  } else {
    std::cerr << "could not find a benchmark with name \"" << benchmark
              << "\"; try\"single\", \"mrv-flex-vector\", \"pr-array\"\n";
    return 1;
  }

  // CSV: benchmark, workers, execution time, read percentage, writes, reads,
  // write throughput (ops/s), read throughput (ops/s), abort rate
  std::cout << benchmark << "," << workers << "," << execution_time.count()
            << "," << read_percentage << "," << result.writes << ","
            << result.reads << "," << result.writes / execution_time.count()
            << "," << result.reads / execution_time.count() << ","
            << result.abort_rate << "\n";

  // return 0;
  quick_exit(0);
}
