/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <wstm/stm.h>

#include <cassert>
#include <random>

#include "action.h"
#include "manager.h"
#include "splittable/mrv/mrv_flex_vector.hpp"
#include "splittable/pr/pr_array.hpp"
#include "splittable/single/single.hpp"
#include "thread.h"
#include "utility.h"

template <typename S>
struct client_t {
  long id;
  manager_t<S>* managerPtr;
  std::mt19937 randomPtr;
  long numOperation;
  long numQueryPerTransaction;
  long queryRange;
  long percentUser;

  client_t(long id, manager_t<S>* managerPtr, long numOperation,
           long numQueryPerTransaction, long queryRange, long percentUser);

  // NB: no need for explicit destructor, since the managerPtr is shared
  // among clients
};

/* =============================================================================
 * selectAction
 * =============================================================================
 */
static action_t selectAction(long r, long percentUser) {
  action_t action;

  if (r < percentUser) {
    action = ACTION_MAKE_RESERVATION;
  } else if (r & 1) {
    action = ACTION_DELETE_CUSTOMER;
  } else {
    action = ACTION_UPDATE_TABLES;
  }

  return action;
}

/* =============================================================================
 * client_run
 * -- Execute list operations on the database
 * =============================================================================
 */
template <typename S>
void client_run(void* argPtr) {
  long myId = thread_getId<S>();
  client_t<S>* clientPtr = ((client_t<S>**)argPtr)[myId];

  manager_t<S>* managerPtr = clientPtr->managerPtr;
  std::mt19937& randomPtr = clientPtr->randomPtr;

  long numOperation = clientPtr->numOperation;
  long numQueryPerTransaction = clientPtr->numQueryPerTransaction;
  long queryRange = clientPtr->queryRange;
  long percentUser = clientPtr->percentUser;

  long* types = new long[numQueryPerTransaction];
  long* ids = new long[numQueryPerTransaction];
  long* ops = new long[numQueryPerTransaction];
  long* prices = new long[numQueryPerTransaction];

  for (long i = 0; i < numOperation; i++) {
    long r = randomPtr() % 100;
    action_t action = selectAction(r, percentUser);

    switch (action) {
      case ACTION_MAKE_RESERVATION: {
        long maxPrices[NUM_RESERVATION_TYPE] = {-1, -1, -1};
        long maxIds[NUM_RESERVATION_TYPE] = {-1, -1, -1};
        long n;
        long numQuery = randomPtr() % numQueryPerTransaction + 1;
        long customerId = randomPtr() % queryRange + 1;
        for (n = 0; n < numQuery; n++) {
          types[n] = randomPtr() % NUM_RESERVATION_TYPE;
          ids[n] = (randomPtr() % queryRange) + 1;
        }
        bool isFound = false;
        bool done = false;
        //[wer210] I modified here to remove _ITM_abortTransaction().
        while (!done) {
          try {
            WSTM::Atomically([&](WSTM::WAtomic& at) {
              done = true;

              for (n = 0; n < numQuery; n++) {
                long t = types[n];
                long id = ids[n];
                long price = -1;
                switch (t) {
                  case RESERVATION_CAR:
                    if (managerPtr->queryCar(at, id) >= 0) {
                      price = managerPtr->queryCarPrice(at, id);
                    }
                    break;
                  case RESERVATION_FLIGHT:
                    if (managerPtr->queryFlight(at, id) >= 0) {
                      price = managerPtr->queryFlightPrice(at, id);
                    }
                    break;
                  case RESERVATION_ROOM:
                    if (managerPtr->queryRoom(at, id) >= 0) {
                      price = managerPtr->queryRoomPrice(at, id);
                    }
                    break;
                  default:
                    assert(0);
                }
                //[wer210] read-only above
                if (price > maxPrices[t]) {
                  maxPrices[t] = price;
                  maxIds[t] = id;
                  isFound = true;
                }
              } /* for n */

              if (isFound) {
                done = done && managerPtr->addCustomer(at, customerId);
              }

              if (maxIds[RESERVATION_CAR] > 0) {
                done = done && managerPtr->reserveCar(at, customerId,
                                                      maxIds[RESERVATION_CAR]);
              }
              if (maxIds[RESERVATION_FLIGHT] > 0) {
                done = done && managerPtr->reserveFlight(
                                   at, customerId, maxIds[RESERVATION_FLIGHT]);
              }
              if (maxIds[RESERVATION_ROOM] > 0) {
                done = done && managerPtr->reserveRoom(
                                   at, customerId, maxIds[RESERVATION_ROOM]);
              }

              if (!done) {
                // assert(0);
                throw cancel_transaction();
              }
            });
          } catch (cancel_transaction const&) {
            done = false;
          }
        }
        break;
      }

      case ACTION_DELETE_CUSTOMER: {
        long customerId = randomPtr() % queryRange + 1;
        bool done = false;
        while (!done) {
          try {
            WSTM::Atomically([&](WSTM::WAtomic& at) {
              done = true;
              long bill = managerPtr->queryCustomerBill(at, customerId);
              if (bill >= 0) {
                done = done && managerPtr->deleteCustomer(at, customerId);
              }
              if (!done) {
                // assert(0);
                throw cancel_transaction();
              }
            });
          } catch (cancel_transaction const&) {
            done = false;
          }
        }
        break;
      }

      case ACTION_UPDATE_TABLES: {
        long numUpdate = randomPtr() % numQueryPerTransaction + 1;
        long n;
        for (n = 0; n < numUpdate; n++) {
          types[n] = randomPtr() % NUM_RESERVATION_TYPE;
          ids[n] = (randomPtr() % queryRange) + 1;
          ops[n] = randomPtr() % 2;
          if (ops[n]) {
            prices[n] = ((randomPtr() % 5) * 10) + 50;
          }
        }
        bool done = false;
        while (!done) {
          try {
            WSTM::Atomically([&](WSTM::WAtomic& at) {
              done = true;
              for (n = 0; n < numUpdate; n++) {
                long t = types[n];
                long id = ids[n];
                long doAdd = ops[n];
                if (doAdd) {
                  long newPrice = prices[n];
                  switch (t) {
                    case RESERVATION_CAR:
                      done = done && managerPtr->addCar(at, id, 100, newPrice);
                      break;
                    case RESERVATION_FLIGHT:
                      done =
                          done && managerPtr->addFlight(at, id, 100, newPrice);
                      break;
                    case RESERVATION_ROOM:
                      done = done && managerPtr->addRoom(at, id, 100, newPrice);
                      break;
                    default:
                      assert(0);
                  }
                } else { /* do delete */
                  switch (t) {
                    case RESERVATION_CAR:
                      done = done && managerPtr->deleteCar(at, id, 100);
                      break;
                    case RESERVATION_FLIGHT:
                      done = done && managerPtr->deleteFlight(at, id);
                      break;
                    case RESERVATION_ROOM:
                      done = done && managerPtr->deleteRoom(at, id, 100);
                      break;
                    default:
                      assert(0);
                  }
                }
              }
              if (!done) {
                // assert(0);
                throw cancel_transaction();
              }
            });
          } catch (cancel_transaction const&) {
            done = false;
          }
        }
        break;
      }

      default:
        assert(0);

    } /* switch (action) */

  } /* for i */

  delete[] types;
  delete[] ids;
  delete[] ops;
  delete[] prices;
}
