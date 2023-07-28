/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <getopt.h>
#include <wstm/stm.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>

#include "splittable/benchmarks/vacation/client.h"
#include "splittable/benchmarks/vacation/manager.h"
#include "splittable/benchmarks/vacation/thread.h"
#include "splittable/benchmarks/vacation/timer.h"
#include "splittable/benchmarks/vacation/utility.h"

enum param_types {
  PARAM_CLIENTS = (unsigned char)'t',
  PARAM_NUMBER = (unsigned char)'n',
  PARAM_QUERIES = (unsigned char)'q',
  PARAM_RELATIONS = (unsigned char)'r',
  PARAM_SPLITTABLE_TYPE = (unsigned char)'s',
  PARAM_TRANSACTIONS = (unsigned char)'T',
  PARAM_USER = (unsigned char)'u'
};

#define PARAM_DEFAULT_CLIENTS (1)
#define PARAM_DEFAULT_NUMBER (4)
#define PARAM_DEFAULT_QUERIES (60)
#define PARAM_DEFAULT_RELATIONS (1 << 20)
#define PARAM_DEFAULT_SPLITTABLE_TYPE "mrv-flex-vector"
#define PARAM_DEFAULT_TRANSACTIONS (1 << 22)
#define PARAM_DEFAULT_USER (90)

double global_params[256]; /* 256 = ascii limit */
std::string global_splittable_type;

pthread_barrier_t* global_barrierPtr;

/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void displayUsage(const char* appName) {
  printf("Usage: %s [options]\n", appName);
  puts("\nOptions:                                             (defaults)\n");
  printf("    t <UINT>   Number of clien[t]s ([t]hreads)       (%i)\n",
         PARAM_DEFAULT_CLIENTS);
  printf("    n <UINT>   [n]umber of user queries/transaction  (%i)\n",
         PARAM_DEFAULT_NUMBER);
  printf("    q <UINT>   Percentage of relations [q]ueried     (%i)\n",
         PARAM_DEFAULT_QUERIES);
  printf("    r <UINT>   Number of possible [r]elations        (%i)\n",
         PARAM_DEFAULT_RELATIONS);
  printf("    s <STR>    Type of [s]plittable value to use     (%s)\n",
         PARAM_DEFAULT_SPLITTABLE_TYPE);
  printf("    T <UINT>   Number of [T]ransactions              (%i)\n",
         PARAM_DEFAULT_TRANSACTIONS);
  printf("    u <UINT>   Percentage of [u]ser transactions     (%i)\n",
         PARAM_DEFAULT_USER);
  exit(1);
}

/* =============================================================================
 * setDefaultParams
 * =============================================================================
 */
static void setDefaultParams() {
  global_params[PARAM_CLIENTS] = PARAM_DEFAULT_CLIENTS;
  global_params[PARAM_NUMBER] = PARAM_DEFAULT_NUMBER;
  global_params[PARAM_QUERIES] = PARAM_DEFAULT_QUERIES;
  global_params[PARAM_RELATIONS] = PARAM_DEFAULT_RELATIONS;
  global_splittable_type = PARAM_DEFAULT_SPLITTABLE_TYPE;
  global_params[PARAM_TRANSACTIONS] = PARAM_DEFAULT_TRANSACTIONS;
  global_params[PARAM_USER] = PARAM_DEFAULT_USER;
}

/* =============================================================================
 * parseArgs
 * =============================================================================
 */
static void parseArgs(long argc, char* const argv[]) {
  long i;
  long opt;

  opterr = 0;

  setDefaultParams();

  while ((opt = getopt(argc, argv, "t:n:q:r:s:T:u:L")) != -1) {
    switch (opt) {
      case 'T':
      case 'n':
      case 'q':
      case 'r':
      case 't':
      case 'u':
        global_params[(unsigned char)opt] = atol(optarg);
        break;
      case 'L':
        global_params[PARAM_NUMBER] = 2;
        global_params[PARAM_QUERIES] = 90;
        global_params[PARAM_USER] = 98;
        break;
      case 's': {
        std::string type(optarg);
        if (type == "single" || type == "mrv-flex-vector" ||
            type == "pr-array") {
          global_splittable_type = type;
        } else {
          fprintf(stderr,
                  "Could not find a benchmark with name %s; try \"single\", "
                  "\"mrv-flex-vector\", \"pr-array\"\n",
                  type.c_str());
          opterr++;
        }
        break;
      }
      case '?':
      default:
        opterr++;
        break;
    }
  }

  for (i = optind; i < argc; i++) {
    fprintf(stderr, "Non-option argument: %s\n", argv[i]);
    opterr++;
  }

  if (opterr) {
    displayUsage(argv[0]);
  }
}

