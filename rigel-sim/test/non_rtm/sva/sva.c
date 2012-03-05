////////////////////////////////////////////////////////////////////////////////
// SVA
////////////////////////////////////////////////////////////////////////////////
// A simple RTM-based test with golden outputs!
// Adds two dynamically allocated arrays that are initialized in a simple
// fashion
////////////////////////////////////////////////////////////////////////////////

#include "rigel.h"
#include "rigel-tasks.h"
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
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


ATOMICFLAG_INIT_CLEAR(Init_flag);
BARRIER_INFO bi;

float *vec1_float;
int   *vec1_int;
float *vec2_float;
int   *vec2_int;
float *result_float;
int   *result_int;
volatile int VECSIZE;

    
////////////////////////////////////////////////////////////////////////////////
// simple Scaled Vector Addition Testcode
////////////////////////////////////////////////////////////////////////////////

// simple task, add the specified ranges
void DoTask(int i) {
  result_float[i] = vec1_float[i] + vec2_float[i];
  result_int[i] = vec1_int[i] + vec2_int[i];
}


////////////////////////////////////////////////////////////////////////////////
// enqueue and dequeue/distribute/dispatch tasks
////////////////////////////////////////////////////////////////////////////////
void SVA(int thread, int tasks_per_thread) {
  int i;
  int begin = thread*tasks_per_thread;
  int end = thread*tasks_per_thread+tasks_per_thread;
	for( i = begin; i < end; i+=2 ){
    DoTask(i);
    DoTask(i+1);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////
////	START MAIN
////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int main() {
	int i=0;
  int j=0;
	int core = RigelGetCoreNum();
  int thread = RigelGetThreadNum();
  int numthreads = RigelGetNumThreads();
  int tasks_per_thread;
	if (core == 0 && thread == 0) {
    
    BARRIER_INIT(&bi);
    fprintf(stderr, "Beginning...\n");

		// Read input
		fscanf(stdin, "%d", &VECSIZE);
		fprintf(stderr, "vector size: %d\n", VECSIZE);
    fprintf(stderr, "tasks/thread: %d\n",VECSIZE/numthreads);
  	// Initialize Matrix
		vec1_int   = (int*)malloc(sizeof(int)*VECSIZE);
		vec2_int   = (int*)malloc(sizeof(int)*VECSIZE);
		result_int = (int*)malloc(sizeof(int)*VECSIZE);
	  // Initialize Matrix
		vec1_float   = (float*)malloc(sizeof(float)*VECSIZE);
		vec2_float   = (float*)malloc(sizeof(float)*VECSIZE);
		result_float = (float*)malloc(sizeof(float)*VECSIZE);
    // init to some values module primes to minimize phasing
    for(i=0;i<VECSIZE;i++) {
      vec1_int[i] = i%97;
      vec2_int[i] = i%17;
      vec1_float[i] = i%97;
      vec2_float[i] = i%17;
    }

		ClearTimer(0);
    // Start full run timer
		StartTimer(0);

    SIM_SLEEP_OFF();

		atomic_flag_set(&Init_flag);
	}

  // ready, set, GO!
	atomic_flag_spin_until_set(&Init_flag);
  tasks_per_thread = VECSIZE/numthreads;
	SVA(thread,tasks_per_thread);

  BARRIER_ENTER(&bi);
  // Stopfull run timer
	if (core == 0 && thread == 0) {
    StopTimer(0);
    for(i=0;i<VECSIZE;i++) {
      RigelPrint(result_int[i]);
    }
    for(i=0;i<VECSIZE;i++) {
      RigelPrint(result_float[i]);
    }

  }

	return 0;
}
