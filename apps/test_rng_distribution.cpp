#include <array>
#include <atomic>
#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

#define MAX_RANGE 128

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::steady_clock;

using namespace std::chrono_literals;

struct result_t {
  std::array<uint64_t, MAX_RANGE> occurrences;
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

result_t run(seconds duration, std::function<size_t(size_t, size_t)> generate) {
  std::array<uint64_t, MAX_RANGE> occurrences;
  occurrences.fill(0);

  auto now = steady_clock::now;
  auto start = now();

  while ((now() - start) < duration) {
    ++occurrences[generate(0, MAX_RANGE - 1)];
  }

  return {.occurrences = occurrences};
}

int main(int argc, char const* argv[]) {
  if (argc < 3) {
    std::cerr
        << "requires 2 positional arguments: benchmark, execution time (s)\n";
    return 1;
  }

  seconds execution_time;
  try {
    execution_time = seconds{std::stoi(argv[2])};
  } catch (...) {
    std::cerr << "could not convert \"" << argv[2] << "\" to an integer\n";
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
  result = run(execution_time, generate);

  // CSV header: benchmark, execution time (s), occurrences
  std::cout << benchmark << "," << execution_time.count() << ",";
  for (size_t i = 0; i < MAX_RANGE - 1; ++i) {
    std::cout << result.occurrences[i] << ";";
  }
  std::cout << result.occurrences[MAX_RANGE - 1] << "\n";

  // return 0;
  quick_exit(0);
}
