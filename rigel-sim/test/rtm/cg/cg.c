#include "../common.h"

#include "cg.h"
#include "rigel.h"
#include <math.h>
#include "rigel-tasks.h"
#include <spinlock.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <incoherent_malloc.h>

ATOMICFLAG_INIT_CLEAR(ParReduceFlag);
ATOMICFLAG_INIT_CLEAR(Init_Flag);

int incoherent_malloc_enabled;

//XXX FIX ME XXX
#define MODE DATASET_MODE_READ

// Turn on for debugging
#define DEBUG_PRINT_OUTPUT
#define DEBUG_TIMERS_ENABLED 1

//#define DEBUG_PRINT(x) RigelPrint(x)
#define DEBUG_PRINT(x)

static inline int CG_REMAP_START_ITER(int start, int total)
{
  // For small matrices, we punt.
  if (total < 1024) { return start; }
  // If we are in the last block, we punt since we only gaurantee 1:1 with
  // 128-sized blocks.
  if ((start + 1024) > total) { return start; }
  
  int block_start = start & ~(1024UL - 1);
  int start_m128 = start & (1024UL - 1);

  // Hash into the 128 set.
  int offset = (start_m128 * 367) & (1024UL - 1);

  return block_start + offset;
}
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

BARRIER_INFO bi;

extern spin_lock_t GLOBAL_tq_enqueue_lock;
const int LINE_SIZE_BYTES = 32;
const int LINE_SIZE_WORDS = 8;
const int PARAM_TASK_GROUP_PREFETCH = 2;
const int PARAM_ITERS_PER_TASK = 4;

// From common.h
int TCMM_INV_LAZY;
int TCMM_INV_EAGER;
int TCMM_WRB_LAZY;
int TCMM_WRB_EAGER;

// TODO: Investigate smarter prefetching of data 
#undef SW_PREFETCH
// Same as SW_PREFETCH except grab a block at the start instead of every
// iteration
#define SW_PREFETCH_BLOCK
// Prefetch distance in cachelines (2 is default)
#define PREFETCH_DISTANCE_LINES 2
#define PREFETCH_DISTANCE_WORDS (PREFETCH_DISTANCE_LINES*LINE_SIZE_WORDS)
// Number of lines to prefetch at once: 4
#include "smatrix.h"

void 
MatVecMul(const SparseMatrix *A, const ElemType *vecn, ElemType *vec_out) 
{
  int row, idx, col;
  ElemType data;

  for (row = 0; row < MATRIX_DIM; row++) {
    vec_out[row] = 0.0f;
    for (idx = 0; idx < SMatGetRowSize(A, row); idx++) {
      data = SMatGetElem(A, row, idx, &col);
      vec_out[row] += vecn[col] * data;
    }
  }
}

