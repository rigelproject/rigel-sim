////////////////////////////////////////////////////////////////////////////////
// nosync
////////////////////////////////////////////////////////////////////////////////
// computes something simple on 1 core
// no complex cross-core synchronization to worry about for this test
////////////////////////////////////////////////////////////////////////////////

#include "rigel.h"
#include "rigel-tasks.h"
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>


unsigned int results[32];

void voidFunc(int i);
int  intFunc(int i);
    
////////////////////////////////////////////////////////////////////////////////
////	START MAIN
////////////////////////////////////////////////////////////////////////////////
int main() {

  if(RigelGetCoreNum()==0) {

    SIM_SLEEP_OFF();

    RigelPrint(0x1333beef);

    for (int i=0; i<32; i++) {
      RigelPrint(i);
      switch(i) {
        case 0:
        case 1:
        case 2:
        case 3:
          results[i] = i; 
          break;
        case 4:
        case 5:
        case 6:
        case 7:
          results[i] = -i; 
          break;
        case 8:
          results[i] = (int)(0xF0000000) >> 28;
          break;
        case 9:
          results[i] = (unsigned)(0xF0000000) >> 28;
          break;
        case 10:
          results[i] = intFunc(i);
          break;
        case 11:
          if( (int)(results[0]) > (int)(results[4]) ) {
            results[i] == 0xd00d0000;
          } else {
            results[i] == 0xf00d000;
          }
          break;
        case 12:
          if( (unsigned)(results[0]) > (unsigned)(results[4]) ) {
            results[i] == 0xd00d0000;
          } else {
            results[i] == 0xf00d000;
          }
          break;
        case 13:
          if( (int)(results[1]) > (int)(results[5]) ) {
            results[i] == 0xd00d0000;
          } else {
            results[i] == 0xf00d000;
          }
          break;
        case 14:
          if( (unsigned)(results[1]) > (unsigned)(results[5]) ) {
            results[i] == 0xd00d0000;
          } else {
            results[i] == 0xf00d000;
          }
          break;
        case 15:
          results[i] = i - 16;
          break;
        case 16:
          results[i] = results[10] * results[3];
          break;
        default:
          voidFunc(i & 0x0007);
          break;
      }
      RigelPrint(results[i]);
    }
    RigelPrint(0xf00dd00d);
  }
  return 0;
}

int intFunc(int i) {
  int fact = 0;
  int ii = i;
  while(ii > 0) {
    fact = fact * ii;
    ii--;
  }
  results[i] = fact;
}
void voidFunc(int i) {
  results[i] = intFunc(i);
}
