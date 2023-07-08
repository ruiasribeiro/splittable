/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * reservation.c: Representation of car, flight, and hotel relations
 */

#include "splittable/benchmarks/vacation/reservation.h"

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */

/* =============================================================================
 * reservation_info_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */

reservation_info_t::reservation_info_t(reservation_type_t _type, long _id,
                                       long _price) {
  type = _type;
  id = _id;
  price = _price;
}

// static void
bool reservation_t::checkReservation(WSTM::WAtomic& at) {
  auto num_used = numUsed.Get(at);
  if (num_used < 0) {
    //_ITM_abortTransaction(2);
    return false;
  }

  auto num_free = numFree.Get(at);
  if (num_free < 0) {
    //_ITM_abortTransaction(2);
    return false;
  }

  auto num_total = numTotal.Get(at);
  if (num_total < 0) {
    //_ITM_abortTransaction(2);
    return false;
  }

  if ((num_used + num_free) != num_total) {
    //_ITM_abortTransaction(2);
    return false;
  }

  if (price < 0) {
    //_ITM_abortTransaction(2);
    return false;
  }

  return true;
}

/* =============================================================================
 * reservation_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */

reservation_t::reservation_t(long _id, long _numTotal, long _price,
                             bool* success)
    : id(_id),
      numUsed(0),
      numFree(_numTotal),
      numTotal(_numTotal),
      price(_price) {
  *success = WSTM::Atomically(
      [this](WSTM::WAtomic& at) -> bool { return checkReservation(at); });
}

/* =============================================================================
 * reservation_t::addToTotal
 * -- Adds if 'num' > 0, removes if 'num' < 0;
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool reservation_t::addToTotal(WSTM::WAtomic& at, long num, bool* success) {
  auto num_free = numFree.Get(at);
  if (num_free + num < 0) {
    return false;
  }

  num_free += num;
  numFree.Set(num_free);

  auto num_total = numTotal.Get(at);
  num_total += num;
  numTotal.Set(num_total, at);

  *success = checkReservation(at);
  return true;
}

/* =============================================================================
 * reservation_t::make
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool reservation_t::make(WSTM::WAtomic& at) {
  auto num_free = numFree.Get(at);
  if (num_free < 1) {
    return false;
  }

  auto num_used = numUsed.Get(at);
  num_used += 1;
  numUsed.Set(num_used, at);

  num_free -= 1;
  numFree.Set(num_free, at);

  // TODO: the original code does not do anything with this? to check later
  checkReservation(at);
  return true;
}

/* =============================================================================
 * reservation_t::cancel
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool reservation_t::cancel(WSTM::WAtomic& at) {
  auto num_used = numUsed.Get(at);
  if (num_used < 1) {
    return false;
  }

  num_used -= 1;
  numUsed.Set(num_used, at);

  auto num_free = numFree.Get(at);
  num_free += 1;
  numFree.Set(num_free, at);

  //[wer210] Note here, return false, instead of abort in checkReservation
  return checkReservation(at);
}

/* =============================================================================
 * reservation_t::updatePrice
 * -- Failure if 'price' < 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] returns were not used before, so use it to indicate aborts
bool reservation_t::updatePrice(WSTM::WAtomic& at, long newPrice) {
  if (newPrice < 0) {
    // return FALSE;
    return true;
  }

  price = newPrice;

  return checkReservation(at);
}

/* =============================================================================
 * TEST_RESERVATION
 * =============================================================================
 */
#ifdef TEST_RESERVATION

#include <cassert>
#include <cstdio>

#include "reservation.h"

/* =============================================================================
 * reservation_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
static long reservation_compare(reservation_t* aPtr, reservation_t* bPtr) {
  return (aPtr->id - bPtr->id);
}

int main() {
  reservation_info_t* reservationInfo1Ptr;
  reservation_info_t* reservationInfo2Ptr;
  reservation_info_t* reservationInfo3Ptr;

  reservation_t* reservation1Ptr;
  reservation_t* reservation2Ptr;
  reservation_t* reservation3Ptr;

  assert(memory_init(1, 4, 2));

  puts("Starting...");

  reservationInfo1Ptr = reservation_info_alloc(0, 0, 0);
  reservationInfo2Ptr = reservation_info_alloc(0, 0, 1);
  reservationInfo3Ptr = reservation_info_alloc(2, 0, 1);

  /* Test compare */
  assert(reservation_info_compare(reservationInfo1Ptr, reservationInfo2Ptr) ==
         0);
  assert(reservation_info_compare(reservationInfo1Ptr, reservationInfo3Ptr) >
         0);
  assert(reservation_info_compare(reservationInfo2Ptr, reservationInfo3Ptr) >
         0);

  reservation1Ptr = reservation_alloc(0, 0, 0);
  reservation2Ptr = reservation_alloc(0, 0, 1);
  reservation3Ptr = reservation_alloc(2, 0, 1);

  /* Test compare */
  assert(reservation_compare(reservation1Ptr, reservation2Ptr) == 0);
  assert(reservation_compare(reservation1Ptr, reservation3Ptr) != 0);
  assert(reservation_compare(reservation2Ptr, reservation3Ptr) != 0);

  /* Cannot reserve if total is 0 */
  assert(!reservation_make(reservation1Ptr));

  /* Cannot cancel if used is 0 */
  assert(!reservation_cancel(reservation1Ptr));

  /* Cannot update with negative price */
  assert(!reservation_updatePrice(reservation1Ptr, -1));

  /* Cannot make negative total */
  assert(!reservation_addToTotal(reservation1Ptr, -1));

  /* Update total and price */
  assert(reservation_addToTotal(reservation1Ptr, 1));
  assert(reservation_updatePrice(reservation1Ptr, 1));
  assert(reservation1Ptr->numUsed == 0);
  assert(reservation1Ptr->numFree == 1);
  assert(reservation1Ptr->numTotal == 1);
  assert(reservation1Ptr->price == 1);
  checkReservation(reservation1Ptr);

  /* Make and cancel reservation */
  assert(reservation_make(reservation1Ptr));
  assert(reservation_cancel(reservation1Ptr));
  assert(!reservation_cancel(reservation1Ptr));

  reservation_info_free(reservationInfo1Ptr);
  reservation_info_free(reservationInfo2Ptr);
  reservation_info_free(reservationInfo3Ptr);

  reservation_free(reservation1Ptr);
  reservation_free(reservation2Ptr);
  reservation_free(reservation3Ptr);

  puts("All tests passed.");

  return 0;
}

#endif /* TEST_RESERVATION */

/* =============================================================================
 *
 * End of reservation.c
 *
 * =============================================================================
 */
