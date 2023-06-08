#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

#include "splittable/mrv/mrv_array.hpp"
#include "splittable/mrv/mrv_flex_vector.hpp"
#include "splittable/pr/pr_array.hpp"
#include "wstm/stm.h"

#define CACHE_LINE_SIZE 64
#define VALUE_SIZE 8
#define PADDING (CACHE_LINE_SIZE / VALUE_SIZE)

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::seconds;
using std::chrono::system_clock;

using namespace std::chrono_literals;

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

long long bm_single(size_t workers, seconds duration, int time_padding) {
  auto value = WSTM::WVar<uint>(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      double val;
      bar.wait();

      auto now = high_resolution_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(time_padding);

          auto current_value = value.Get(at);
          value.Set(current_value + 1, at);

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

  auto result = value.GetReadOnly();

  return result;
}

long long bm_mrv_array(size_t workers, seconds duration, int time_padding) {
  auto value = splittable::mrv::mrv_array::new_mrv(1);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      double val;
      bar.wait();

      auto now = high_resolution_clock::now;
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

  auto result =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });

  return result;
}

long long bm_mrv_flex_vector(size_t workers, seconds duration,
                             int time_padding) {
  auto value = splittable::mrv::mrv_flex_vector::new_mrv(1);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      double val;
      bar.wait();

      auto now = high_resolution_clock::now;
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

  auto result =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });

  return result;
}

long long bm_pr_array(size_t workers, seconds duration, int time_padding) {
  splittable::pr::pr_array::set_num_threads(workers);
  auto value = splittable::pr::pr_array::new_pr();
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      value->register_thread();

      double val;
      bar.wait();

      auto now = high_resolution_clock::now;
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

  auto result =
      WSTM::Atomically([&](WSTM::WAtomic& at) { return value->read(at); });

  return result;
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

  long long count;
  if (benchmark == "single") {
    count = bm_single(workers, execution_time, padding);
  } else if (benchmark == "mrv-array") {
    count = bm_mrv_array(workers, execution_time, padding);
  } else if (benchmark == "mrv-flex-vector") {
    count = bm_mrv_flex_vector(workers, execution_time, padding);
  } else if (benchmark == "pr-array") {
    count = bm_pr_array(workers, execution_time, padding);
  } else {
    std::cerr << "could not find a benchmark with name \"" << benchmark
              << "\"; try\"single\", \"mrv-array\", "
                 "\"mrv-flex-vector\",\"pr-array\"\n";
    return 1;
  }

  // CSV header: benchmark, workers, execution time, padding, # of commited
  // operations
  std::cout << benchmark << "," << workers << "," << execution_time.count()
            << "," << padding << "," << count << "\n";

  // return 0;
  quick_exit(0);
}
