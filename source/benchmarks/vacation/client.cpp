/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include "splittable/benchmarks/vacation/client.h"

// explicit instantiations
template struct client_t<splittable::single::single>;
template struct client_t<splittable::mrv::mrv_flex_vector>;
template struct client_t<splittable::pr::pr_array>;

/* =============================================================================
 * client_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
template <typename S>
client_t<S>::client_t(long _id, manager_t<S>* _managerPtr, long _numOperation,
                      long _numQueryPerTransaction, long _queryRange,
                      long _percentUser) {
  id = _id;
  managerPtr = _managerPtr;
  randomPtr.seed(id);
  numOperation = _numOperation;
  numQueryPerTransaction = _numQueryPerTransaction;
  queryRange = _queryRange;
  percentUser = _percentUser;
}