/* =============================================================================
 * addCustomer
 * -- Wrapper function
 * =============================================================================
 */
template <typename S>
static bool addCustomer(manager_t<S>* managerPtr, long id, long, long) {
  return WSTM::Atomically([=](WSTM::WAtomic& at) -> bool {
    return managerPtr->addCustomer(at, id);
  });
}

/* Three quick-and-dirty functions so that we don't need method pointers in the
 * following code */
template <typename S>
bool manager_addCar(manager_t<S>* m, long a, long b, long c) {
  return WSTM::Atomically(
      [=](WSTM::WAtomic& at) -> bool { return m->addCar(at, a, b, c); });
}

template <typename S>
bool manager_addFlight(manager_t<S>* m, long a, long b, long c) {
  return WSTM::Atomically(
      [=](WSTM::WAtomic& at) -> bool { return m->addFlight(at, a, b, c); });
}

template <typename S>
bool manager_addRoom(manager_t<S>* m, long a, long b, long c) {
  return WSTM::Atomically(
      [=](WSTM::WAtomic& at) -> bool { return m->addRoom(at, a, b, c); });
}

/* =============================================================================
 * initializeManager
 * =============================================================================
 */
template <typename S>
static manager_t<S>* initializeManager() {
  manager_t<S>* managerPtr;
  long i;
  long numRelation;
  std::mt19937 randomPtr;
  long* ids;
  bool (*manager_add[])(manager_t<S>*, long, long, long) = {
      &manager_addCar, &manager_addFlight, &manager_addRoom, &addCustomer};
  long t;
  long numTable = sizeof(manager_add) / sizeof(manager_add[0]);

  managerPtr = new manager_t<S>();
  assert(managerPtr != NULL);

  numRelation = (long)global_params[PARAM_RELATIONS];
  ids = (long*)malloc(numRelation * sizeof(long));
  for (i = 0; i < numRelation; i++) {
    ids[i] = i + 1;
  }

  for (t = 0; t < numTable; t++) {
    /* Shuffle ids */
    for (i = 0; i < numRelation; i++) {
      long x = randomPtr() % numRelation;
      long y = randomPtr() % numRelation;
      long tmp = ids[x];
      ids[x] = ids[y];
      ids[y] = tmp;
    }

    /* Populate table */
    for (i = 0; i < numRelation; i++) {
      bool status;
      long id = ids[i];
      long num = ((randomPtr() % 5) + 1) * 100;
      long price = ((randomPtr() % 5) * 10) + 50;
      status = manager_add[t](managerPtr, id, num, price);
      assert(status);
    }

  } /* for t */

  free(ids);

  return managerPtr;
}

/* =============================================================================
 * initializeClients
 * =============================================================================
 */
template <typename S>
static client_t<S>** initializeClients(manager_t<S>* managerPtr) {
  client_t<S>** clients;
  long i;
  long numClient = (long)global_params[PARAM_CLIENTS];
  long numTransaction = (long)global_params[PARAM_TRANSACTIONS];
  long numTransactionPerClient;
  long numQueryPerTransaction = (long)global_params[PARAM_NUMBER];
  long numRelation = (long)global_params[PARAM_RELATIONS];
  long percentQuery = (long)global_params[PARAM_QUERIES];
  long queryRange;
  long percentUser = (long)global_params[PARAM_USER];

  clients = (client_t<S>**)malloc(numClient * sizeof(client_t<S>*));
  assert(clients != NULL);
  numTransactionPerClient =
      (long)((double)numTransaction / (double)numClient + 0.5);
  queryRange = (long)((double)percentQuery / 100.0 * (double)numRelation + 0.5);

  for (i = 0; i < numClient; i++) {
    clients[i] = new client_t(i, managerPtr, numTransactionPerClient,
                              numQueryPerTransaction, queryRange, percentUser);
    assert(clients[i] != NULL);
  }

  return clients;
}

/* =============================================================================
 * checkTables
 * -- some simple checks (not comprehensive)
 * -- dependent on tasks generated for clients in initializeClients()
 * =============================================================================
 */
