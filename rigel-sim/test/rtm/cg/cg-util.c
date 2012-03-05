#include "cg.h"
// START: GLOBALS
volatile int GlobalSense;
volatile int BarrierCount;
// Pointer to array of reduction variables, one per core
ElemType *ReduceVals;
int MATRIX_DIM;
int NONZERO_ELEMS;
int MAXITER;
ElemType * b;
SparseMatrix Amat;
ElemType * x;
ElemType * res;
ElemType *temp;
ElemType *p;
ElemType *q;
ElemType rho;
ElemTypeListEntry *elem_list;
ElemType *__MatData;
// Accumulated error only written by core zero
ElemType error;
int *__IA;
int *__IJ;
// TODO set this up as a tree of global counters so that reduction checking
// takes log(N) and work can be done early
volatile unsigned int GlobalReduceClustersDone;
volatile ElemType GlobalReduceReturn;
ElemType *ReduceVals;
