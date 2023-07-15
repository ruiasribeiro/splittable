#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

#include "splittable/mrv/mrv_flex_vector.hpp"
#include "splittable/pr/pr_array.hpp"
#include "wstm/stm.h"

#define TOTAL_ITERATIONS 100000
#define TIME_PADDING 100000

#define CACHE_LINE_SIZE 64
#define VALUE_SIZE 8
#define PADDING (CACHE_LINE_SIZE / VALUE_SIZE)

using namespace std::chrono_literals;

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

uint run_single(size_t workers) {
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

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }

  return value.GetReadOnly();
}

uint run_mrv_flex_vector(size_t workers) {
  auto value = splittable::mrv::mrv_flex_vector::new_instance(0);
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

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }

  auto result =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });

  return result;
}

uint run_pr_array(size_t workers) {
  splittable::pr::pr_array::set_num_threads(workers);
  auto value = splittable::pr::pr_array::new_instance(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  const auto workload = TOTAL_ITERATIONS / workers;

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, workload]() {
      double val;
      value->register_thread();
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

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }

  auto result =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });

  return result;
}

int main(int argc, char const* argv[]) {
  if (argc < 3) {
    std::cerr << "requires 2 positional arguments: test, number of workers"
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

  std::string test(argv[1]);

  uint result;
  if (test == "single") {
    result = run_single(workers);
  } else if (test == "mrv-flex-vector") {
    result = run_mrv_flex_vector(workers);
  } else if (test == "pr-array") {
    result = run_pr_array(workers);
  } else {
    std::cerr << "could not find a test with name \"" << test
              << "\"; try\"single\", \"mrv-flex-vector\", \"pr-array\""
              << std::endl;
    return 1;
  }

  uint expected = TOTAL_ITERATIONS - (TOTAL_ITERATIONS % workers);

  if (result != expected) {
    std::cerr << "ERROR: invalid result! got " << result << ", expected "
              << expected << "\n";
  }

  // return 0;
  quick_exit(0);
}
