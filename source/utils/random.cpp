#include "splittable/utils/random.hpp"

namespace splittable::utils {

auto random_index(size_t min, size_t max) -> size_t {
  return random_index_mt19937(min, max);
}

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

}  // namespace splittable::utils