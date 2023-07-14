#pragma once

#include "splittable/splittable.hpp"
#include "wstm/stm.h"

namespace splittable::single {

class single : public splittable {
 private:
  WSTM::WVar<uint> value;

 public:
  // TODO: this is not private because of make_shared, need to revise that later
  single(uint value);

  auto static new_instance(uint value) -> std::shared_ptr<single>;
  auto static delete_instance(std::shared_ptr<single>) -> void;

  auto read(WSTM::WAtomic& at) -> uint;
  // auto inconsistent_read(WSTM::WInconsistent& inc) -> uint;
  auto add(WSTM::WAtomic& at, uint value) -> void;
  auto sub(WSTM::WAtomic& at, uint value) -> void;
};

}  // namespace splittable::single