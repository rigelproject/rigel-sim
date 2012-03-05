#ifndef __BENCHMARKS__COMMON_H__
#define __BENCHMARKS__COMMON_H__

#ifndef RIGEL
  #define RIGEL
#endif

#undef DEBUG
#undef DEBUG_PRINT_OUTPUT

#define PARTIAL_RUN
#undef FAST_FILE_IO

#define SW_COHERENCE 1
// Number of task_list_entry_t per core.   Since we assign one iter or more per
// tasks, we are probably going to be able to support upto 8*MAX_TASKLIST_ENTRY
// rows in the matrix which is *a lot*
#ifndef MAX_TASKS
  #define MAX_TASKS (1 << 18)
#endif

#ifdef HW_TQ
  #define FAST_DEQUEUE
  #define RIGEL_DEQUEUE TaskQueue_Dequeue_FAST
  #define RIGEL_ENQUEUE TaskQueue_Enqueue
  #define RIGEL_LOOP    TaskQueue_Loop  
  #define RIGEL_INIT    TaskQueue_Init
  #define RIGEL_END     TaskQueue_End
#endif

#ifdef CLUSTER_LOCAL_TQ
  #define RIGEL_DEQUEUE SW_TaskQueue_Dequeue_FAST
  #define RIGEL_ENQUEUE SW_TaskQueue_Enqueue
//  #define RIGEL_LOOP    SW_TaskQueue_Loop_SERIAL
  #define RIGEL_LOOP    SW_TaskQueue_Loop_PARALLEL
  #define RIGEL_INIT    SW_TaskQueue_Init
  #define RIGEL_END     SW_TaskQueue_End
#endif

// Added a switch to turn on and off use of non-globally allocate prefetches.
// CG is the only benchmark that uses these right now.  If turned of, regular
// prefetches are used.
#define USE_SW_PREFETCH_NGA

// Modes for fileslurp/filedump commands to control generation of input sets.
// This only needs to be run once for each benchmark. 
enum { 
  DATASET_MODE_CREATE = 1,
  DATASET_MODE_READ   = 0,
};

// Added to benchmarks to simulate flushes at the end of an interval (LAZY) or
// on a task-by-task basis (EAGER)
enum {
  TCMM_INV_LAZY_WB_LAZY = 0,
  TCMM_INV_LAZY_WB_EAGER = 1,
  TCMM_INV_EAGER_WB_LAZY = 2,
  TCMM_INV_EAGER_WB_EAGER = 3,
  TCMM_NONE = 4,
};
// Set on command line and read in at startup.
extern int TCMM_INV_LAZY;
extern int TCMM_INV_EAGER;
extern int TCMM_WRB_LAZY;
extern int TCMM_WRB_EAGER;

// Used for flushing
static const int ELEM_PER_CACHE_LINE_SH = 3; // 1 << 3 == 8
static const int ELEM_PER_CACHE_LINE = 8;
static const int MAX_TASKS_PER_CORE = 512;

// Structure for tracking ranges returned for task dequeues.
typedef struct rtm_range_t {
  union { unsigned int v1; } v1;
  union { unsigned int v2; } v2;
  union { unsigned int v3; int begin; int start; } v3;
  union { unsigned int v4; int count; int end; } v4;
} rtm_range_t;

#include <stdio.h>
#include <assert.h>

static inline void 
RTM_SetWritebackStrategy()
{
  int rtm_strat = -1;
  fscanf(stdin, "%d", &rtm_strat);
  switch (rtm_strat) 
  {
    case TCMM_INV_LAZY_WB_LAZY:
      TCMM_INV_LAZY   = 1; TCMM_INV_EAGER  = 0;
      TCMM_WRB_LAZY   = 1; TCMM_WRB_EAGER  = 0;
      fprintf(stderr, "RTM Mode: LILW\n", rtm_strat);
      break;
    case TCMM_INV_LAZY_WB_EAGER:
      TCMM_INV_LAZY   = 1; TCMM_INV_EAGER  = 0;
      TCMM_WRB_LAZY   = 0; TCMM_WRB_EAGER  = 1;
      fprintf(stderr, "RTM Mode: LIEW\n", rtm_strat);
      break;
    case TCMM_INV_EAGER_WB_LAZY:
      TCMM_INV_LAZY   = 0; TCMM_INV_EAGER  = 1;
      TCMM_WRB_LAZY   = 1; TCMM_WRB_EAGER  = 0;
      fprintf(stderr, "RTM Mode: EILW\n", rtm_strat);
      break;
    case TCMM_INV_EAGER_WB_EAGER:
      TCMM_INV_LAZY   = 0; TCMM_INV_EAGER  = 1;
      TCMM_WRB_LAZY   = 0; TCMM_WRB_EAGER  = 1;
      fprintf(stderr, "RTM Mode: EIEW\n", rtm_strat);
      break;
    case TCMM_NONE:
      TCMM_INV_LAZY   = 0; TCMM_INV_EAGER  = 0;
      TCMM_WRB_LAZY   = 0; TCMM_WRB_EAGER  = 0;
      fprintf(stderr, "RTM Mode: NONE\n", rtm_strat);
      break;
    default:
      fprintf(stderr, "Unknown RTM flush strategy: %d\n", rtm_strat);
      assert(0);
  }
}
#endif
