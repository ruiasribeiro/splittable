/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <cassert>
#include <random>

#include "action.h"
#include "manager.h"
#include "thread.h"
#include "utility.h"
#include "wstm/stm.h"

struct client_t {
  long id;
  manager_t* managerPtr;
  std::mt19937 randomPtr;
  long numOperation;
  long numQueryPerTransaction;
  long queryRange;
  long percentUser;

  client_t(long id, manager_t* managerPtr, long numOperation,
           long numQueryPerTransaction, long queryRange, long percentUser);

  // NB: no need for explicit destructor, since the managerPtr is shared
  // among clients
};

/*
 * client_run
 * -- Execute list operations on the database
 */
void client_run(void* argPtr);
