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

ATOMICFLAG_INIT_CLEAR(Init_flag);

float *vec1_float;
int   *vec1_int;
float *vec2_float;
int   *vec2_int;
float *result_float;
int   *result_int;
volatile int VECSIZE;
#define ELEMS_PER_TASK 8 
#define QID 0
#define MAX_TASKS 1<<16

// Task Q stuff
//  #define FAST_DEQUEUE
  #define RIGEL_DEQUEUE SW_TaskQueue_Dequeue_FAST
  #define RIGEL_ENQUEUE SW_TaskQueue_Enqueue
  #define RIGEL_LOOP    SW_TaskQueue_Loop_PARALLEL  
  #define RIGEL_INIT    SW_TaskQueue_Init
  #define RIGEL_END     SW_TaskQueue_End
    
////////////////////////////////////////////////////////////////////////////////
// simple Scaled Vector Addition Testcode
////////////////////////////////////////////////////////////////////////////////

// simple task, add the specified ranges
void DoTask( TQ_TaskDesc *task ) {
  int i;
	for( i = task->begin; i < task->end; i++ ){
    result_float[i] = vec1_float[i] + vec2_float[i];
    result_int[i] = vec1_int[i] + vec2_int[i];
  }
}


////////////////////////////////////////////////////////////////////////////////
// enqueue and dequeue/distribute/dispatch tasks
////////////////////////////////////////////////////////////////////////////////
void SVA() {

  TQ_TaskDesc tdesc  = { 0x0, 0x0};
  int retval;

  // toss 'em in the Q!
  retval = RIGEL_LOOP( QID, VECSIZE, ELEMS_PER_TASK, &tdesc );

  // take 'em outta da Q!
  while (1) {

    	#ifdef FAST_DEQUEUE
    	RIGEL_DEQUEUE(QID, &tdesc, &retval);
    	#else
    	retval = RIGEL_DEQUEUE(QID, &tdesc);
    	#endif
    	if (retval == TQ_RET_BLOCK) continue;
    	if (retval == TQ_RET_SYNC) break;
    	if (retval != TQ_RET_SUCCESS) RigelAbort();
      {
        DoTask(&tdesc); // add the numbers
    	}

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
	int i, j;
	int core = RigelGetCoreNum();
  int thread = RigelGetThreadNum();
  float *data;

	if (core == 0 && thread == 0) {

    SIM_SLEEP_ON();

    fprintf(stderr, "Beginning...\n");

		// Read input
		fscanf(stdin, "%d", &VECSIZE);
		fprintf(stderr, "vector size: %d\n", VECSIZE);

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
		ClearTimer(1);
    // Start full run timer
		StartTimer(0);

		RIGEL_INIT(QID, MAX_TASKS);
   
    SIM_SLEEP_OFF();

		atomic_flag_set(&Init_flag);
	}

  // ready, set, GO!
	atomic_flag_spin_until_set(&Init_flag);
	SVA();

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
