#include "splittable/splittable.hpp"

namespace splittable {

std::atomic_uint64_t splittable::total_aborts(0);
std::atomic_uint64_t splittable::total_commits(0);

auto splittable::setup_transaction_tracking(WSTM::WAtomic& at) -> void {
  at.OnFail([]() { total_aborts.fetch_add(1, std::memory_order_relaxed); });
  at.After([]() { total_commits.fetch_add(1, std::memory_order_relaxed); });
}

auto splittable::reset_global_stats() -> void {
  total_commits.store(0);
  total_aborts.store(0);
}

auto splittable::get_global_stats() -> status {
  return status{.aborts = total_aborts.load(), .commits = total_commits.load()};
}

}  // namespace splittable