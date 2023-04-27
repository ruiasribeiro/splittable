#include <string>

#include "splittable/splittable.hpp"

exported_class::exported_class()
    : m_name {"splittable"}
{
}

auto exported_class::name() const -> const char*
{
  return m_name.c_str();
}
