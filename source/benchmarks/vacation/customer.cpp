/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * customer.c: Representation of customer
 */

#include "splittable/benchmarks/vacation/customer.h"

/* =============================================================================
 * customer_alloc
 * =============================================================================
 */
customer_t::customer_t(long _id) : id(_id) {}

/* =============================================================================
 * customer_free
 * =============================================================================
 */
customer_t::~customer_t() {}

/* =============================================================================
 * customer_addReservationInfo
 * -- Returns TRUE if success, else FALSE
 * =============================================================================
 */
bool customer_t::addReservationInfo(WSTM::WAtomic& at, reservation_type_t type,
                                    long id, long price) {
  reservation_info_t reservation_info(type, id, price);

  auto current_list = reservationInfoList.Get(at);
  auto updated_list = current_list.insert(reservation_info);

  auto did_update = current_list != updated_list;
  if (did_update) {
    reservationInfoList.Set(updated_list, at);
  }

  return did_update;
}

/* =============================================================================
 * customer_removeReservationInfo
 * -- Returns TRUE if success, else FALSE
 * =============================================================================
 */
//[wer210] called only in manager.c, cancel() which is used to cancel a
//         flight/car/room, which never happens...
bool customer_t::removeReservationInfo(WSTM::WAtomic& at,
                                       reservation_type_t type, long id) {
  // NB: price not used to compare reservation infos
  reservation_info_t findReservationInfo(type, id, 0);

  auto current_list = reservationInfoList.Get(at);
  auto reservationInfoPtr = current_list.find(findReservationInfo);

  if (reservationInfoPtr == nullptr) {
    return false;
  }

  auto updated_list = current_list.erase(findReservationInfo);
  bool status = current_list != updated_list;  // removed the element

  //[wer210] get rid of restart()
  if (status == false) {
    //_ITM_abortTransaction(2);
    return false;
  }

  delete reservationInfoPtr;

  return true;
}

/* =============================================================================
 * customer_getBill
 * -- Returns total cost of reservations
 * =============================================================================
 */
long customer_t::getBill(WSTM::WAtomic& at) {
  long bill = 0;
  for (auto i : reservationInfoList.Get(at)) {
    bill += i.price;
  }
  return bill;
}

/* =============================================================================
 * TEST_CUSTOMER
 * =============================================================================
 */
#ifdef TEST_CUSTOMER

#include <cassert>
#include <cstdio>

int main() {
  customer_t* customer1Ptr;
  customer_t* customer2Ptr;
  customer_t* customer3Ptr;

  assert(memory_init(1, 4, 2));

  puts("Starting...");

  customer1Ptr = customer_alloc(314);
  customer2Ptr = customer_alloc(314);
  customer3Ptr = customer_alloc(413);

  /* Test compare */
  /* =============================================================================
   * customer_compare
   * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
   * REMOVED, never used except here.
   * =============================================================================
   */
  // assert(customer_compare(customer1Ptr, customer2Ptr) == 0);
  assert((customer1Ptr->id - customer2Ptr->id) == 0);
  assert((customer2Ptr->id, customer3Ptr->id) != 0);
  assert((customer1Ptr->id, customer3Ptr->id) != 0);

  /* Test add reservation info */
  assert(customer_addReservationInfo(customer1Ptr, 0, 1, 2));
  assert(!customer_addReservationInfo(customer1Ptr, 0, 1, 2));
  assert(customer_addReservationInfo(customer1Ptr, 1, 1, 3));
  assert(customer_getBill(customer1Ptr) == 5);

  /* Test remove reservation info */
  assert(!customer_removeReservationInfo(customer1Ptr, 0, 2));
  assert(!customer_removeReservationInfo(customer1Ptr, 2, 0));
  assert(customer_removeReservationInfo(customer1Ptr, 0, 1));
  assert(!customer_removeReservationInfo(customer1Ptr, 0, 1));
  assert(customer_getBill(customer1Ptr) == 3);
  assert(customer_removeReservationInfo(customer1Ptr, 1, 1));
  assert(customer_getBill(customer1Ptr) == 0);

  customer_free(customer1Ptr);
  customer_free(customer2Ptr);
  customer_free(customer3Ptr);

  puts("All tests passed.");

  return 0;
}

#endif /* TEST_CUSTOMER */
