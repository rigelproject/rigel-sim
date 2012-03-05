#include "rigel.h"
#include <stdio.h>
#include <stdlib.h>
#include <spinlock.h>

// Make it the cluster cache size.
#define GLOBAL_DATA_SIZE 256
static const int WORDS_PER_LINE = 8;
const int ITERATIONS = 2;
volatile int GLOBAL_data[GLOBAL_DATA_SIZE]  __attribute__ ((aligned (8))) __attribute__ ((section (".rigellocks")));
volatile int GLOBAL_data_ro[GLOBAL_DATA_SIZE]  __attribute__ ((aligned (8))) __attribute__ ((section (".rigellocks")));

ATOMICFLAG_INIT_CLEAR(initFlag);

int main(int argc, char *argv[])
{
  int core = RigelGetCoreNum();
  int idx = 0;

  if(core == 0) {
    SIM_SLEEP_ON();
    for (idx = 0; idx < GLOBAL_DATA_SIZE; idx++) { GLOBAL_data[idx] = 0; }
    SIM_SLEEP_OFF();
    atomic_flag_set(&initFlag);
  } else {
    atomic_flag_spin_until_set(&initFlag);
  }

  // Test read-to-write upgrades
  for (idx = 0; idx < ITERATIONS; idx++)
  {
    int cluster = RigelGetClusterNum();
    if ((0x07 & core) == 0)
    {
      asm volatile ( 
          "   mvui  $1, %%hi(GLOBAL_data); \n"
          "   addi  $1, $1, GLOBAL_data; \n"
          "   mfsr  $26, $4;                  \n"
          "   slli  $26, $26, 5; \n"
          "   add   $27, $26, $1; \n"
          "   addi  $25, $zero, 42; \n"
          "L%=_00:              \n"
          "   ldl   $26, $27; \n"
          "   stc   $1, $25, $27; \n"
          "   beq   $1, $zero, L%=_00;\n"
          :
        : 
        : "27", "26", "1"
      );
      RigelWritebackLine(&(GLOBAL_data[cluster * WORDS_PER_LINE]));
    } else {
      asm volatile ( 
          "   mvui  $1, %%hi(GLOBAL_data); \n"
          "   addi  $1, $1, GLOBAL_data; \n"
          "   mfsr  $26, $4;                  \n"
          "   slli  $26, $26, 5; \n"
          "   add   $27, $26, $1; \n"
          "L%=_00:              \n"
          //"   line.inv  $27     \n"
          "   ldl   $26, $27; \n"
          //"   stc   $26, $27, 0; \n"
          "   beq   $26, $zero, L%=_00;\n"
          :
        : 
        : "27", "26", "1"
      );
    }
  }

  // Test writebacks.
  for (idx = 0; idx < ITERATIONS; idx++)
  {
    switch (core & 0x3) {
      case 0x00: 
      {
        int j;
        for (j = 0; j < GLOBAL_DATA_SIZE; j += WORDS_PER_LINE) { RigelPrint(GLOBAL_data[j]); }
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j-= WORDS_PER_LINE) { RigelWritebackLine(&GLOBAL_data[j]); }
        break;
      }
      case 0x01: 
      {
        int j;
        int dummy = 0;
        for (j = 0; j < GLOBAL_DATA_SIZE; j += WORDS_PER_LINE) {  GLOBAL_data[j] = 0x42; }
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j--) { RigelAtomicADDU(dummy, GLOBAL_data[j], dummy); }
        break;
      }
      case 0x02: 
      {
        int j;
        int dummy = 0;
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j-= WORDS_PER_LINE) { RigelAtomicINC(dummy, GLOBAL_data[j]); }
        for (j = 0; j < GLOBAL_DATA_SIZE; j += WORDS_PER_LINE) {  RigelInvalidateLine(&GLOBAL_data[j]); }
        break;
      }
      case 0x03: 
      {
        int j;
        for (j = 0; j < GLOBAL_DATA_SIZE; j+=WORDS_PER_LINE) { RigelPrint(GLOBAL_data[j]); }
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j--) { GLOBAL_data[j] = 0x47; }
        break;
      }
      default: /* This should not happen */
        break;
    }
  }

  // Test normal operation
  for (idx = 0; idx < ITERATIONS; idx++)
  {
    switch (core & 0x3) {
      case 0x00: 
      {
        int j;
        for (j = 0; j < GLOBAL_DATA_SIZE; j++) { RigelPrint(GLOBAL_data[j]); }
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j-= WORDS_PER_LINE) { RigelWritebackLine(&GLOBAL_data[j]); }
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j--) { GLOBAL_data[j] = 0x47; }
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j--) { volatile int x = GLOBAL_data_ro[j]; }
        break;
      }
      case 0x01: 
      {
        int j;
        int dummy = 0;
        for (j = 0; j < GLOBAL_DATA_SIZE; j += WORDS_PER_LINE) {  GLOBAL_data[j] = 0x42; }
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j--) { RigelAtomicADDU(dummy, GLOBAL_data[j], dummy); }
        break;
      }
      case 0x02: 
      {
        int j;
        int dummy = 0;
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j--) { RigelAtomicINC(dummy, GLOBAL_data[j]); }
        for (j = 0; j < GLOBAL_DATA_SIZE; j += WORDS_PER_LINE) {  RigelInvalidateLine(&GLOBAL_data[j]); }
        for (j = 0; j < GLOBAL_DATA_SIZE; j++) { volatile int x = GLOBAL_data_ro[j]; }
        for (j = 0; j < GLOBAL_DATA_SIZE; j++) { RigelPrint(GLOBAL_data[j]); }
        break;
      }
      case 0x03: 
      {
        int j;
        for (j = GLOBAL_DATA_SIZE-1; j >= 0; j--) { GLOBAL_data[j] = 0x47; }
        for (j = 0; j < GLOBAL_DATA_SIZE; j++) { RigelPrint(GLOBAL_data[j]); }
        break;
      }
      default: /* This should not happen */
        break;
    }
        int j;
    // Test writebacks
    for (j = GLOBAL_DATA_SIZE-1; j >= 0; j-= WORDS_PER_LINE) { RigelWritebackLine(&GLOBAL_data[j]); }
  }

  return 0;
}
