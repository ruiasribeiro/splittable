/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * reservation.hpp: Representation of car, flight, and hotel relations
 */

#pragma once

#include <wstm/stm.h>

#include <boost/functional/hash.hpp>
#include <memory>

#include "splittable/splittable.hpp"
#include "utility.h"

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

template <typename S>
struct reservation_t {
  long id;
  WSTM::WVar<long> numUsed;
  // WSTM::WVar<long> numFree;
  std::shared_ptr<S> numFree;
  WSTM::WVar<long> numTotal;
  WSTM::WVar<long> price;

  reservation_t(long _id, long _price, long _numTotal, bool* success)
      : id(_id), numUsed(0), numTotal(_numTotal), price(_price) {
    numFree = S::new_instance(_numTotal);
    *success = WSTM::Atomically(
        [this](WSTM::WAtomic& at) -> bool { return checkReservation(at); });
  }

  ~reservation_t() { S::delete_instance(numFree); }

  /*
   * addToTotal
   * -- Adds if 'num' > 0, removes if 'num' < 0;
   * -- Returns TRUE on success, else FALSE
   */
  bool addToTotal(WSTM::WAtomic& at, long num, bool* success) {
    if (num < 0) {
      try {
        // we start a new transaction here to avoid having an half-done
        // subtraction. doing it this way, we can rollback the subtraction.
        WSTM::Atomically([this, num](WSTM::WAtomic& at2) {
          numFree->sub(at2, static_cast<uint>(-num));
        });
      } catch (...) {
        return false;
      }
    } else {
      numFree->add(at, static_cast<uint>(num));
    }

    auto num_total = numTotal.Get(at);
    num_total += num;
    numTotal.Set(num_total, at);

    *success = checkReservation(at);
    return true;
  }

  /*
   * make
   * -- Returns TRUE on success, else FALSE
   */
  bool make(WSTM::WAtomic& at) {
    try {
      // we start a new transaction here to avoid having an half-done
      // subtraction. doing it this way, we can rollback the subtraction.
      WSTM::Atomically([this](WSTM::WAtomic& at2) { numFree->sub(at2, 1); });
    } catch (...) {
      return false;
    }

    auto num_used = numUsed.Get(at);
    num_used += 1;
    numUsed.Set(num_used, at);

    // TODO: the original code does not do anything with this? to check later
    checkReservation(at);
    return true;
  }

  /*
   * cancel
   * -- Returns TRUE on success, else FALSE
   */
  bool cancel(WSTM::WAtomic& at) {
    auto num_used = numUsed.Get(at);
    if (num_used < 1) {
      return false;
    }

    num_used -= 1;
    numUsed.Set(num_used, at);

    numFree->add(at, 1);

    //[wer210] Note here, return false, instead of abort in checkReservation
    return checkReservation(at);
  }

  /*
   * updatePrice
   * -- Failure if 'price' < 0
   * -- Returns TRUE on success, else FALSE
   */
  bool updatePrice(WSTM::WAtomic& at, long newPrice) {
    if (newPrice < 0) {
      // return FALSE;
      return true;
    }

    price.Set(newPrice, at);

    return checkReservation(at);
  }

 private:
  bool checkReservation(WSTM::WAtomic& at) {
    auto num_used = numUsed.Get(at);
    if (num_used < 0) {
      //_ITM_abortTransaction(2);
      return false;
    }

    auto num_free = numFree->read(at);
    // if (num_free < 0) {
    //   //_ITM_abortTransaction(2);
    //   return false;
    // }

    auto num_total = numTotal.Get(at);
    if (num_total < 0) {
      //_ITM_abortTransaction(2);
      return false;
    }

    if ((num_used + static_cast<long>(num_free)) != num_total) {
      //_ITM_abortTransaction(2);

      return false;
    }

    auto price_val = price.Get(at);
    if (price_val < 0) {
      //_ITM_abortTransaction(2);
      return false;
    }

    return true;
  }
};
