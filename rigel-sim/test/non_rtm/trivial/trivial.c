#include <assert.h>
#include <rigel.h>
#include <rigel-tasks.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// threads_trivial_test.c
////////////////////////////////////////////////////////////////////////////////
// A simple test for checking if something is totally busted 
////////////////////////////////////////////////////////////////////////////////

#if 0
#define BARRIER_INFO  RigelBarrier_Info
#define BARRIER_INIT  RigelBarrier_Init
#define BARRIER_ENTER RigelBarrier_EnterFull
#endif

#if 1 
#define BARRIER_INFO  RigelBarrierMT_Info
#define BARRIER_INIT  RigelBarrierMT_Init
#define BARRIER_ENTER RigelBarrierMT_EnterFull
#endif

ATOMICFLAG_INIT_CLEAR(Init_Flag);

////////////////////////////////////////////////////////////////////////////////
int main() {

  SIM_SLEEP_OFF();

  int corenum   = RigelGetCoreNum();
  int threadnum = RigelGetThreadNum();
	int numcores  = RigelGetNumCores();
  int numthreads = RigelGetNumThreads();
  int th_per_core    = RigelGetNumThreadsPerCore();
  int th_per_cluster = RigelGetNumThreadsPerCluster();

  // setup on thread 0
  if(threadnum == 0) {
    RigelPrint(corenum);
    RigelPrint(threadnum);
    RigelPrint(numcores);
    RigelPrint(numthreads);
    RigelPrint(th_per_core);
    RigelPrint(th_per_cluster);
    atomic_flag_set(&Init_Flag);
  }

  // wait for core 0 to finish setup
  atomic_flag_spin_until_set(&Init_Flag);

  return 1;
}
////////////////////////////////////////////////////////////////////////////////