ElemType
ParallelReduce(int threadnum, int numthreads, ElemType val, volatile ElemType *ReduceVals) {
  // XXX: Note that we no longer have a barrier *inside* the reduce.  A barrier
  // for each cluster must be reached before entering or bad things happen.

  // Print token for tracking by task scripts
  //RigelPrint(0xccff0100);

  // Special case for one thread
  if (numthreads == 1) return val;

  // Handle cache coherence by invalidating line before hand and flushing it at
  // the end.
  RigelInvalidateLine(&GlobalReduceReturn);
  RigelWritebackLine(&ReduceVals[threadnum]);

  // Do cluster summation on thread 0 for the cluster
  int mask = RigelGetNumThreadsPerCluster()-1;
  if ( (threadnum & mask) == 0 ) {
    int foo;
    int t;
    ElemType sum = 0;
    // unroll by factor of 8 (1 loop iter per core thread)
    for(t=0; t<RigelGetNumThreadsPerCluster(); t+=8) {
      ElemType val0 = ReduceVals[threadnum+t+0];
      ElemType val1 = ReduceVals[threadnum+t+1];
      ElemType val2 = ReduceVals[threadnum+t+2];
      ElemType val3 = ReduceVals[threadnum+t+3];
      ElemType val4 = ReduceVals[threadnum+t+4];
      ElemType val5 = ReduceVals[threadnum+t+5];
      ElemType val6 = ReduceVals[threadnum+t+6];
      ElemType val7 = ReduceVals[threadnum+t+7];
      sum += val0 + val1 + val2 + val3 + val4 + val5 + val6 + val7;
    }

    // Add to count to tell thread 0 to do global reduction
    ReduceVals[threadnum] = sum;
    RigelWritebackLine(&ReduceVals[threadnum]);
    ENABLE_NONBLOCKING_ATOMICS();
    RigelAtomicINC(foo, GlobalReduceClustersDone);
    DISABLE_NONBLOCKING_ATOMICS();

    // TODO: We could in fact have all of the cores in one cluster slurp in
    // values and thus increase the MLP relative to one core suffering
    // sequential misses at the highest level of the reduction
    if (threadnum == 0) {
      ElemType sum = 0;
      int i;
      int numclusters; RigelGetNumClusters(numclusters);
      // Spin until all clusters have partial sum done
      while ((GlobalReduceClustersDone % numclusters) != 0);

      int tpc = RigelGetNumThreadsPerCluster();
      // Unroll if worthwhile
      if (numclusters > 4) {
        for (i = 0; i < numclusters; i+=4) {
          ElemType val0 = ReduceVals[(i+0)*tpc];
          ElemType val1 = ReduceVals[(i+1)*tpc];
          ElemType val2 = ReduceVals[(i+2)*tpc];
          ElemType val3 = ReduceVals[(i+3)*tpc];
          sum += val0 + val1 + val2 + val3;
        }
      } else {
        for (i = 0; i < numclusters; i++) {
          sum += ReduceVals[i*tpc];
        }
      }
      GlobalReduceReturn = sum;
      // Thread 0 flushes it out so everyone can "see" it. Could use G.ST
      RigelWritebackLine(&GlobalReduceReturn);
    }
  }
  BARRIER_ENTER(&bi);
  // Move invalidations after barrier to hide latency of sequential work.
  if (!incoherent_malloc_enabled)
  {
    int numclusters; RigelGetNumClusters(numclusters);
    if (0 == threadnum) {
      int i;
      for (i = 0; i < numclusters; i++) {
        RigelInvalidateLine(&ReduceVals[i*8]);
      }
    }
  }

  return GlobalReduceReturn;
}
//////////////////////////////////////////////////
// Actual CG Solver Code
//////////////////////////////////////////////////
void
CGMain(int threadnum, int numthreads,  ElemType rho) {
  int i;
  SparseMatrix * A = &Amat;
  // Localize alpha and do divide indepdendently
  ElemType local_alpha;
  ElemType local_rho = rho;
  ElemType local_beta;

  int iters_per_task;
  TQ_TaskDesc enq_tdesc, deq_tdesc;
  TQ_RetType retval;

  ElemType reduceval_1;
  ElemType reduceval_2;

  // Count of tasks done in a particular interval on this thread
  int RTM_WB_Count = 0;
  // Array of start-end pairs to flush.
  rtm_range_t RTM_WB_IndexArray[MAX_TASKS_PER_CORE];

  // This is a tunable parameter that determines the granularity of work.
  // Since the matrices that we are working with are on the small side (1-2k
  // rows) we may want to reconsider the size here.
  iters_per_task = PARAM_ITERS_PER_TASK;


  if (threadnum == 0) {
    StartTimer(0);
    //fprintf(stderr, "START\n");
  }
  // Iterate upto MAXITER
  for (i = 0; i < MAXITER; i++) {
    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(2); }

    //////////////////////
    // XXX: PHASE 1 START
    //////////////////////
    RIGEL_LOOP(QID, MATRIX_DIM, iters_per_task, &enq_tdesc);

    // Everyone resets their reduction variable
    ReduceVals[threadnum] = 0.0f;
    while (1) 
    {
      #ifdef FAST_DEQUEUE
      TaskQueue_Dequeue_FAST(QID, &deq_tdesc, &retval);
      #else
      retval = RIGEL_DEQUEUE(QID, &deq_tdesc);
      #endif

      if (retval == TQ_RET_BLOCK) continue;
      if (retval == TQ_RET_SYNC) break;
      if (retval != TQ_RET_SUCCESS) RigelAbort();
      {
        int count = deq_tdesc.end - deq_tdesc.begin;
        int start = CG_REMAP_START_ITER(deq_tdesc.begin, MATRIX_DIM);
        ElemType reduceval = 0;

        // We will need to flush out the pieces of p before making this call.
        // Each block walks over p arbitrarily depending on which values in the
        // sparse matrix are non-zero
        Task_SMVMRows(A, (const ElemType *)p, q, &deq_tdesc); 

        // Do partial dot product
        Task_VecDot(&p[start], &q[start], count, &reduceval);
        ReduceVals[threadnum] += reduceval;

          // Handle the flushing for this iteration.  
//////////  START COHERENCE ACTIONS [[ EAGER ]] ////////////////////////////////
        if (!incoherent_malloc_enabled)
        {
          if (TCMM_WRB_EAGER)
          {
            int i;
            for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
              int offset = start + (i*ELEM_PER_CACHE_LINE);
              RigelWritebackLine(&q[offset]);
            }
          } 
          if (TCMM_INV_EAGER)
          {
            int i;
            for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
              int offset = start + (i*ELEM_PER_CACHE_LINE);
              RigelInvalidateLine(&p[offset]);
            }
          }
          if (TCMM_WRB_LAZY || TCMM_INV_LAZY) {
            // Save bounds of tdesc for later later flushes
            RTM_WB_IndexArray[RTM_WB_Count].v3.start = start;
            RTM_WB_IndexArray[RTM_WB_Count].v4.count = count;
            RTM_WB_Count++;
            if (RTM_WB_Count > MAX_TASKS_PER_CORE) { assert(0 && "Overflowed per-thread task descriptor array!  FIXME."); }
          }
        }
//////////  STOP COHERENCE ACTIONS [[ EAGER ]] ////////////////////////////////
      }
    }
