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

size_t hash_value(reservation_info_t const& res) {
  using boost::hash_value;
  return hash_value(std::make_pair(res.id, res.type));
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