template <typename S>
static void checkTables(manager_t<S>* managerPtr) {
  long i;
  long numRelation = (long)global_params[PARAM_RELATIONS];

  auto custs = managerPtr->customerTable.GetReadOnly();
  WSTM::WVar<immer::map<long, std::shared_ptr<reservation_t<S>>>>* tbls[] = {
      &managerPtr->carTable,
      &managerPtr->flightTable,
      &managerPtr->roomTable,
  };

  long numTable = sizeof(tbls) / sizeof(tbls[0]);
  bool (*manager_add[])(manager_t<S>*, long, long, long) = {
      &manager_addCar, &manager_addFlight, &manager_addRoom};
  long t;

  /* Check for unique customer IDs */
  long percentQuery = (long)global_params[PARAM_QUERIES];
  long queryRange =
      (long)((double)percentQuery / 100.0 * (double)numRelation + 0.5);
  long maxCustomerId = queryRange + 1;
  for (i = 1; i <= maxCustomerId; i++) {
    auto f = custs.find(i);
    if (f != nullptr) {
      auto updated_custs = custs.erase(i);

      if (updated_custs != custs) {
        auto c = custs.find(i);
        assert(c == nullptr);
      }

      custs = updated_custs;
    }
  }

  /* Check reservation tables for consistency and unique ids */
  for (t = 0; t < numTable; t++) {
    for (i = 1; i <= numRelation; i++) {
      WSTM::Atomically([&](WSTM::WAtomic& at) {
        auto table = tbls[t]->Get(at);
        auto f = table.find(i);
        if (f != nullptr) {
          assert(manager_add[t](managerPtr, i, 0, 0)); /* validate entry */

          auto updated_table = table.erase(i);
          if (updated_table != table) {
            auto second_updated_table = updated_table.erase(i);
            assert(second_updated_table == updated_table);
            tbls[t]->Set(second_updated_table, at);
          } else {
            tbls[t]->Set(updated_table, at);
          }
        }
      });
    }
  }
}

/* =============================================================================
 * freeClients
 * =============================================================================
 */
template <typename S>
static void freeClients(client_t<S>** clients) {
  long numClient = (long)global_params[PARAM_CLIENTS];

  for (long i = 0; i < numClient; i++) {
    client_t<S>* clientPtr = clients[i];
    delete clientPtr;
  }
}

/* =============================================================================
 * main
 * =============================================================================
 */
template <typename S>
int templated_main() {
  manager_t<S>* managerPtr;
  client_t<S>** clients;
  TIMER_T start;
  TIMER_T stop;

  /* Initialization */
  managerPtr = initializeManager<S>();
  assert(managerPtr != NULL);
  clients = initializeClients(managerPtr);
  assert(clients != NULL);
  long numThread = global_params[PARAM_CLIENTS];
  thread_startup<S>(numThread);

  /* Run transactions */
  S::reset_global_stats();
  TIMER_READ(start);
  thread_start<S>(client_run<S>, (void*)clients);
  TIMER_READ(stop);
  auto stats = splittable::splittable::get_global_stats();
  auto avg_adjust_interval = S::get_avg_adjust_interval();
  auto avg_balance_interval = S::get_avg_balance_interval();
  auto avg_phase_interval = S::get_avg_phase_interval();

  checkTables<S>(managerPtr);

  // benchmark, workers, execution time (s), abort rate, avg adjust interval
  // (ms), avg balance interval (ms), avg phase interval (ms)
  auto abort_rate =
      static_cast<double>(stats.aborts) / (stats.aborts + stats.commits);
  std::cout << global_splittable_type << "," << numThread << ","
            << TIMER_DIFF_SECONDS(start, stop) << "," << abort_rate << ","
            << avg_adjust_interval.count() / 1000000.0 << ","
            << avg_balance_interval.count() / 1000000.0 << ","
            << avg_phase_interval.count() / 1000000.0 << "\n";

  /* Clean up */
  freeClients(clients);
  /*
   * TODO: The contents of the manager's table need to be deallocated.
   */
  delete managerPtr;

  thread_shutdown<S>();

  quick_exit(0);  // allows the program to exit without cleaning its resources
                  // correctly
}

int main(int argc, char** argv) {
  parseArgs(argc, argv);

  if (global_splittable_type == "single") {
    return templated_main<splittable::single::single>();
  }

  if (global_splittable_type == "mrv-flex-vector") {
    return templated_main<splittable::mrv::mrv_flex_vector>();
  }

  if (global_splittable_type == "pr-array") {
    return templated_main<splittable::pr::pr_array>();
  }

  // Should be impossible to reach here.
  assert(false);
}