//////////  START COHERENCE ACTIONS [[ LAZY ]] ////////////////////////////////
    if (!incoherent_malloc_enabled)
    {
      if (TCMM_WRB_LAZY)
      {
        int wb_count = 0;
        for (wb_count = 0; wb_count < RTM_WB_Count; wb_count++)
        {
          int i = 0;
          int start = RTM_WB_IndexArray[wb_count].v3.start;
          int count = RTM_WB_IndexArray[wb_count].v4.count;

          for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
            int offset = start + (i*ELEM_PER_CACHE_LINE);
            RigelWritebackLine(&q[offset]);
          }
        }
      }
      if (TCMM_INV_LAZY)
      {
        int wb_count = 0;
        for (wb_count = 0; wb_count < RTM_WB_Count; wb_count++)
        {
          int i = 0;
          int start = RTM_WB_IndexArray[wb_count].v3.start;
          int count = RTM_WB_IndexArray[wb_count].v4.count;

          for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
            int offset = start + (i*ELEM_PER_CACHE_LINE);
            RigelInvalidateLine(&p[offset]);
          }
        }
      }
      RTM_WB_Count = 0;
    }
//////////  STOP COHERENCE ACTIONS [[ LAZY ]] ////////////////////////////////

    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(2); }
    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(1); }
    reduceval_1 = 
      ParallelReduce(threadnum, numthreads, ReduceVals[threadnum], ReduceVals);
    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(1); }
    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(3); }

    //////////////////////
    // XXX: PHASE 2 START
    //////////////////////

    RIGEL_LOOP(QID, MATRIX_DIM, iters_per_task, &enq_tdesc);

    ReduceVals[threadnum] = 0.0f;
    while (1) 
    {
      #ifdef FAST_DEQUEUE
      TaskQueue_Dequeue_FAST(QID, &deq_tdesc, &retval);
      #else
      retval = RIGEL_DEQUEUE(QID, &deq_tdesc);
      #endif


      if (retval == TQ_RET_BLOCK) continue;
      if (retval == TQ_RET_SYNC) break;
      if (retval != TQ_RET_SUCCESS) RigelAbort();
      {
        int count = deq_tdesc.end - deq_tdesc.begin;
        int start = CG_REMAP_START_ITER(deq_tdesc.begin, MATRIX_DIM);
        ElemType reduceval = 0;


        // Calc ALPHA ( Made local)
        local_alpha = local_rho / reduceval_1;
        // Calc x[i+1] and Calc r[i+1]
        Task_VecScaleAdd(local_alpha, &p[start], &x[start], &x[start], count);
        Task_VecScaleAdd(-1.0f * local_alpha, &q[start], &res[start], &res[start], count);
        Task_VecDot(&res[start], &res[start], count, &reduceval); 
        ReduceVals[threadnum] += reduceval;

//////////  START COHERENCE ACTIONS [[ EAGER ]] ////////////////////////////////
        if (!incoherent_malloc_enabled)
        {
          if (TCMM_WRB_EAGER)
          {
            int i;
            for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
              int offset = start + (i*ELEM_PER_CACHE_LINE);
              RigelWritebackLine(&res[offset]);
              RigelWritebackLine(&x[offset]);
            }
          } 
          if (TCMM_INV_EAGER)
          {
            int i;
            for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
              int offset = start + (i*ELEM_PER_CACHE_LINE);
              RigelInvalidateLine(&p[offset]);
              RigelInvalidateLine(&q[offset]);
              RigelInvalidateLine(&res[offset]);
              RigelInvalidateLine(&x[offset]);
            }
          }
          if (TCMM_WRB_LAZY || TCMM_INV_LAZY) {
            // Save bounds of tdesc for later later flushes
            RTM_WB_IndexArray[RTM_WB_Count].v3.start = start;
            RTM_WB_IndexArray[RTM_WB_Count].v4.count = count;
            //RigelPrint(0xabababab);
            RTM_WB_Count++;
            if (RTM_WB_Count > MAX_TASKS_PER_CORE) { assert(0 && "Overran writeback buffer!  FIXME."); }
          }
        }
