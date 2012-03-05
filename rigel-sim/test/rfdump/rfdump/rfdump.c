#include "../../../../targetcode/src/testing/common.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <rigel.h>
#include <rigel-tasks.h>
#include <spinlock.h>
#include "rtm-sw.h"
#include "rigellib.h"


ATOMICFLAG_INIT_CLEAR(Init_Flag);
RigelBarrier_Info bi;

#define LOOP_COUNT 1024*100

int
main(int argc, char *argv[])
{
	int corenum = RigelGetCoreNum();
  int i;

  //////////////////////////////////////////////////////////////////////////////
	// PROLOGUE
	if (0 == corenum) {
    RigelBarrier_Init(&bi);
    ClearTimer(0);
    StartTimer(0);
    atomic_flag_set(&Init_Flag);
	} 

  atomic_flag_spin_until_set(&Init_Flag);

  // BODY
    RigelPrint(0xaaaa0001);
    asm volatile (
      " add   $5, $0, $0; " // #; clear R5
      " addi  $5, $5, 1;  " // #; R5 dependency
      " addi  $5, $5, 2;  " // #; R5 dependency
      " addi  $5, $5, 2;  " // #; R5 dependency (5 in R5)
      " addi  $6, $0, 6;  " // #; 6 in R6
      " addi  $7, $0, 7;  " // #; 7 in R7
      " addi  $8, $0, 8;  " // #; 8 in R8
      :
      :
      : "5", "6", "7", "8"
    );
    if(corenum==0) {
      EVENTTRACK_RF_DUMP();
    }

  RigelBarrier_EnterFull(&bi);

  // EPILOGUE
  asm volatile (
    " printreg $5; "
    " printreg $6; "
    " printreg $7; "
    " printreg $8; "
  );
	if (0 == corenum) {
    StopTimer(0);
  }
  
  //////////////////////////////////////////////////////////////////////////////

  return 0;
}
