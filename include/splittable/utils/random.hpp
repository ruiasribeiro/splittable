#pragma once

#include <random>

namespace splittable::utils {

/// @brief Generates a thread-safe index without needing thread
/// synchronisation. Inclusive interval.
/// @param min
/// @param max
/// @return
auto random_index(size_t min, size_t max) -> size_t;

}  // namespace splittable::utils