//////////  START COHERENCE ACTIONS [[ EAGER ]] ////////////////////////////////
      }
    }
//////////  STOP COHERENCE ACTIONS [[ LAZY ]] ////////////////////////////////
    if (!incoherent_malloc_enabled)
    {
      if (TCMM_WRB_LAZY)
      {
        int wb_count = 0;
        for (wb_count = 0; wb_count < RTM_WB_Count; wb_count++)
        {
          int i = 0;
          int start = RTM_WB_IndexArray[wb_count].v3.start;
          int count = RTM_WB_IndexArray[wb_count].v4.count;

          for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
            int offset = start + (i*ELEM_PER_CACHE_LINE);
            RigelWritebackLine(&res[offset]);
            RigelWritebackLine(&x[offset]);
          }
        }
      }
      if (TCMM_INV_LAZY)
      {
        int wb_count = 0;
        for (wb_count = 0; wb_count < RTM_WB_Count; wb_count++)
        {
          int i = 0;
          int start = RTM_WB_IndexArray[wb_count].v3.start;
          int count = RTM_WB_IndexArray[wb_count].v4.count;

          for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
            int offset = start + (i*ELEM_PER_CACHE_LINE);
            RigelInvalidateLine(&p[offset]);
            RigelInvalidateLine(&q[offset]);
            RigelInvalidateLine(&res[offset]);
            RigelInvalidateLine(&x[offset]);
          }
        }
      }
      RTM_WB_Count = 0;
    }
//////////  STOP COHERENCE ACTIONS [[ LAZY ]] ////////////////////////////////

    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(3); }
    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(5); }
    // XXX: BARRIER
    #ifdef FAST_DEQUEUE
    TaskQueue_Dequeue_FAST(QID, &deq_tdesc, &retval);
    #else
    BARRIER_ENTER(&bi);
    #endif
    
    // Parallel Reduce!
    reduceval_2 = 
      ParallelReduce(threadnum, numthreads, ReduceVals[threadnum], ReduceVals);
    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(5); }
    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StartTimer(4); }

    //////////////////////
    // XXX: PHASE 3: START
    //////////////////////
    RIGEL_LOOP(QID, MATRIX_DIM, iters_per_task, &enq_tdesc);

    ReduceVals[threadnum] = 0.0f;
    while (1) 
    {
      #ifdef FAST_DEQUEUE
      TaskQueue_Dequeue_FAST(QID, &deq_tdesc, &retval);
      #else
      retval = RIGEL_DEQUEUE(QID, &deq_tdesc);
      #endif


      if (retval == TQ_RET_BLOCK) continue;
      if (retval == TQ_RET_SYNC) break;
      if (retval != TQ_RET_SUCCESS) RigelAbort();
      {
        int count = deq_tdesc.end - deq_tdesc.begin;
        int start = CG_REMAP_START_ITER(deq_tdesc.begin, MATRIX_DIM);

        // Calc BETA
        local_beta = reduceval_2 / local_rho;
        // Calc p[i+1]
        Task_VecScaleAdd(local_beta, &p[start], &res[start], &p[start], count);

//////////  START COHERENCE ACTIONS [[ EAGER ]] ////////////////////////////////
        if (!incoherent_malloc_enabled)
        {
          if (TCMM_WRB_EAGER)
          {
            int i;
            for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
              int offset = start + (i*ELEM_PER_CACHE_LINE);
              RigelWritebackLine(&p[offset]);
            }
          } 
          if (TCMM_INV_EAGER)
          {
            int i;
            for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
              int offset = start + (i*ELEM_PER_CACHE_LINE);
              RigelInvalidateLine(&p[offset]);
              RigelInvalidateLine(&res[offset]);
            }
          }
          if (TCMM_WRB_LAZY || TCMM_INV_LAZY) {
            // Save bounds of tdesc for later later flushes
            RTM_WB_IndexArray[RTM_WB_Count].v3.start = start;
            RTM_WB_IndexArray[RTM_WB_Count].v4.count = count;
            RTM_WB_Count++;
            if (RTM_WB_Count > MAX_TASKS_PER_CORE) { assert(0 && "Overran writeback buffer!  FIXME."); }
          }
        }
