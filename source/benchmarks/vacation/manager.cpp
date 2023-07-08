/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * manager.c: Travel reservation resource manager
 */

#include "splittable/benchmarks/vacation/manager.h"

#include <cassert>

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */

long queryNumFree(WSTM::WAtomic& at,
                  immer::map<long, std::shared_ptr<reservation_t>> tbl,
                  long id);

long queryPrice(WSTM::WAtomic& at,
                immer::map<long, std::shared_ptr<reservation_t>> tbl, long id);

bool reserve(WSTM::WAtomic& at,
             immer::map<long, std::shared_ptr<reservation_t>> tbl,
             immer::map<long, std::shared_ptr<customer_t>> custs,
             long customerId, long id, reservation_type_t type);

bool cancel(WSTM::WAtomic& at,
            immer::map<long, std::shared_ptr<reservation_t>> tbl,
            immer::map<long, std::shared_ptr<customer_t>> custs,
            long customerId, long id, reservation_type_t type);

// bool addReservation(std::map<long, reservation_t*>* tbl, long id, long num,
//                     long price);

/**
 * Constructor for manager objects
 */
manager_t::manager_t() {}

/**
 * Destructor
 *
 * [mfs] notes in the earlier code suggest that contents are not deleted
 *       here.  That's bad, but we can't fix it yet.
 */
manager_t::~manager_t() {}

/* =============================================================================
 * ADMINISTRATIVE INTERFACE
 * =============================================================================
 */

/* =============================================================================
 * addReservation
 * -- If 'num' > 0 then add, if < 0 remove
 * -- Adding 0 seats is error if does not exist
 * -- If 'price' < 0, do not update price
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] return value not used before, now indicationg aborts.

bool addReservation(
    WSTM::WAtomic& at,
    WSTM::WVar<immer::map<long, std::shared_ptr<reservation_t>>>& tbl, long id,
    long num, long price) {
  auto table = tbl.Get(at);
  auto reservation = table.find(id);

  bool success = true;
  if (reservation != nullptr) {
    /* Create new reservation */
    if (num < 1 || price < 0) {
      // return FALSE;
      return true;
    }

    table = table.insert(std::make_pair(
        id, std::make_shared<reservation_t>(id, num, price, &success)));
    tbl.Set(table, at);
  } else {
    /* Update existing reservation */
    //[wer210] there was aborts inside RESERVATION_ADD_TO_TOTAL, passing an
    // extra parameter.

    auto reservation_ptr = reservation->get();

    if (!reservation_ptr->addToTotal(at, num, &success)) {
      // return FALSE;
      if (success)
        return true;
      else
        return false;
    }

    if (reservation_ptr->numTotal.Get(at) == 0) {
      auto updated_table = table.erase(id);

      bool status = updated_table != table;
      if (status == false) {
        //_ITM_abortTransaction(2);
        return false;
      }

      tbl.Set(updated_table, at);
    } else {
      //[wer210] there was aborts inside RESERVATIOn_UPDATE_PRICE, and return
      // was not used
      if (!reservation_ptr->updatePrice(at, price)) {
        return false;
      }
    }
  }

  return true;
}

/* =============================================================================
 * manager_t::addCar
 * -- Add cars to a city
 * -- Adding to an existing car overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Return value not used before.
bool manager_t::addCar(WSTM::WAtomic& at, long carId, long numCars,
                       long price) {
  return addReservation(at, carTable, carId, numCars, price);
}

/* =============================================================================
 * manager_t::deleteCar
 * -- Delete cars from a city
 * -- Decreases available car count (those not allocated to a customer)
 * -- Fails if would make available car count negative
 * -- If decresed to 0, deletes entire entry
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::deleteCar(WSTM::WAtomic& at, long carId, long numCar) {
  /* -1 keeps old price */
  return addReservation(at, carTable, carId, -numCar, -1);
}

/* =============================================================================
 * manager_t::addRoom
 * -- Add rooms to a city
 * -- Adding to an existing room overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::addRoom(WSTM::WAtomic& at, long roomId, long numRoom,
                        long price) {
  return addReservation(at, roomTable, roomId, numRoom, price);
}

/* =============================================================================
 * manager_t::deleteRoom
 * -- Delete rooms from a city
 * -- Decreases available room count (those not allocated to a customer)
 * -- Fails if would make available room count negative
 * -- If decresed to 0, deletes entire entry
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::deleteRoom(WSTM::WAtomic& at, long roomId, long numRoom) {
  /* -1 keeps old price */
  return addReservation(at, roomTable, roomId, -numRoom, -1);
}

