#include <chrono>
#include <iostream>
#include <thread>

#include "splittable/mrv/mrv_array.hpp"

using namespace std::chrono_literals;

int main(int argc, char const* argv[]) {
  const auto iter = 10000;
  auto x = splittable::mrv::mrv_array::new_mrv(1);
  std::vector<std::thread> threads;
  threads.reserve(10);

  for (int i = 0; i < 10; i++) {
    threads.push_back(std::thread([&x]() {
      WSTM::Atomically([&x](WSTM::WAtomic& at) {
        auto value = x->read(at);
        // at.After([=]() { std::cout << value << std::endl; });
      });

      for (auto i = 0; i < iter; ++i) {
        WSTM::Atomically([&x](WSTM::WAtomic& at) { x->add(at, 1); });

        // try {
        //   WSTM::Atomically([&x](WSTM::WAtomic& at) { x->sub(at, 10); });
        // } catch (...) {
        // }

        // try {
        //   WSTM::Atomically([&x](WSTM::WAtomic& at) { x->sub(at, 20); });
        // } catch (...) {
        // }
      }
    }));
  }

  for (int i = 0; i < 10; i++) {
    threads[i].join();
  }

  WSTM::Atomically([&x](WSTM::WAtomic& at) {
    auto value = x->read(at);
    at.After([=]() { std::cout << value << std::endl; });
  });

  return 0;
}
