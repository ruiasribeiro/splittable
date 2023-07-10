/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * manager.h: Travel reservation resource manager
 */

#pragma once

#include <memory>

#include "customer.h"
#include "immer/map.hpp"
#include "reservation.h"
#include "utility.h"
#include "wstm/stm.h"

struct manager_t {
  WSTM::WVar<immer::map<long, std::shared_ptr<reservation_t<splittable_type>>>> carTable;
  WSTM::WVar<immer::map<long, std::shared_ptr<reservation_t<splittable_type>>>> roomTable;
  WSTM::WVar<immer::map<long, std::shared_ptr<reservation_t<splittable_type>>>> flightTable;
  WSTM::WVar<immer::map<long, std::shared_ptr<customer_t>>> customerTable;

  manager_t();
  ~manager_t();

  /*
   * ADMINISTRATIVE INTERFACE
   */

  /*
   * addCar
   * -- Add cars to a city
   * -- Adding to an existing car overwrite the price if 'price' >= 0
   * -- Returns TRUE on success, else FALSE
   */
  bool addCar(WSTM::WAtomic& at, long carId, long numCar, long price);

  /*
   * deleteCar
   * -- Delete cars from a city
   * -- Decreases available car count (those not allocated to a customer)
   * -- Fails if would make available car count negative
   * -- If decresed to 0, deletes entire entry
   * -- Returns TRUE on success, else FALSE
   */
  bool deleteCar(WSTM::WAtomic& at, long carId, long numCar);

  /*
   * addRoom
   * -- Add rooms to a city
   * -- Adding to an existing room overwrites the price if 'price' >= 0
   * -- Returns TRUE on success, else FALSE
   */
  bool addRoom(WSTM::WAtomic& at, long roomId, long numRoom, long price);

  /*
   * deleteRoom
   * -- Delete rooms from a city
   * -- Decreases available room count (those not allocated to a customer)
   * -- Fails if would make available room count negative
   * -- If decresed to 0, deletes entire entry
   * -- Returns TRUE on success, else FALSE
   */
  bool deleteRoom(WSTM::WAtomic& at, long roomId, long numRoom);

  /*
   * addFlight
   * -- Add seats to a flight
   * -- Adding to an existing flight overwrites the price if 'price' >= 0
   * -- Returns TRUE on success, FALSE on failure
   */
  bool addFlight(WSTM::WAtomic& at, long flightId, long numSeat, long price);

  /*
   * deleteFlight
   * -- Delete an entire flight
   * -- Fails if customer has reservation on this flight
   * -- Returns TRUE on success, else FALSE
   */
  bool deleteFlight(WSTM::WAtomic& at, long flightId);

  /*
   * addCustomer
   * -- If customer already exists, returns success
   * -- Returns TRUE on success, else FALSE
   */
  bool addCustomer(WSTM::WAtomic& at, long customerId);

  /*
   * deleteCustomer
   * -- Delete this customer and associated reservations
   * -- If customer does not exist, returns success
   * -- Returns TRUE on success, else FALSE
   */
  bool deleteCustomer(WSTM::WAtomic& at, long customerId);

  /*
   * QUERY INTERFACE
   */

  /*
   * queryCar
   * -- Return the number of empty seats on a car
   * -- Returns -1 if the car does not exist
   */
  long queryCar(WSTM::WAtomic& at, long carId);

  /*
   * queryCarPrice
   * -- Return the price of the car
   * -- Returns -1 if the car does not exist
   */
  long queryCarPrice(WSTM::WAtomic& at, long carId);

  /*
   * queryRoom
   * -- Return the number of empty seats on a room
   * -- Returns -1 if the room does not exist
   */
  long queryRoom(WSTM::WAtomic& at, long roomId);

  /*
   * queryRoomPrice
   * -- Return the price of the room
   * -- Returns -1 if the room does not exist
   */
  long queryRoomPrice(WSTM::WAtomic& at, long roomId);

  /*
   * queryFlight
   * -- Return the number of empty seats on a flight
   * -- Returns -1 if the flight does not exist
   */
  long queryFlight(WSTM::WAtomic& at, long flightId);

  /*
   * queryFlightPrice
   * -- Return the price of the flight
   * -- Returns -1 if the flight does not exist
   */
  long queryFlightPrice(WSTM::WAtomic& at, long flightId);

  /*
   * queryCustomerBill
   * -- Return the total price of all reservations held for a customer
   * -- Returns -1 if the customer does not exist
   */
  long queryCustomerBill(WSTM::WAtomic& at, long customerId);

  /*
   * RESERVATION INTERFACE
   */

  /*
   * reserveCar
   * -- Returns failure if the car or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  bool reserveCar(WSTM::WAtomic& at, long customerId, long carId);

  /*
   * reserveRoom
   * -- Returns failure if the room or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  bool reserveRoom(WSTM::WAtomic& at, long customerId, long roomId);

  /*
   * reserveFlight
   * -- Returns failure if the flight or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  bool reserveFlight(WSTM::WAtomic& at, long customerId, long flightId);

  /*
   * cancelCar
   * -- Returns failure if the car, reservation, or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  bool cancelCar(WSTM::WAtomic& at, long customerId, long carId);

  /*
   * cancelRoom
   * -- Returns failure if the room, reservation, or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  bool cancelRoom(WSTM::WAtomic& at, long customerId, long roomId);

  /*
   * cancelFlight
   * -- Returns failure if the flight, reservation, or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  bool cancelFlight(WSTM::WAtomic& at, long customerId, long flightId);
};
