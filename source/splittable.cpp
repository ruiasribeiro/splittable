#include "splittable/splittable.hpp"

namespace splittable {

std::atomic_uint64_t splittable::total_aborts(0);
std::atomic_uint64_t splittable::total_commits(0);

std::shared_ptr<BS::thread_pool> splittable::pool = nullptr;

auto splittable::setup_transaction_tracking(WSTM::WAtomic& at) -> void {
  at.OnFail([]() { total_aborts.fetch_add(1, std::memory_order_relaxed); });
  at.After([]() { total_commits.fetch_add(1, std::memory_order_relaxed); });
}

auto splittable::initialise_pool(uint count) -> void {
  assert(pool == nullptr);
  pool = std::make_shared<BS::thread_pool>(count);
}

auto splittable::get_pool() -> std::shared_ptr<BS::thread_pool> {
  // assert(pool != nullptr);
  return pool;
}

auto splittable::get_global_stats() -> status {
  return status{.aborts = total_aborts.load(), .commits = total_commits.load()};
}

}  // namespace splittable