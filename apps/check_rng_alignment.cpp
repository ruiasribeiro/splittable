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
#define SEQUENCE_SIZE 10

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::steady_clock;

using namespace std::chrono_literals;

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

void run(size_t workers, std::function<size_t(size_t, size_t)> generate) {
  std::mutex cout_mutex;
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i]() {
      std::vector<uint64_t> sequence;

      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      for (auto j = 0; j < SEQUENCE_SIZE; ++j) {
        sequence.push_back(generate(0, MAX_RANGE - 1));
      }

      {
        std::lock_guard<std::mutex> guard(cout_mutex);

        std::cout << "thread " << i << ":";
        for (auto&& num : sequence) {
          std::cout << " " << num;
        }
        std::cout << "\n";
      }
    });
  }

  bar.wait();

  for (size_t i = 0; i < workers; i++) {
    threads[i].join();
  }
}

int main(int argc, char const* argv[]) {
  if (argc < 3) {
    std::cerr << "requires 2 positional arguments: benchmark, number of "
                 "workers\n";
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

  std::cout << "Running for " << benchmark << ", " << workers << " workers\n";
  run(workers, generate);

  quick_exit(0);
}