/* =============================================================================
 * manager_t::addFlight
 * -- Add seats to a flight
 * -- Adding to an existing flight overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, FALSE on failure
 * =============================================================================
 */
bool manager_t::addFlight(WSTM::WAtomic& at, long flightId, long numSeat,
                          long price) {
  return addReservation(at, flightTable, flightId, numSeat, price);
}

/* =============================================================================
 * manager_t::deleteFlight
 * -- Delete an entire flight
 * -- Fails if customer has reservation on this flight
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] return not used before, make it to indicate aborts
bool manager_t::deleteFlight(WSTM::WAtomic& at, long flightId) {
  auto table = flightTable.Get(at);
  auto res = table.find(flightId);

  if (res == nullptr) {
    return true;
  }

  auto reservation_ptr = res->get();

  if (reservation_ptr->numUsed.Get(at) > 0) {
    // return FALSE; /* somebody has a reservation */
    return true;
  }

  return addReservation(at, flightTable, flightId,
                        -1 * reservation_ptr->numTotal.Get(at),
                        -1 /* -1 keeps old price */);
}

/* =============================================================================
 * manager_t::addCustomer
 * -- If customer already exists, returns failure
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Function is called inside a transaction in client.c
//         But return value of this function was never used,
//         so I make it to return true all the time except when need to abort.
bool manager_t::addCustomer(WSTM::WAtomic& at, long customerId) {
  auto table = customerTable.Get(at);
  auto res = table.find(customerId);

  if (res != nullptr) {
    return true;
  }

  auto customerPtr = std::make_shared<customer_t>(customerId);
  assert(customerPtr != NULL);

  auto updated_table = table.insert(std::make_pair(customerId, customerPtr));
  if (updated_table == table) {
    //_ITM_abortTransaction(2);
    return false;
  }

  return true;
}

/* =============================================================================
 * manager_t::deleteCustomer
 * -- Delete this customer and associated reservations
 * -- If customer does not exist, returns success
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Again, the return values were not used (except for test cases below)
//         So I make it alway returning true, unless need to abort a
//         transaction.
bool manager_t::deleteCustomer(WSTM::WAtomic& at, long customerId) {
  immer::map<long, std::shared_ptr<reservation_t>> tables[NUM_RESERVATION_TYPE];
  auto customer_table = customerTable.Get(at);
  auto res = customer_table.find(customerId);
  if (res == nullptr) {
    return true;
  }

  tables[RESERVATION_CAR] = carTable.Get(at);
  tables[RESERVATION_ROOM] = roomTable.Get(at);
  tables[RESERVATION_FLIGHT] = flightTable.Get(at);

  /* Cancel this customer's reservations */
  for (auto i : res->get()->reservationInfoList.Get(at)) {
    auto reservation = tables[i.type].find(i.id);
    if (reservation == nullptr) {
      return false;
    }

    bool status = reservation->get()->cancel(at);
    if (status == false) {
      //_ITM_abortTransaction(2);
      return false;
    }
  }

  auto updated_customer_table = customer_table.erase(customerId);
  if (updated_customer_table == customer_table) {
    return false;
  }

  customerTable.Set(updated_customer_table, at);

  return true;
}

/* =============================================================================
 * QUERY INTERFACE
 * =============================================================================
 */

/* =============================================================================
 * queryNumFree
 * -- Return numFree of a reservation, -1 if failure
 * =============================================================================
 */

long queryNumFree(WSTM::WAtomic& at,
                  immer::map<long, std::shared_ptr<reservation_t>> tbl,
                  long id) {
  long numFree = -1;
  auto res = tbl.find(id);
  if (res != nullptr) {
    numFree = res->get()->numFree.Get(at);
  }
  return numFree;
}

/* =============================================================================
 * queryPrice
 * -- Return price of a reservation, -1 if failure
 * =============================================================================
 */

long queryPrice(WSTM::WAtomic& at,
                immer::map<long, std::shared_ptr<reservation_t>> tbl, long id) {
  long price = -1;
  auto res = tbl.find(id);
  if (res != nullptr) {
    price = res->get()->price;
  }
  return price;
}

/* =============================================================================
 * manager_t::queryCar
 * -- Return the number of empty seats on a car
 * -- Returns -1 if the car does not exist
 * =============================================================================
 */
