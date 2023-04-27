#include <string>

#include "splittable/splittable.hpp"

auto main() -> int
{
  auto const exported = exported_class {};

  return std::string("splittable") == exported.name() ? 0 : 1;
}
