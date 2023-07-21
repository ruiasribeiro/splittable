#include "splittable/pr/pr.hpp"

namespace splittable::pr {

std::atomic_uint pr::id_counter{0};

std::atomic_uint pr::thread_id_counter{0};
// needs to be set with register_thread()
thread_local uint pr::thread_id;
// needs to be set with set_num_threads()
uint pr::num_threads;

auto pr::thread_init() -> void { register_thread(); }
auto pr::global_init(uint num_threads) -> void { set_num_threads(num_threads); }

// should only be called once, at the start of the thread. it is assumed that
// once a thread is registered it runs until the end
auto pr::register_thread() -> void {
  thread_id = thread_id_counter.fetch_add(1, std::memory_order_relaxed);
}

// should only be called once, at the start of the program
auto pr::set_num_threads(uint num) -> void { num_threads = num; }

}  // namespace splittable::pr