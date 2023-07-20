#include <wstm/stm.h>

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

struct result_t {
  uint operations;
};

struct alignas(std::hardware_destructive_interference_size) padded_wvar_t {
  WSTM::WVar<uint> value;
  padded_wvar_t() : value() {}
};

double waste_time(size_t iterations) {
  auto value = 1;

  for (size_t i = 0; i < iterations; ++i) {
    value += value;
  }

  return value;
}

result_t bm_stl_vector(size_t workers, seconds duration, int time_padding) {
  auto values = std::vector<padded_wvar_t>(workers);
  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      double val;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(time_padding);
          values[i].value.Set(values[i].value.Get(at) + 1, at);
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

  auto operations = 0u;
  for (auto&& value : values) {
    operations += value.value.GetReadOnly();
  }

  return {.operations = operations};
}

result_t bm_stl_vector_ptrs(size_t workers, seconds duration,
                            int time_padding) {
  auto values = std::vector<std::shared_ptr<padded_wvar_t>>();
  for (auto i = 0u; i < workers; ++i) {
    values.push_back(std::make_shared<padded_wvar_t>());
  }

  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      double val;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(time_padding);
          values[i]->value.Set(values[i]->value.Get(at) + 1, at);
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

  auto operations = 0u;
  for (auto&& value : values) {
    operations += value->value.GetReadOnly();
  }

  return {.operations = operations};
}

result_t bm_immer_flex_vector(size_t workers, seconds duration,
                              int time_padding) {
  auto values = immer::flex_vector<std::shared_ptr<padded_wvar_t>>();

  auto t = values.transient();
  for (auto i = 0u; i < workers; ++i) {
    t.push_back(std::make_shared<padded_wvar_t>());
  }
  values = t.persistent();

  auto threads = std::make_unique<std::thread[]>(workers);

  boost::barrier bar(workers + 1);

  for (size_t i = 0; i < workers; i++) {
    threads[i] = std::thread([&, i, duration]() {
      double val;
      bar.wait();

      auto now = steady_clock::now;
      auto start = now();

      while ((now() - start) < duration) {
        WSTM::Atomically([&](WSTM::WAtomic& at) {
          val = waste_time(time_padding);
          values[i]->value.Set(values[i]->value.Get(at) + 1, at);
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

  auto operations = 0u;
  for (auto&& value : values) {
    operations += value->value.GetReadOnly();
  }

  return {.operations = operations};
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

  result_t result;
  if (benchmark == "stl_vector") {
    result = bm_stl_vector(workers, execution_time, padding);
  } else if (benchmark == "stl_vector_ptrs") {
    result = bm_stl_vector_ptrs(workers, execution_time, padding);
  } else if (benchmark == "immer_flex_vector") {
    result = bm_immer_flex_vector(workers, execution_time, padding);
  } else {
    std::cerr << "could not find a benchmark with name \"" << benchmark
              << "\"; try \"stl_vector\", \"stl_vector_ptrs\", "
                 "\"immer_flex_vector\"\n";
    return 1;
  }

  // CSV header: benchmark, workers, execution time, padding, # of commited
  // operations, throughput (ops/s)
  std::cout << benchmark << "," << workers << "," << execution_time.count()
            << "," << padding << "," << result.operations << ","
            << result.operations / execution_time.count() << "\n";

  // return 0;
  quick_exit(0);
}
