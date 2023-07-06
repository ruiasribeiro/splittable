/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * customer.h: Representation of customer
 */

#pragma once

#include <cassert>
#include <cstdlib>
#include <memory>

#include "immer/set.hpp"
#include "reservation.h"
#include "wstm/stm.h"

struct customer_t {
  long id;
  WSTM::WVar<immer::set<reservation_info_t>> reservationInfoList;

  customer_t(long id);
  ~customer_t();

  /*
   * customer_addReservationInfo
   * -- Returns TRUE if success, else FALSE
   */
  bool addReservationInfo(WSTM::WAtomic& at, reservation_type_t type, long id,
                          long price);

  /*
   * customer_removeReservationInfo
   * -- Returns TRUE if success, else FALSE
   */
  bool removeReservationInfo(WSTM::WAtomic& at, reservation_type_t type,
                             long id);

  /*
   * customer_getBill
   * -- Returns total cost of reservations
   */
  long getBill(WSTM::WAtomic& at);
};
