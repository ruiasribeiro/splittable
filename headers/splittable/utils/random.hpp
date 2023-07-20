#pragma once

#include <random>

namespace splittable::utils {

/// @brief Generates a thread-safe index without needing thread
/// synchronisation. Inclusive interval.
/// @param min
/// @param max
/// @return
auto random_index(size_t min, size_t max) -> size_t;

auto random_index_mt19937(size_t min, size_t max) -> size_t;
auto random_index_minstd_rand(size_t min, size_t max) -> size_t;
auto random_index_ranlux24_base(size_t min, size_t max) -> size_t;

}  // namespace splittable::utils