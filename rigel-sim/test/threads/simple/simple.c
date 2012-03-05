#include <assert.h>
#include <rigel.h>
#include <rigel-tasks.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// threads_simple_test.c
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

#define VECT_SIZE  16 
ATOMICFLAG_INIT_CLEAR(Init_Flag);
BARRIER_INFO bi __attribute__ ((aligned (32)));

int A[VECT_SIZE];

////////////////////////////////////////////////////////////////////////////////
int main() {

  int i; 
  int corenum    = RigelGetCoreNum();
  int threadnum  = RigelGetThreadNum();
 	int numcores   = RigelGetNumCores();
  int numthreads = RigelGetNumThreads();
  int th_per_core    = RigelGetNumThreadsPerCore();
  int th_per_cluster = RigelGetNumThreadsPerCluster();

  //int sense = 0;

  // setup on thread 0
  if(threadnum == 0) {
    // RigelPrint(corenum);
    // RigelPrint(threadnum);
    // RigelPrint(numcores);
    // RigelPrint(numthreads);
    // RigelPrint(th_per_core);
    // RigelPrint(th_per_cluster);
    BARRIER_INIT(&bi);
    atomic_flag_set(&Init_Flag);
  }

  // wait for core 0 to finish setup
  atomic_flag_spin_until_set(&Init_Flag);

  // each thread prints it's thread id
  // prints happen in order by using spinlocks
  for(i=0;i<numthreads;i++) {
    if(threadnum==i) {
      RigelPrint(threadnum); 
    }
    // barrier for a deterministic print ordering for verif
    BARRIER_ENTER(&bi);
  }

  // EPILOGUE
  BARRIER_ENTER(&bi);
  return 1;
}
////////////////////////////////////////////////////////////////////////////////


