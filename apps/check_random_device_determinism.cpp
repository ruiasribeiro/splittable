#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

int main() {
  auto num_threads = 10ul;

  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  std::mutex cout_mutex;

  for (auto i = 0ul; i < num_threads; ++i) {
    threads.emplace_back([i, &cout_mutex]() {
      auto num = std::random_device{}();

      {
        std::lock_guard<std::mutex> guard(cout_mutex);
        std::cout << "thread " << i << ": " << num << "\n";
      }
    });
  }

  for (auto i = 0ul; i < num_threads; ++i) {
    threads[i].join();
  }

  return 0;
}
