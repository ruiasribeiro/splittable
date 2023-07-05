/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * reservation.hpp: Representation of car, flight, and hotel relations
 */

#pragma once

#include "wstm/stm.h"

enum reservation_type_t {
  RESERVATION_CAR,
  RESERVATION_FLIGHT,
  RESERVATION_ROOM,
  NUM_RESERVATION_TYPE
};

struct reservation_info_t {
  reservation_type_t type;
  long id;
  long price; /* holds price at time reservation was made */

  reservation_info_t(reservation_type_t type, long id, long price);

  // NB: no need to provide destructor... default will do
};

struct reservation_t {
  long id;
  long numUsed;
  long numFree;
  long numTotal;
  long price;

  reservation_t(long id, long price, long numTotal, bool* success);

  // NB: no need to provide destructor... default will do

  /*
   * addToTotal
   * -- Adds if 'num' > 0, removes if 'num' < 0;
   * -- Returns TRUE on success, else FALSE
   */
  bool addToTotal(WSTM::WAtomic& at, long num, bool* success);

  /*
   * make
   * -- Returns TRUE on success, else FALSE
   */
  bool make(WSTM::WAtomic& at);

  /*
   * cancel
   * -- Returns TRUE on success, else FALSE
   */
  bool cancel(WSTM::WAtomic& at);

  /*
   * updatePrice
   * -- Failure if 'price' < 0
   * -- Returns TRUE on success, else FALSE
   */
  bool updatePrice(long newPrice);

 private:
  bool checkReservation();
};

/*
 * reservation_info_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 */
bool reservation_info_compare(reservation_info_t* aPtr,
                              reservation_info_t* bPtr);
