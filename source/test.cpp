#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

#include "splittable/mrv/mrv_array.hpp"
#include "wstm/stm.h"

#define TOTAL_ITERATIONS 10000
#define TIME_PADDING 100000

#define CACHE_LINE_SIZE 64
#define VALUE_SIZE 8
#define PADDING (CACHE_LINE_SIZE / VALUE_SIZE)

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::microseconds;
using std::chrono::system_clock;

using namespace std::chrono_literals;

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

long long bm_single(size_t workers) {
  auto value = WSTM::WVar<uint>(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  const auto workload = TOTAL_ITERATIONS / workers;

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, workload]() {
      double val;
      bar.wait();

      for (auto j = 0ul; j < workload; j++) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(TIME_PADDING);

          auto current_value = value.Get(at);
          value.Set(current_value + 1, at);

          val = waste_time(TIME_PADDING);
        });
      }

      volatile auto avoid_optimisation = val;
    });
  }

  bar.wait();

  auto start = high_resolution_clock::now();
  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }
  auto end = high_resolution_clock::now();

  auto result = value.GetReadOnly();
  auto expected = TOTAL_ITERATIONS - (TOTAL_ITERATIONS % workers);

  if (result != expected) {
    std::cerr << "ERROR: invalid result! got " << result << ", expected "
              << expected << "\n";
  }

  return duration_cast<microseconds>(end - start).count();
}

long long bm_mrv_array(size_t workers) {
  auto value = splittable::mrv::mrv_array::new_mrv(1);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  const auto workload = TOTAL_ITERATIONS / workers;

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, workload]() {
      double val;
      bar.wait();

      for (auto j = 0ul; j < workload; j++) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(TIME_PADDING);
          value->add(at, 1);
          val = waste_time(TIME_PADDING);
        });
      }

      volatile auto avoid_optimisation = val;
    });
  }

  bar.wait();

  auto start = high_resolution_clock::now();
  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }
  auto end = high_resolution_clock::now();

  auto result =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });
  auto expected = TOTAL_ITERATIONS - (TOTAL_ITERATIONS % workers);

  if (result != expected) {
    std::cerr << "ERROR: invalid result! got " << result << ", expected "
              << expected << "\n";
  }

  return duration_cast<microseconds>(end - start).count();
}

int main(int argc, char const* argv[]) {
  if (argc < 3) {
    std::cerr << "requires 2 positional arguments: benchmark, number of workers"
              << std::endl;
    return 1;
  }

  int workers;
  try {
    workers = std::stoi(argv[2]);
  } catch (...) {
    std::cerr << "could not convert \"" << argv[2] << "\" to an integer"
              << std::endl;
    return 1;
  }

  if (workers < 1) {
    std::cerr << "minimum of 1 worker is required" << std::endl;
    return 1;
  }

  std::string benchmark(argv[1]);

  long long time;
  if (benchmark == "single") {
    time = bm_single(workers);
  } else if (benchmark == "mrv-array") {
    time = bm_mrv_array(workers);
  } else {
    std::cerr << "could not find a benchmark with name \"" << benchmark
              << "\"; try\"single\", \"mrv-array\"" << std::endl;
    return 1;
  }

  // CSV header: benchmark, workers, elapsed time (us)
  std::cout << benchmark << "," << workers << "," << time << std::endl;

  // return 0;
  quick_exit(0);
}