long manager_t::queryCar(WSTM::WAtomic& at, long carId) {
  return queryNumFree(at, carTable.Get(at), carId);
}

/* =============================================================================
 * manager_t::queryCarPrice
 * -- Return the price of the car
 * -- Returns -1 if the car does not exist
 * =============================================================================
 */
long manager_t::queryCarPrice(WSTM::WAtomic& at, long carId) {
  return queryPrice(at, carTable.Get(at), carId);
}

/* =============================================================================
 * manager_t::queryRoom
 * -- Return the number of empty seats on a room
 * -- Returns -1 if the room does not exist
 * =============================================================================
 */
long manager_t::queryRoom(WSTM::WAtomic& at, long roomId) {
  return queryNumFree(at, roomTable.Get(at), roomId);
}

/* =============================================================================
 * manager_t::queryRoomPrice
 * -- Return the price of the room
 * -- Returns -1 if the room does not exist
 * =============================================================================
 */
long manager_t::queryRoomPrice(WSTM::WAtomic& at, long roomId) {
  return queryPrice(at, roomTable.Get(at), roomId);
}

/* =============================================================================
 * manager_t::queryFlight
 * -- Return the number of empty seats on a flight
 * -- Returns -1 if the flight does not exist
 * =============================================================================
 */
long manager_t::queryFlight(WSTM::WAtomic& at, long flightId) {
  return queryNumFree(at, flightTable.Get(at), flightId);
}

/* =============================================================================
 * manager_t::queryFlightPrice
 * -- Return the price of the flight
 * -- Returns -1 if the flight does not exist
 * =============================================================================
 */
long manager_t::queryFlightPrice(WSTM::WAtomic& at, long flightId) {
  return queryPrice(at, flightTable.Get(at), flightId);
}

/* =============================================================================
 * manager_t::queryCustomerBill
 * -- Return the total price of all reservations held for a customer
 * -- Returns -1 if the customer does not exist
 * =============================================================================
 */

long manager_t::queryCustomerBill(WSTM::WAtomic& at, long customerId) {
  long bill = -1;

  auto res = customerTable.Get(at).find(customerId);
  if (res != nullptr) {
    bill = res->get()->getBill(at);
  }
  return bill;
}

/* =============================================================================
 * RESERVATION INTERFACE
 * =============================================================================
 */

/* =============================================================================
 * reserve
 * -- Customer is not allowed to reserve same (type, id) multiple times
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Again, the original return values are not used. So I modified return
// values
// to indicate if should restart a transaction.

bool reserve(WSTM::WAtomic& at,
             immer::map<long, std::shared_ptr<reservation_t>> tbl,
             immer::map<long, std::shared_ptr<customer_t>> custs,
             long customerId, long id, reservation_type_t type) {
  auto cust = custs.find(customerId);
  if (cust == nullptr) {
    return true;
  }
  customer_t* customerPtr = cust->get();

  auto res = tbl.find(id);
  if (res == nullptr) {
    return true;
  }
  reservation_t* reservationPtr = res->get();

  if (!reservationPtr->make(at)) {
    // return FALSE;
    return true;
  }

  if (!customerPtr->addReservationInfo(at, type, id, reservationPtr->price)) {
    /* Undo previous successful reservation */
    bool status = reservationPtr->cancel(at);
    if (status == false) {
      //_ITM_abortTransaction(2);
      return false;
    }
    // return FALSE;
  }
  return true;
}

/* =============================================================================
 * manager_t::reserveCar
 * -- Returns failure if the car or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::reserveCar(WSTM::WAtomic& at, long customerId, long carId) {
  return reserve(at, carTable.Get(at), customerTable.Get(at), customerId, carId,
                 RESERVATION_CAR);
}

/* =============================================================================
 * manager_t::reserveRoom
 * -- Returns failure if the room or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::reserveRoom(WSTM::WAtomic& at, long customerId, long roomId) {
  return reserve(at, roomTable.Get(at), customerTable.Get(at), customerId,
                 roomId, RESERVATION_ROOM);
}

/* =============================================================================
 * manager_t::reserveFlight
 * -- Returns failure if the flight or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::reserveFlight(WSTM::WAtomic& at, long customerId,
                              long flightId) {
  return reserve(at, flightTable.Get(at), customerTable.Get(at), customerId,
                 flightId, RESERVATION_FLIGHT);
}

/* =============================================================================
 * cancel
 * -- Customer is not allowed to cancel multiple times
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] was a "static" function, invoked by three functions below
//         however, never called.

bool cancel(WSTM::WAtomic& at,
            immer::map<long, std::shared_ptr<reservation_t>> tbl,
            immer::map<long, std::shared_ptr<customer_t>> custs,
            long customerId, long id, reservation_type_t type) {
  auto cust = custs.find(customerId);
  if (cust == nullptr) {
    return false;
  }
  customer_t* customerPtr = cust->get();

  auto res = tbl.find(id);
  if (res == nullptr) {
    return false;
  }
  reservation_t* reservationPtr = res->get();

  if (!reservationPtr->cancel(at)) {
    return false;
  }

  if (!customerPtr->removeReservationInfo(at, type, id)) {
    /* Undo previous successful cancellation */
    bool status = reservationPtr->make(at);
    if (status == false) {
      //_ITM_abortTransaction(2);
      return false;
    }
    return false;
  }
  return true;
}

