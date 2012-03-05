////////////////////////////////////////////////////////////////////////////////
// nosync
////////////////////////////////////////////////////////////////////////////////
// computes something simple on 1 core
// tests floating point operations
// no complex cross-core synchronization to worry about for this test
////////////////////////////////////////////////////////////////////////////////

#include "rigel.h"
#include "rigel-tasks.h"
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
////	START MAIN
////////////////////////////////////////////////////////////////////////////////
int main() {

  if (RigelGetThreadNum()==0) {

    SIM_SLEEP_OFF();

    RigelPrint(0x1333beef);

    // volatile...because the compiler is too darn clever with constants!
    volatile float a = 2.0;
    volatile float b = 1.0f / a;
    float c = sqrt(a);
    float d = a + b;
    float e = a - b;
    float f = a * b;
    float g = a / b;
    float h = -1.0f * a;
    int   i = (int)(a * b * c);
    int   j = (int)(h);
    int   k = (int)(c);

    RigelPrint(a);
    RigelPrint(b);
    RigelPrint(c);
    RigelPrint(d);
    RigelPrint(e);
    RigelPrint(f);
    RigelPrint(g);
    RigelPrint(h);
    RigelPrint(i);
    RigelPrint(j);
    RigelPrint(k);

  }

  return 0;
}

