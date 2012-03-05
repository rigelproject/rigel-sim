#ifndef __DMM_H__
#define __DMM_H__

typedef float ElemType;

// The number of sub matrices to break the output into.  The number of blocks to
// be done is equal to BLOCK_SIZE_I * BLOCK_SIZE_J
#define ELEMS_PER_CACHE_LINE (32/sizeof(ElemType);
#define QID 0

// Defined in dmm.c
extern volatile int BLOCK_SIZE_I;
extern volatile int BLOCK_SIZE_J;
extern volatile int DIM_M;
extern volatile int DIM_N;
extern volatile int DIM_O;
extern ElemType **matA1;
extern ElemType **matA2;
extern ElemType **resultA;

#endif