/* =============================================================================
 * manager_t::cancelCar
 * -- Returns failure if the car, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::cancelCar(WSTM::WAtomic& at, long customerId, long carId) {
  return cancel(at, carTable.Get(at), customerTable.Get(at), customerId, carId,
                RESERVATION_CAR);
}

/* =============================================================================
 * manager_t::cancelRoom
 * -- Returns failure if the room, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::cancelRoom(WSTM::WAtomic& at, long customerId, long roomId) {
  return cancel(at, roomTable.Get(at), customerTable.Get(at), customerId,
                roomId, RESERVATION_ROOM);
}

/* =============================================================================
 * manager_t::cancelFlight
 * -- Returns failure if the flight, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
bool manager_t::cancelFlight(WSTM::WAtomic& at, long customerId,
                             long flightId) {
  return cancel(at, flightTable.Get(at), customerTable.Get(at), customerId,
                flightId, RESERVATION_FLIGHT);
}

/* =============================================================================
 * TEST_MANAGER
 * =============================================================================
 */
#ifdef TEST_MANAGER

#include <cassert>
#include <cstdio>

int main() {
  manager_t::t* managerPtr;

  assert(memory_init(1, 4, 2));

  puts("Starting...");

  managerPtr = manager_t::alloc();

  /* Test administrative interface for cars */
  assert(!manager_addCar(managerPtr, 0, -1, 0)); /* negative num */
  assert(!manager_addCar(managerPtr, 0, 0, -1)); /* negative price */
  assert(!manager_addCar(managerPtr, 0, 0, 0));  /* zero num */
  assert(manager_addCar(managerPtr, 0, 1, 1));
  assert(!manager_deleteCar(managerPtr, 1, 0)); /* does not exist */
  assert(!manager_deleteCar(managerPtr, 0, 2)); /* cannot remove that many */
  assert(manager_addCar(managerPtr, 0, 1, 0));
  assert(manager_deleteCar(managerPtr, 0, 1));
  assert(manager_deleteCar(managerPtr, 0, 1));
  assert(!manager_deleteCar(managerPtr, 0, 1));  /* none left */
  assert(manager_queryCar(managerPtr, 0) == -1); /* does not exist */

  /* Test administrative interface for rooms */
  assert(!manager_addRoom(managerPtr, 0, -1, 0)); /* negative num */
  assert(!manager_addRoom(managerPtr, 0, 0, -1)); /* negative price */
  assert(!manager_addRoom(managerPtr, 0, 0, 0));  /* zero num */
  assert(manager_addRoom(managerPtr, 0, 1, 1));
  assert(!manager_deleteRoom(managerPtr, 1, 0)); /* does not exist */
  assert(!manager_deleteRoom(managerPtr, 0, 2)); /* cannot remove that many */
  assert(manager_addRoom(managerPtr, 0, 1, 0));
  assert(manager_deleteRoom(managerPtr, 0, 1));
  assert(manager_deleteRoom(managerPtr, 0, 1));
  assert(!manager_deleteRoom(managerPtr, 0, 1));  /* none left */
  assert(manager_queryRoom(managerPtr, 0) == -1); /* does not exist */

  /* Test administrative interface for flights and customers */
  assert(!manager_addFlight(managerPtr, 0, -1, 0)); /* negative num */
  assert(!manager_addFlight(managerPtr, 0, 0, -1)); /* negative price */
  assert(!manager_addFlight(managerPtr, 0, 0, 0));
  assert(manager_addFlight(managerPtr, 0, 1, 0));
  assert(!manager_deleteFlight(managerPtr, 1));    /* does not exist */
  assert(!manager_deleteFlight(managerPtr, 2));    /* cannot remove that many */
  assert(!manager_cancelFlight(managerPtr, 0, 0)); /* do not have reservation */
  assert(
      !manager_reserveFlight(managerPtr, 0, 0));  /* customer does not exist */
  assert(!manager_deleteCustomer(managerPtr, 0)); /* does not exist */
  assert(manager_addCustomer(managerPtr, 0));
  assert(!manager_addCustomer(managerPtr, 0)); /* already exists */
  assert(manager_reserveFlight(managerPtr, 0, 0));
  assert(manager_addFlight(managerPtr, 0, 1, 0));
  assert(!manager_deleteFlight(managerPtr, 0)); /* someone has reservation */
  assert(manager_cancelFlight(managerPtr, 0, 0));
  assert(manager_deleteFlight(managerPtr, 0));
  assert(!manager_deleteFlight(managerPtr, 0));     /* does not exist */
  assert(manager_queryFlight(managerPtr, 0) == -1); /* does not exist */
  assert(manager_deleteCustomer(managerPtr, 0));

  /* Test query interface for cars */
  assert(manager_addCustomer(managerPtr, 0));
  assert(manager_queryCar(managerPtr, 0) == -1);      /* does not exist */
  assert(manager_queryCarPrice(managerPtr, 0) == -1); /* does not exist */
  assert(manager_addCar(managerPtr, 0, 1, 2));
  assert(manager_queryCar(managerPtr, 0) == 1);
  assert(manager_queryCarPrice(managerPtr, 0) == 2);
  assert(manager_addCar(managerPtr, 0, 1, -1));
  assert(manager_queryCar(managerPtr, 0) == 2);
  assert(manager_reserveCar(managerPtr, 0, 0));
  assert(manager_queryCar(managerPtr, 0) == 1);
  assert(manager_deleteCar(managerPtr, 0, 1));
  assert(manager_queryCar(managerPtr, 0) == 0);
  assert(manager_queryCarPrice(managerPtr, 0) == 2);
  assert(manager_addCar(managerPtr, 0, 1, 1));
  assert(manager_queryCarPrice(managerPtr, 0) == 1);
  assert(manager_deleteCustomer(managerPtr, 0));
  assert(manager_queryCar(managerPtr, 0) == 2);
  assert(manager_deleteCar(managerPtr, 0, 2));

  /* Test query interface for rooms */
  assert(manager_addCustomer(managerPtr, 0));
  assert(manager_queryRoom(managerPtr, 0) == -1);      /* does not exist */
  assert(manager_queryRoomPrice(managerPtr, 0) == -1); /* does not exist */
  assert(manager_addRoom(managerPtr, 0, 1, 2));
  assert(manager_queryRoom(managerPtr, 0) == 1);
  assert(manager_queryRoomPrice(managerPtr, 0) == 2);
  assert(manager_addRoom(managerPtr, 0, 1, -1));
  assert(manager_queryRoom(managerPtr, 0) == 2);
  assert(manager_reserveRoom(managerPtr, 0, 0));
  assert(manager_queryRoom(managerPtr, 0) == 1);
  assert(manager_deleteRoom(managerPtr, 0, 1));
  assert(manager_queryRoom(managerPtr, 0) == 0);
  assert(manager_queryRoomPrice(managerPtr, 0) == 2);
  assert(manager_addRoom(managerPtr, 0, 1, 1));
  assert(manager_queryRoomPrice(managerPtr, 0) == 1);
  assert(manager_deleteCustomer(managerPtr, 0));
  assert(manager_queryRoom(managerPtr, 0) == 2);
  assert(manager_deleteRoom(managerPtr, 0, 2));

  /* Test query interface for flights */
  assert(manager_addCustomer(managerPtr, 0));
  assert(manager_queryFlight(managerPtr, 0) == -1);      /* does not exist */
  assert(manager_queryFlightPrice(managerPtr, 0) == -1); /* does not exist */
  assert(manager_addFlight(managerPtr, 0, 1, 2));
  assert(manager_queryFlight(managerPtr, 0) == 1);
  assert(manager_queryFlightPrice(managerPtr, 0) == 2);
  assert(manager_addFlight(managerPtr, 0, 1, -1));
  assert(manager_queryFlight(managerPtr, 0) == 2);
  assert(manager_reserveFlight(managerPtr, 0, 0));
  assert(manager_queryFlight(managerPtr, 0) == 1);
  assert(manager_addFlight(managerPtr, 0, 1, 1));
  assert(manager_queryFlightPrice(managerPtr, 0) == 1);
  assert(manager_deleteCustomer(managerPtr, 0));
  assert(manager_queryFlight(managerPtr, 0) == 3);
  assert(manager_deleteFlight(managerPtr, 0));

  /* Test query interface for customer bill */

  assert(manager_queryCustomerBill(managerPtr, 0) == -1); /* does not exist */
  assert(manager_addCustomer(managerPtr, 0));
  assert(manager_queryCustomerBill(managerPtr, 0) == 0);
  assert(manager_addCar(managerPtr, 0, 1, 1));
  assert(manager_addRoom(managerPtr, 0, 1, 2));
  assert(manager_addFlight(managerPtr, 0, 1, 3));

  assert(manager_reserveCar(managerPtr, 0, 0));
  assert(manager_queryCustomerBill(managerPtr, 0) == 1);
  assert(!manager_reserveCar(managerPtr, 0, 0));
  assert(manager_queryCustomerBill(managerPtr, 0) == 1);
  assert(manager_addCar(managerPtr, 0, 0, 2));
  assert(manager_queryCar(managerPtr, 0) == 0);
  assert(manager_queryCustomerBill(managerPtr, 0) == 1);

  assert(manager_reserveRoom(managerPtr, 0, 0));
  assert(manager_queryCustomerBill(managerPtr, 0) == 3);
  assert(!manager_reserveRoom(managerPtr, 0, 0));
  assert(manager_queryCustomerBill(managerPtr, 0) == 3);
  assert(manager_addRoom(managerPtr, 0, 0, 2));
  assert(manager_queryRoom(managerPtr, 0) == 0);
  assert(manager_queryCustomerBill(managerPtr, 0) == 3);

  assert(manager_reserveFlight(managerPtr, 0, 0));
  assert(manager_queryCustomerBill(managerPtr, 0) == 6);
  assert(!manager_reserveFlight(managerPtr, 0, 0));
  assert(manager_queryCustomerBill(managerPtr, 0) == 6);
  assert(manager_addFlight(managerPtr, 0, 0, 2));
  assert(manager_queryFlight(managerPtr, 0) == 0);
  assert(manager_queryCustomerBill(managerPtr, 0) == 6);

  assert(manager_deleteCustomer(managerPtr, 0));
  assert(manager_deleteCar(managerPtr, 0, 1));
  assert(manager_deleteRoom(managerPtr, 0, 1));
  assert(manager_deleteFlight(managerPtr, 0));

  /* Test reservation interface */

  assert(manager_addCustomer(managerPtr, 0));
  assert(manager_queryCustomerBill(managerPtr, 0) == 0);
  assert(manager_addCar(managerPtr, 0, 1, 1));
  assert(manager_addRoom(managerPtr, 0, 1, 2));
  assert(manager_addFlight(managerPtr, 0, 1, 3));

  assert(!manager_cancelCar(managerPtr, 0, 0)); /* do not have reservation */
  assert(manager_reserveCar(managerPtr, 0, 0));
  assert(manager_queryCar(managerPtr, 0) == 0);
  assert(manager_cancelCar(managerPtr, 0, 0));
  assert(manager_queryCar(managerPtr, 0) == 1);

  assert(!manager_cancelRoom(managerPtr, 0, 0)); /* do not have reservation */
  assert(manager_reserveRoom(managerPtr, 0, 0));
  assert(manager_queryRoom(managerPtr, 0) == 0);
  assert(manager_cancelRoom(managerPtr, 0, 0));
  assert(manager_queryRoom(managerPtr, 0) == 1);

  assert(!manager_cancelFlight(managerPtr, 0, 0)); /* do not have reservation */
  assert(manager_reserveFlight(managerPtr, 0, 0));
  assert(manager_queryFlight(managerPtr, 0) == 0);
  assert(manager_cancelFlight(managerPtr, 0, 0));
  assert(manager_queryFlight(managerPtr, 0) == 1);

  assert(manager_deleteCar(managerPtr, 0, 1));
  assert(manager_deleteRoom(managerPtr, 0, 1));
  assert(manager_deleteFlight(managerPtr, 0));
  assert(manager_deleteCustomer(managerPtr, 0));

  manager_free(managerPtr);

  puts("All tests passed.");

  return 0;
}

#endif /* TEST_MANAGER */

/* =============================================================================
 *
 * End of manager.c
 *
 * =============================================================================
 */
