#include "splittable/mrv/mrv.hpp"

namespace splittable::mrv {

std::atomic_uint mrv::id_counter{0};

auto mrv::thread_init() -> void {}
auto mrv::global_init(uint, uint num_threads_pool) -> void {
  splittable::splittable::initialise_pool(num_threads_pool);
}

}  // namespace splittable::mrv