//////////  START COHERENCE ACTIONS [[ EAGER ]] ////////////////////////////////
      }
    }
//////////  STOP COHERENCE ACTIONS [[ LAZY ]] ////////////////////////////////
    if (!incoherent_malloc_enabled)
    {
      if (TCMM_WRB_LAZY)
      {
        int wb_count = 0;
        for (wb_count = 0; wb_count < RTM_WB_Count; wb_count++)
        {
          int i = 0;
          int start = RTM_WB_IndexArray[wb_count].v3.start;
          int count = RTM_WB_IndexArray[wb_count].v4.count;

          for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
            int offset = start + (i*ELEM_PER_CACHE_LINE);
            RigelWritebackLine(&p[offset]);
          }
        }
      }
      if (TCMM_INV_LAZY)
      {
        int wb_count = 0;
        for (wb_count = 0; wb_count < RTM_WB_Count; wb_count++)
        {
          int i = 0;
          int start = RTM_WB_IndexArray[wb_count].v3.start;
          int count = RTM_WB_IndexArray[wb_count].v4.count;

          for (i = 0; i == 0 || i < (count >> ELEM_PER_CACHE_LINE_SH); i++) {
            int offset = start + (i*ELEM_PER_CACHE_LINE);
            RigelInvalidateLine(&p[offset]);
            RigelInvalidateLine(&res[offset]);
          }
        }
      }
      RTM_WB_Count = 0;
    }
//////////  STOP COHERENCE ACTIONS [[ LAZY ]] ////////////////////////////////

    // Exchange pointers for next iteration
    local_rho = reduceval_2;
    if (threadnum == 0 && DEBUG_TIMERS_ENABLED) { StopTimer(4); }
  }
  if (threadnum == 0) StopTimer(0);
}

