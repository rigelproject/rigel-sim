////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include "rigel.h"
#include "rigel-tasks.h"
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>


#define ITERS  32
float results[ITERS];
const float recipi = 1.0f/3.14159f;

////////////////////////////////////////////////////////////////////////////////
////	START MAIN
////////////////////////////////////////////////////////////////////////////////
int main() {

  if(RigelGetThreadNum()==0) {

    SIM_SLEEP_OFF();

    RigelPrint(0x1333beef);

    for (int i=0; i<ITERS; i++) {
      results[i] = 3.14159f * i;
    }

    for (int i=0; i<ITERS; i++) {
      results[i] = ((float)(i)) + (results[i] * recipi);
    }

    for (int i=0; i<ITERS; i++) {
      RigelPrint(results[i]);
    }

  }

  return 0;
}

