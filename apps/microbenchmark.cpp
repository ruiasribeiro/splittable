#include <wstm/stm.h>

#include <atomic>
#include <boost/program_options.hpp>
#include <boost/thread/barrier.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
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

struct options_t {
  std::string benchmark;
  size_t num_workers;
  size_t read_percentage;
  seconds duration;
  size_t time_padding;
  size_t num_threads_pool;
  size_t scale;
};

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

template <typename S>
result_t run(options_t options) {
  auto total_reads = std::atomic_uint(0);
  auto total_writes = std::atomic_uint(0);

  auto value = S::new_instance(0);
  S::global_init(options.num_workers, options.num_threads_pool);

  auto threads = std::make_unique<std::thread[]>(options.num_workers);

  boost::barrier bar(options.num_workers + 1);

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i] = std::thread([&, i, options]() {
      S::thread_init();

      double val{};
      uint val2{};
      size_t reads = 0;
      size_t writes = 0;

      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < options.duration) {
        auto random_pick = splittable::utils::random_index(1, 100);

        try {
          WSTM::Atomically([&](WSTM::WAtomic& at) {
            val = waste_time(options.time_padding);

            if (random_pick <= options.read_percentage) {
              val2 = value->read(at);
            } else if (splittable::utils::random_index(0, options.scale) == 0) {
              value->add(at, options.scale);
            } else {
              value->sub(at, 1);
            }

            val = waste_time(options.time_padding);
          });

          if (random_pick <= options.read_percentage) {
            reads++;
          } else {
            writes++;
          }
        } catch (...) {
          // this catch block should only be reached when a sub cannot perform
          // due to no stock, which isn't a problem; we just don't count that
          // write
        }
      }

      volatile auto avoid_optimisation __attribute__((unused)) = val;
      volatile auto avoid_optimisation2 __attribute__((unused)) = val2;
      total_reads.fetch_add(reads);
      total_writes.fetch_add(writes);
    });
  }

  bar.wait();

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i].join();
  }

  S::shutdown();

  auto stats = splittable::splittable::get_global_stats();
  auto abort_rate =
      static_cast<double>(stats.aborts) / (stats.aborts + stats.commits);

  return {.writes = total_writes.load(),
          .reads = total_reads.load(),
          .abort_rate = abort_rate};
}

int main(int argc, char const* argv[]) {
  namespace po = boost::program_options;

  options_t options;
  po::options_description description("Allowed options");

  // clang-format off
  description.add_options()
    ("help,h", "produce help message")
    ("benchmark,b", 
      po::value<std::string>(), 
      "set splittable type")
    ("num_workers,w", 
      po::value<size_t>(), 
      "set number of clients for the benchmark")
    ("read_percentage,r", 
      po::value<size_t>(),  
      "set percentage of read operations")
    ("duration,d", 
      po::value<size_t>(),  
      "set benchmark duration (in seconds)")
    ("time_padding,p", 
      po::value<size_t>(),  
      "set padding for the transactions")
    ("num_threads_pool,t", 
      po::value<size_t>(),  
      "set number of threads for the splittable pool")
    ("scale,s", 
      po::value<size_t>()->default_value(1), 
      "set scale for writes (how big should adds be per sub)");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, description), vm);
  po::notify(vm);

  options.benchmark = vm["benchmark"].as<std::string>();
  options.num_workers = vm["num_workers"].as<size_t>();
  options.read_percentage = vm["read_percentage"].as<size_t>();
  options.duration = seconds{vm["duration"].as<size_t>()};
  options.time_padding = vm["time_padding"].as<size_t>();
  options.num_threads_pool = vm["num_threads_pool"].as<size_t>();
  options.scale = vm["scale"].as<size_t>();

  result_t result;
  if (options.benchmark == "single") {
    using splittable_t = splittable::single::single;
    result = run<splittable_t>(options);
  } else if (options.benchmark == "mrv-flex-vector") {
    using splittable_t = splittable::mrv::mrv_flex_vector;
    result = run<splittable_t>(options);
  } else if (options.benchmark == "pr-array") {
    using splittable_t = splittable::pr::pr_array;
    result = run<splittable_t>(options);
  } else {
    std::cerr << "could not find a benchmark with name \"" << options.benchmark
              << "\"; try\"single\", \"mrv-flex-vector\", \"pr-array\"\n";
    return 1;
  }

  // CSV: benchmark, workers, execution time, read percentage, writes, reads,
  // write throughput (ops/s), read throughput (ops/s), abort rate
  std::cout << options.benchmark << "," << options.num_workers << ","
            << options.duration.count() << "," << options.read_percentage << ","
            << result.writes << "," << result.reads << ","
            << result.writes / options.duration.count() << ","
            << result.reads / options.duration.count() << ","
            << result.abort_rate << "\n";

  // return 0;
  quick_exit(0);
}
