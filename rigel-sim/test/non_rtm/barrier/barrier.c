#include <assert.h>
#include <rigel.h>
#include <rigel-tasks.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include "rtm-sw.h"

////////////////////////////////////////////////////////////////////////////////
// Barrier
////////////////////////////////////////////////////////////////////////////////
// this is a simple test code that checks various API functions and basic
// simulator functionality
//
// This test is NOT comprehensive and does not mean your changes work; it is
// just an easy way to see if something is horribly busted.
//
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
BARRIER_INFO bi;

////////////////////////////////////////////////////////////////////////////////
int main() {

  int threadnum  = RigelGetThreadNum();

  //////////////////////////////////////////////////////////////////////////////
	if (0 == threadnum) {

    SIM_SLEEP_OFF(); // because rigelsim starts with it on sometimes, dammit
    RigelPrint(0xf00dd00d); // sentinel print

    // Initialize the barrier before use.  No core may attempt entry into the
    // barrier until this has completed.
    BARRIER_INIT(&bi);
    RigelPrint(0xcafecafe); // sentinel print

    // Barrier is setup.  Wake up other cores.
    atomic_flag_set(&Init_Flag);
	} 
  //////////////////////////////////////////////////////////////////////////////
  
  // Wait for the barrier to initialize.
  atomic_flag_spin_until_set(&Init_Flag);

  //RigelPrint(threadnum); //non-deterministic ordering, this is print just for debugging
  BARRIER_ENTER(&bi);

  // print something to record that we got here
  if (threadnum == 0) {  // would be better if this was not zero, but for now...
    RigelPrint(0x1337beef); 
  } 

  return 1;
} 


