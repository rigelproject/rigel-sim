#include <assert.h>
#include <rigel.h>
#include <rigel-tasks.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include "rtm-sw.h"

////////////////////////////////////////////////////////////////////////////////
// SimpleTest
////////////////////////////////////////////////////////////////////////////////
// this is a simple test code that checks various API functions and basic
// simulator functionality
//
// This test is NOT comprehensive and does not mean your changes work; it is
// just an easy way to see if something is horribly busted.
//
// This does NOT test RTM functionality!
//
// Some simple trivial computations are performed per-core/thread and the
// results printed to allow regression testing
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

#define VECT_SIZE  32
ATOMICFLAG_INIT_CLEAR(Init_Flag);
BARRIER_INFO bi;


int A[VECT_SIZE];

////////////////////////////////////////////////////////////////////////////////
int main() {

  int i; 
  int corenum   = RigelGetCoreNum();
  int threadnum = RigelGetThreadNum();
	int numcores  = RigelGetNumCores();
  int numthreads = RigelGetNumThreads();
  int th_per_core    = RigelGetNumThreadsPerCore();
  int th_per_cluster = RigelGetNumThreadsPerCluster();

  //////////////////////////////////////////////////////////////////////////////
	if (0 == threadnum) {

    SIM_SLEEP_ON(); // test SIM_SLEEP
    
    assert(numthreads <= VECT_SIZE && "more threads than VECT_SIZE");

    fprintf(stderr,"Thread %d (of %d per core, of %d per cluster, of  %d total) on core %d (of %d total)\n", 
      threadnum, th_per_core, th_per_cluster, numthreads, corenum, numcores);

    // Initialize the barrier before use.  No core may attempt entry into the
    // barrier until this has completed.
    BARRIER_INIT(&bi);
    // Barrier is setup.  Wake up other cores.
    for(i=0;i<VECT_SIZE;i++){
      A[i] = i;
      RigelPrint(A[i]);
    }
    atomic_flag_set(&Init_Flag);

		// FIXME re-enable the memory image dump when we've reimplemented it for BackingStore.
		// Previously, the MemoryModelGDDR4 wrapper class traversed the memory image and dumped it to file.
		// We have the means to do this much more efficiently for sparse memory images by traversing the
		// BackingStore's radix tree directly.
    // test mem dump
    //EVENTTRACK_MEM_DUMP();

    SIM_SLEEP_OFF();

	} 
  //////////////////////////////////////////////////////////////////////////////
  
  // Wait for the barrier to initialize.
  atomic_flag_spin_until_set(&Init_Flag);

  //RigelPrint(threadnum); //non-deterministic ordering, this is print just for debugging
  BARRIER_ENTER(&bi);

  // Random wait
  A[threadnum]=A[threadnum]+threadnum; // some trivial data modification

  // barrier again after data updates
  BARRIER_ENTER(&bi);

  //  core 0 prints results
  if( 0 == threadnum) {
    for(i=0;i<VECT_SIZE;i++) {
      RigelPrint(A[i]);
    }
  }

  // barrier again so we all quit at about the same time
  BARRIER_ENTER(&bi);

  return 1;
} 


