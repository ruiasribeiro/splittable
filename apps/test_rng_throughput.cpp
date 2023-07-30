#include <XoshiroCpp.hpp>
#include <atomic>
#include <boost/program_options.hpp>
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

const seconds warmup(5);

struct result_t {
  uint64_t operations;
};

struct options_t {
  std::string benchmark;
  size_t num_workers;
  seconds duration;
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

auto random_index_xoshiro128plusplus(size_t min, size_t max) -> size_t {
  thread_local XoshiroCpp::Xoshiro128PlusPlus generator(std::random_device{}());
  thread_local std::uniform_int_distribution<size_t> distribution(min, max);
  return distribution(generator);
}

result_t run(options_t options,
             std::function<size_t(size_t, size_t)> generate) {
  auto total_ops = std::atomic_uint64_t(0);
  auto threads = std::make_unique<std::thread[]>(options.num_workers);

  boost::barrier bar(options.num_workers + 1);

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i] = std::thread([&, i]() {
      size_t ops = 0;
      uint val{};
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < warmup) {
        val = generate(MIN_RANGE, MAX_RANGE);
      }

      start = now();

      while ((now() - start) < options.duration) {
        val = generate(MIN_RANGE, MAX_RANGE);
        ++ops;
      }

      volatile auto avoid_optimisation __attribute__((unused)) = val;
      total_ops.fetch_add(ops);
    });
  }

  bar.wait();

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i].join();
  }

  return {.operations = total_ops.load()};
}

int main(int argc, char const* argv[]) {
  namespace po = boost::program_options;

  options_t options;
  po::options_description description("Allowed options");

  // clang-format off
  description.add_options()
    ("help,h", "produce help message")
    ("benchmark,b", 
      po::value<std::string>()->default_value(""), 
      "set splittable type")
    ("num_workers,w", 
      po::value<size_t>()->required(), 
      "set number of clients for the benchmark")
    ("duration,d", 
      po::value<size_t>()->required(),  
      "set benchmark duration (in seconds)");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, description), vm);
  po::notify(vm);

  options.benchmark = vm["benchmark"].as<std::string>();
  options.num_workers = vm["num_workers"].as<size_t>();
  options.duration = seconds{vm["duration"].as<size_t>()};

  std::function<size_t(size_t, size_t)> generate;
  if (options.benchmark == "mt") {
    generate = random_index_mt19937;
  } else if (options.benchmark == "minstd") {
    generate = random_index_minstd_rand;
  } else if (options.benchmark == "ranlux24") {
    generate = random_index_ranlux24_base;
  } else if (options.benchmark == "xoshiro128") {
    generate = random_index_xoshiro128plusplus;
  } else {
    std::cerr << "could not find a benchmark with name \"" << options.benchmark
              << "\"; try \"mt\", \"minstd\", \"ranlux24\", \"xoshiro128\"\n";
    return 1;
  }

  result_t result = run(options, generate);

  // CSV header: benchmark, workers, execution time (s), # of commited
  // operations, throughput (ops/s)
  std::cout << options.benchmark << "," << options.num_workers << ","
            << options.duration.count() << "," << result.operations << ","
            << static_cast<double>(result.operations) / options.duration.count()
            << "\n";

  // return 0;
  quick_exit(0);
}
