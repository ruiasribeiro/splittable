#include "splittable/utils/random.hpp"

namespace splittable::utils {

auto random_index(size_t min, size_t max) -> size_t {
  thread_local XoshiroCpp::Xoshiro128PlusPlus generator(std::random_device{}());
  std::uniform_int_distribution<size_t> distribution(min, max);
  return distribution(generator);
}

}  // namespace splittable::utils