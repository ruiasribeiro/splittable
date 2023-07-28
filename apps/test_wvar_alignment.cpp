#include <wstm/stm.h>

#include <atomic>
#include <boost/program_options.hpp>
#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

using std::chrono::seconds;
using std::chrono::steady_clock;

using namespace std::chrono_literals;

const seconds warmup(5);

struct result_t {
  uint writes;
};

struct options_t {
  std::string benchmark;
  size_t num_workers;
  seconds duration;
  size_t time_padding;
};

struct alignas(std::hardware_destructive_interference_size) aligned_wvar_t
    : public WSTM::WVar<uint> {
  // inherit all of the constructors
  using WSTM::WVar<uint>::WVar;
};

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

template <typename T>
result_t run_immer(options_t options) {
  auto total_writes = std::atomic_uint(0);
  auto threads = std::make_unique<std::thread[]>(options.num_workers);

  immer::flex_vector<std::shared_ptr<T>> chunks;
  auto t = chunks.transient();
  for (auto i = 0u; i < options.num_workers; ++i) {
    t.push_back(std::make_shared<T>(0u));
  }
  chunks = t.persistent();

  boost::barrier bar(options.num_workers + 1);

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i] = std::thread([&, i, options]() {
      size_t writes = 0;
      double val{};

      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < warmup) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          chunks[i]->Set(chunks[i]->Get(at) + 1, at);
          val = waste_time(options.time_padding);
        });
      }

      start = now();

      while ((now() - start) < options.duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          chunks[i]->Set(chunks[i]->Get(at) + 1, at);
          val = waste_time(options.time_padding);
        });

        writes++;
      }

      volatile auto avoid_optimisation __attribute__((unused)) = val;
      total_writes.fetch_add(writes);
    });
  }

  bar.wait();

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i].join();
  }

  return {.writes = total_writes.load()};
}

template <typename T>
result_t run_std(options_t options) {
  auto total_writes = std::atomic_uint(0);
  auto threads = std::make_unique<std::thread[]>(options.num_workers);

  std::vector<T> chunks;
  chunks.reserve(options.num_workers);
  for (auto i = 0u; i < options.num_workers; ++i) {
    chunks.emplace_back(0u);
  }

  boost::barrier bar(options.num_workers + 1);

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i] = std::thread([&, i, options]() {
      size_t writes = 0;
      double val{};

      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < warmup) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          chunks[i].Set(chunks[i].Get(at) + 1, at);
          val = waste_time(options.time_padding);
        });
      }

      start = now();

      while ((now() - start) < options.duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          chunks[i].Set(chunks[i].Get(at) + 1, at);
          val = waste_time(options.time_padding);
        });

        writes++;
      }

      volatile auto avoid_optimisation __attribute__((unused)) = val;
      total_writes.fetch_add(writes);
    });
  }

  bar.wait();

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i].join();
  }

  return {.writes = total_writes.load()};
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
      "set benchmark duration (in seconds)")
    ("time_padding,p", 
      po::value<size_t>()->required(),  
      "set padding for the transactions");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, description), vm);
  po::notify(vm);

  options.benchmark = vm["benchmark"].as<std::string>();
  options.num_workers = vm["num_workers"].as<size_t>();
  options.duration = seconds{vm["duration"].as<size_t>()};
  options.time_padding = vm["time_padding"].as<size_t>();

  result_t result;
  if (options.benchmark == "immer-wvar") {
    using wvar_t = WSTM::WVar<uint>;
    result = run_immer<wvar_t>(options);
  } else if (options.benchmark == "immer-aligned-wvar") {
    using wvar_t = aligned_wvar_t;
    result = run_immer<wvar_t>(options);
  } else if (options.benchmark == "std-wvar") {
    using wvar_t = WSTM::WVar<uint>;
    result = run_std<wvar_t>(options);
  } else if (options.benchmark == "std-aligned-wvar") {
    using wvar_t = aligned_wvar_t;
    result = run_std<wvar_t>(options);
  } else {
    std::cerr << "could not find a benchmark with name \"" << options.benchmark
              << "\"; try \"immer-wvar\", \"immer-aligned-wvar\", "
                 "\"std-wvar\", \"std-aligned-wvar\"\n";
    return 1;
  }

  // CSV: benchmark, workers, execution time, padding, writes, throughput
  // (ops/s)
  std::cout << options.benchmark << "," << options.num_workers << ","
            << options.duration.count() << "," << options.time_padding << ","
            << result.writes << "," << result.writes / options.duration.count()
            << "\n";

  // return 0;
  quick_exit(0);
}
