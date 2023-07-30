#include <wstm/stm.h>

#include <boost/program_options.hpp>
#include <boost/thread/barrier.hpp>
#include <chrono>
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

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
  size_t time_padding;
};

using wvar_t = WSTM::WVar<uint>;

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

result_t bm_stl_vector(options_t options) {
  auto total_writes = std::atomic_uint64_t(0);
  auto values = std::vector<wvar_t>(options.num_workers);
  auto threads = std::make_unique<std::thread[]>(options.num_workers);

  boost::barrier bar(options.num_workers + 1);

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i] = std::thread([&, i]() {
      size_t writes = 0;
      double val = 0.0;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < warmup) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          values[i].Set(values[i].Get(at) + 1, at);
          val = waste_time(options.time_padding);
        });
      }

      start = now();

      while ((now() - start) < options.duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          values[i].Set(values[i].Get(at) + 1, at);
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

  return {.operations = total_writes.load()};
}

result_t bm_stl_vector_ptrs(options_t options) {
  auto total_writes = std::atomic_uint64_t(0);

  auto values = std::vector<std::shared_ptr<wvar_t>>();
  for (auto i = 0u; i < options.num_workers; ++i) {
    values.push_back(std::make_shared<wvar_t>());
  }

  auto threads = std::make_unique<std::thread[]>(options.num_workers);

  boost::barrier bar(options.num_workers + 1);

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i] = std::thread([&, i]() {
      size_t writes = 0;
      double val = 0.0;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < warmup) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          values[i]->Set(values[i]->Get(at) + 1, at);
          val = waste_time(options.time_padding);
        });
      }

      start = now();

      while ((now() - start) < options.duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          values[i]->Set(values[i]->Get(at) + 1, at);
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

  return {.operations = total_writes.load()};
}

result_t bm_immer_flex_vector(options_t options) {
  auto total_writes = std::atomic_uint64_t(0);
  auto values = immer::flex_vector<std::shared_ptr<wvar_t>>();

  auto t = values.transient();
  for (auto i = 0u; i < options.num_workers; ++i) {
    t.push_back(std::make_shared<wvar_t>());
  }
  values = t.persistent();

  auto threads = std::make_unique<std::thread[]>(options.num_workers);

  boost::barrier bar(options.num_workers + 1);

  for (size_t i = 0; i < options.num_workers; i++) {
    threads[i] = std::thread([&, i]() {
      size_t writes = 0;
      double val = 0.0;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < warmup) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          values[i]->Set(values[i]->Get(at) + 1, at);
          val = waste_time(options.time_padding);
        });
      }

      start = now();

      while ((now() - start) < options.duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(options.time_padding);
          values[i]->Set(values[i]->Get(at) + 1, at);
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

  return {.operations = total_writes.load()};
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
  if (options.benchmark == "stl_vector") {
    result = bm_stl_vector(options);
  } else if (options.benchmark == "stl_vector_ptrs") {
    result = bm_stl_vector_ptrs(options);
  } else if (options.benchmark == "immer_flex_vector") {
    result = bm_immer_flex_vector(options);
  } else {
    std::cerr << "could not find a benchmark with name \"" << options.benchmark
              << "\"; try \"stl_vector\", \"stl_vector_ptrs\", "
                 "\"immer_flex_vector\"\n";
    return 1;
  }

  // CSV header: benchmark, workers, execution time, padding, # of commited
  // operations, throughput (ops/s)
  std::cout << options.benchmark << "," << options.num_workers << ","
            << options.duration.count() << "," << options.time_padding << ","
            << result.operations << ","
            << static_cast<double>(result.operations) / options.duration.count()
            << "\n";

  quick_exit(0);
}
