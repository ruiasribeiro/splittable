#include <atomic>
#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

#define MIN_RANGE 0
#define MAX_RANGE 127

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::steady_clock;

using namespace std::chrono_literals;

struct result_t {
  uint64_t operations;
};

auto random_index_mt19937(size_t min, size_t max) -> size_t {
  thread_local std::mt19937 generator(std::random_device{}());
  thread_local std::uniform_int_distribution<size_t> distribution(min, max);
  return distribution(generator);
}

auto random_index_minstd_rand(size_t min, size_t max) -> size_t {
  thread_local std::minstd_rand generator(std::random_device{}());
  thread_local std::uniform_int_distribution<size_t> distribution(min, max);
  return distribution(generator);
}

auto random_index_ranlux24_base(size_t min, size_t max) -> size_t {
  thread_local std::ranlux24_base generator(std::random_device{}());
  thread_local std::uniform_int_distribution<size_t> distribution(min, max);
  return distribution(generator);
}

result_t run(size_t workers, seconds duration,
             std::function<size_t(size_t, size_t)> generate) {
  auto total_ops = std::atomic_uint64_t(0);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      size_t ops = 0;
      uint val;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        val = generate(MIN_RANGE, MAX_RANGE);
        ++ops;
      }

      volatile auto avoid_optimisation = val;
      total_ops.fetch_add(ops);
    });
  }

  bar.wait();

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }

  return {.operations = total_ops.load()};
}

int main(int argc, char const* argv[]) {
  if (argc < 4) {
    std::cerr << "requires 3 positional arguments: benchmark, number of "
                 "workers, execution time (s)\n";
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

  std::string benchmark(argv[1]);

  std::function<size_t(size_t, size_t)> generate;
  if (benchmark == "mt") {
    generate = random_index_mt19937;
  } else if (benchmark == "minstd") {
    generate = random_index_minstd_rand;
  } else if (benchmark == "ranlux24") {
    generate = random_index_ranlux24_base;
  } else {
    std::cerr << "could not find a benchmark with name \"" << benchmark
              << "\"; try \"mt\", \"minstd\", \"ranlux24\"\n";
    return 1;
  }

  result_t result;
  result = run(workers, execution_time, generate);

  // CSV header: benchmark, workers, execution time (s), # of commited
  // operations, throughput (ops/s)
  std::cout << benchmark << "," << workers << "," << execution_time.count()
            << "," << result.operations << ","
            << result.operations / execution_time.count() << "\n";

  // return 0;
  quick_exit(0);
}
