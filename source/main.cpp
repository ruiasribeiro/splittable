#include <chrono>
#include <iostream>
#include <thread>

#include "splittable/mrv/mrv_array.hpp"

using namespace std::chrono_literals;

int main(int argc, char const* argv[]) {
  auto x = splittable::mrv::mrv_array::new_mrv(10);
  std::vector<std::thread> threads;
  threads.reserve(10);

  for (int i = 0; i < 10; i++) {
    threads.push_back(std::thread([&x]() {
      WSTM::Atomically([&x](WSTM::WAtomic& at) {
        auto value = x->read(at);
        at.After([=]() { std::cout << value << std::endl; });
      });

      WSTM::Atomically([&x](WSTM::WAtomic& at) { x->add(at, 50); });

      WSTM::Atomically([&x](WSTM::WAtomic& at) {
        auto value = x->read(at);
        at.After([=]() { std::cout << value << std::endl; });
      });

      try {
        WSTM::Atomically([&x](WSTM::WAtomic& at) { x->sub(at, 10); });
      } catch (...) {
      }

      WSTM::Atomically([&x](WSTM::WAtomic& at) {
        auto value = x->read(at);
        at.After([=]() { std::cout << value << std::endl; });
      });

      try {
        WSTM::Atomically([&x](WSTM::WAtomic& at) { x->sub(at, 20); });
      } catch (...) {
      }

      WSTM::Atomically([&x](WSTM::WAtomic& at) {
        auto value = x->read(at);
        at.After([=]() { std::cout << value << std::endl; });
      });
    }));
  }

  for (int i = 0; i < 10; i++) {
    threads[i].join();
  }

  std::this_thread::sleep_for(5s);

  return 0;
}