int main() {
  // START THREAD LOCAL
  int i; 
  int threadnum  = RigelGetThreadNum();
  int numthreads = RigelGetNumThreads(); 
  // END THREAD LOCAL
  SparseMatrix * A = &Amat;

  if (threadnum == 0) {
    FILE *inFile;
    char input_filename[128];
    // Set the number of iterations to execute
    MAXITER = 8;
    RigelFlushLine(&MAXITER);
    
    SIM_SLEEP_ON();

    // Set it so all can see.
    incoherent_malloc_enabled = RigelIncoherentMallocEnabled();
    RigelFlushLine(&incoherent_malloc_enabled);

    RIGEL_INIT(QID, MAX_TASKS);

    // First parameter is invalidation strategy
    RTM_SetWritebackStrategy();

    fscanf(stdin, "%s", input_filename);

    if ((inFile = fopen(input_filename, "r")) == NULL) assert(0 && "fopen() in failed");
    setbuf(inFile, NULL); //make inFile unbuffered
    fread(&MATRIX_DIM, sizeof(int), 1, inFile);
    fread(&NONZERO_ELEMS, sizeof(int), 1, inFile); 
    RigelPrint(0x11110000 | NONZERO_ELEMS);
    RigelPrint(0x22220000 | MATRIX_DIM);


    // Set up data structures that hold the matrix information FIXME: free()
    // these and elem_list
    
    // Re do all malloc calls when slurping....
    ReduceVals = (ElemType *)incoherent_malloc(numthreads* sizeof(ElemType));
    RigelFlushLine(&ReduceVals);
    for (i = 0; i < numthreads; i++) { 
      ReduceVals[i] = 0; 
      RigelFlushLine(&ReduceVals[i]);
    }
    b = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    x = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    temp = (ElemType *)incoherent_malloc(sizeof(ElemType)*MATRIX_DIM);
    p = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    q = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    for (i = 0; i < MATRIX_DIM; i++) {
      p[i] = q[i] = 0.0f;
      x[i] = 1.0f;
      RigelFlushLine(&x[i]);
      RigelFlushLine(&p[i]);
      RigelFlushLine(&q[i]);
    }
    RigelFlushLine(b);
    RigelFlushLine(x);
    RigelFlushLine(temp);
    RigelFlushLine(&temp);
    RigelFlushLine(&b);
    RigelFlushLine(&x);
    RigelFlushLine(&p);
    RigelFlushLine(&q);

    res = (ElemType *)malloc(sizeof(ElemType)*MATRIX_DIM);
    RigelFlushLine(&res);

    __IA = (int *)incoherent_malloc(sizeof(int)*(MATRIX_DIM+1));
    __IJ = (int *)incoherent_malloc(sizeof(int)*NONZERO_ELEMS);
    __MatData = (ElemType *)incoherent_malloc(sizeof(ElemType)*NONZERO_ELEMS);
    RigelFlushLine(&__IA);
    RigelFlushLine(&__IJ);
    RigelFlushLine(&__MatData);

	  // Setup pointers for malloc'd structures
    A->IA = __IA;
    A->IJ = __IJ;
    A->MatData = __MatData;

    fread(A->IA, sizeof(int)*(MATRIX_DIM+1), 1, inFile);
    fread(A->IJ, sizeof(int)*NONZERO_ELEMS, 1, inFile);
    fread(A->MatData, sizeof(ElemType)*NONZERO_ELEMS, 1, inFile);
    fread(b, sizeof(ElemType)*MATRIX_DIM, 1, inFile);
    fread(x, sizeof(ElemType)*MATRIX_DIM, 1, inFile);
    fread(temp, sizeof(ElemType)*MATRIX_DIM, 1, inFile);
    fread((void *)p, sizeof(ElemType)*MATRIX_DIM, 1, inFile);
    fread((void *)q, sizeof(ElemType)*MATRIX_DIM, 1, inFile);
    fread(res, sizeof(ElemType)*MATRIX_DIM, 1, inFile);
    fread((void *)&rho, sizeof(ElemType), 1, inFile);
    fread(&A->NumRows, sizeof(int), 1, inFile);
    fread(&A->NumCols, sizeof(int), 1, inFile);
    fread(&A->NonZeroes, sizeof(int), 1, inFile);
    
    ClearTimer(0);    // Total time 
    ClearTimer(1);    // Reduction 1 cost
    ClearTimer(5);    // Reduction 2 cost
    ClearTimer(2);    // SMVM
    ClearTimer(3);    // Phase II
    ClearTimer(4);    // Phase III
    RigelFlushLine(&A->IA);
    RigelFlushLine(&A->IJ);
    RigelFlushLine(&A->MatData);
    RigelFlushLine(&rho);
    RigelFlushLine(&p);
    RigelFlushLine(&q);
    BARRIER_INIT(&bi);
    RigelFlushLine(&bi);
    SW_TaskQueue_Set_TASK_GROUP_FETCH(PARAM_TASK_GROUP_PREFETCH);
    // Enqueue eight task groups into the LTQ before considering a GTQ enqueue
    //SW_TaskQueue_Set_LOCAL_ENQUEUE_COUNT(1);
    SW_TaskQueue_Set_DATA_PARALLEL_MODE();

    // Bug if this is not set to zero initially!
    GlobalReduceClustersDone = 0;
    RigelFlushLine(&GlobalReduceClustersDone);

// Start flag.
    SIM_SLEEP_OFF();
    atomic_flag_set(&Init_Flag);
  }

  atomic_flag_spin_until_set(&Init_Flag);
  // DO CG ITERATIONS IN PARALLEL
  CGMain(threadnum, numthreads, rho);

  //asm ( "hlt" );
  //asm ( "hlt" );
  //asm ( "hlt" );
  //asm ( "hlt" );
  //asm ( "hlt" );
  BARRIER_ENTER(&bi);
  // Tidy up after the end of the computation

#ifdef DEBUG_PRINT_OUTPUT
  if (threadnum == 0) { SIM_SLEEP_ON(); for (i = 0; i < MATRIX_DIM; i++) RigelPrint(x[i]); SIM_SLEEP_OFF();}
#endif

  return 0;
}
