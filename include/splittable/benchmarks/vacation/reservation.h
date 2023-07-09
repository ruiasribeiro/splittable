/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * reservation.hpp: Representation of car, flight, and hotel relations
 */

#pragma once

#include <boost/functional/hash.hpp>

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
  // reservation_info_t(const reservation_info_t&);

  // bool operator==(reservation_info_t const& rhs);

  // NB: no need to provide destructor... default will do
};

// Based on https://stackoverflow.com/a/52402866
size_t hash_value(reservation_info_t const&);

namespace std {
template <>
struct hash<::reservation_info_t> : boost::hash<::reservation_info_t> {};

template <>
struct equal_to<::reservation_info_t> {
  constexpr bool operator()(const reservation_info_t& lhs,
                            const reservation_info_t& rhs) const {
    return lhs.type == rhs.type && lhs.id == rhs.id;
  }
};
}  // namespace std

struct reservation_t {
  long id;
  WSTM::WVar<long> numUsed;
  WSTM::WVar<long> numFree;
  WSTM::WVar<long> numTotal;
  WSTM::WVar<long> price;

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
  bool updatePrice(WSTM::WAtomic& at, long newPrice);

 private:
  bool checkReservation(WSTM::WAtomic& at);
};
