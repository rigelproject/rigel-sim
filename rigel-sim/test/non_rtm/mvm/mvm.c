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
BARRIER_INFO bi;

#define SIZE 96

int MATRIX[SIZE][SIZE];
int VECTOR[SIZE];
int RESULT[SIZE];

////////////////////////////////////////////////////////////////////////////////
int MVM( int row ) {
  int i;
  int sum = 0;
  // iterate across columns of the assigned row
  for(i=0; i<SIZE; i++) {
    sum = sum + (MATRIX[row][i] * VECTOR[i]);
  }
  return sum;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
int main() {
  SIM_SLEEP_OFF();

  // some variables
  int corenum   = RigelGetCoreNum();
  int threadnum = RigelGetThreadNum();
	int numcores  = RigelGetNumCores();
  int numthreads = RigelGetNumThreads();
  int th_per_core    = RigelGetNumThreadsPerCore();
  int th_per_cluster = RigelGetNumThreadsPerCluster();

  int rows_per_thread = SIZE / numthreads; // statically divide work up
  int start = rows_per_thread * threadnum;
  int end   = start + rows_per_thread;
  int i=0, j=0;

  // setup on thread 0
  if(threadnum == 0) {

    BARRIER_INIT(&bi);

    // init values
    for(i=0;i<SIZE;i++){
      for(j=0;j<SIZE;j++){
        MATRIX[j][i] = j; 
      }
      VECTOR[i] = i;
      RESULT[i] = 0xbeef;
    }

    //printf("rpf:%d numthreads:%d\n", rows_per_thread, numthreads);
    ClearTimer(0);
    // get started
    StartTimer(0);
    atomic_flag_set(&Init_Flag);
  }

  // wait for core 0 to finish setup
  atomic_flag_spin_until_set(&Init_Flag);

  // do the work
  for( i=start; i<end; i++ ) {
    RESULT[i] = MVM(i);  
  }

  BARRIER_ENTER(&bi);

  // cleanup on thread 0
  if(threadnum == 0) {
    StopTimer(0);
    // print results
    for(i=0; i<SIZE; i++) {
      RigelPrint(RESULT[i]);
    }
  }

  return 1;
}
////////////////////////////////////////////////////////////////////////////////


