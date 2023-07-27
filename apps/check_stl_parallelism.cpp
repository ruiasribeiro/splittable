// Adapted from:
// https://www.reddit.com/r/cpp_questions/comments/n4xg3h/does_execution_policy_in_stdtransform_in_gcc_have/

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <execution>
#include <iostream>
#include <numeric>
#include <thread>
#include <utility>

static double f(double x) noexcept {
  const int N = 1000;
  for (int i = 0; i < N; ++i) {
    x = std::log2(x);
    x = std::cos(x);
    x = x * x + 1;
  }
  return x;
}

static double sum(const std::vector<double>& vec) {
  double sum = 0;
  for (auto x : vec) sum += x;
  return sum;
}

int main() {
  std::cout << "Hey! Your machine has " << std::thread::hardware_concurrency()
            << " cores!\n";

  // Make an input vector.
  const int N = 1000000;
  std::vector<double> vec_in(N);
  for (int i = 0; i < N; ++i) vec_in[i] = i + 1;

  {  // Case #1: Plain transform, no parallelism.
    auto startTime = std::chrono::steady_clock::now();
    std::vector<double> vec_out(N);
    std::transform(vec_in.cbegin(), vec_in.cend(), vec_out.begin(), f);
    auto endTime = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    std::cout << "sequential: sum = " << sum(vec_out)
              << ", time = " << diff.count() << "\n";
  }

  {  // Case #2: Transform with parallel unsequenced.
    std::vector<double> vec_out(N);
    auto startTime = std::chrono::steady_clock::now();
    std::transform(std::execution::par_unseq, vec_in.cbegin(), vec_in.cend(),
                   vec_out.begin(), f);
    auto endTime = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    std::cout << "  parallel: sum = " << sum(vec_out)
              << ", time = " << diff.count() << "\n";
  }
}