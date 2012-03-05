#include "rigellib.h"

namespace rigel {
  const char * sprf_strings[SPRF_NUM_SREGS] = {
      "CORE_NUM",                   // 0
      "CORES_TOTAL",                // 1
      "CORES_PER_CLUSTER",          // 2
      "THREADS_PER_CLUSTER",        // 3
      "CLUSTER_ID",                 // 4
      "NUM_CLUSTERS",               // 5
      "THREAD_NUM",                 // 6
      "THREADS_TOTAL",              // 7
      "THREADS_PER_CORE",           // 8
      "SLEEP",                      // 9
      "CURR_CYCLE_LOW",             // 10
      "CURR_CYCLE_HIGH",            // 11
      "ENABLE_INCOHERENT_MALLOC",   // 12
      "HYBRIDCC_BASEADDR",          // 13
      "NONBLOCKING_ATOMICS",        // 14
      "STACKBASE_ADDR",             // 15
      "IS_SIM",                     // 16
      "ARGC",                       // 17
      "ARGV"                        // 18
    };
 } 
