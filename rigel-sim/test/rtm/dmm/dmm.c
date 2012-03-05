#include "../common.h"

#include "rigel.h"
#include "rigel-tasks.h"
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#undef DEBUG
#include "dmm.h"

int incohernt_malloc_enabled;

ATOMICFLAG_INIT_CLEAR(Init_flag);

ElemType **matA1;
ElemType **matA2;
ElemType **resultA;
ElemType *data1, *data2, *data3;

// From common.h
int TCMM_INV_LAZY;
int TCMM_INV_EAGER;
int TCMM_WRB_LAZY;
int TCMM_WRB_EAGER;

volatile int BLOCK_SIZE_I;
volatile int BLOCK_SIZE_J;
volatile int DIM_M;
volatile int DIM_N;
volatile int DIM_O;
volatile int BLOCK_SIZE_I_SH;
volatile int BLOCK_SIZE_I_MASK;
volatile int BLOCK_SIZE_J_MASK;
volatile int BLOCK_SIZE_J_SH;

// We do not bother using blocks for DIM_N > 256
const int BLOCK_SIZE_K = 64;
const int BLOCK_SIZE_K_SH = 6;
const int BLOCK_SIZE_K_MASK = 0x03F;

// Blocking factors.  The matrix is broken up into BLOCK_COUNT_I*BLOCK_COUNT_J
// regions.  One region is computed per interval. The number of outputs given to
// each task is (1 << OUTPUT_PER_TASK_SH) computed on a window of
// OUTPUT_PER_TASK_I x OUTPUT_PER_TASK_J elements.
const unsigned int BLOCK_COUNT_I = 8;
const unsigned int BLOCK_COUNT_J = 8;
const unsigned int OUTPUT_PER_TASK_I = 2;
const unsigned int OUTPUT_PER_TASK_J = 2;
const unsigned int OUTPUT_PER_TASK_I_SH = 1;  // log2(OUTPUT_PER_TASK_I)
const unsigned int OUTPUT_PER_TASK_J_SH = 1;  // log2(OUTPUT_PER_TASK_J)
const unsigned int OUTPUT_PER_TASK_SH = 2;    // OUTPUT_PER_TASK_I_SH + OUTPUT_PER_TASK_J_SH

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////
////  TASK-BASED VERSION OF MM FOR RIGEL
////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// I_ITER = idx_i / BLOCK_SIZE_I
// J_ITER = idx_j % BLOCK_SIZE_J
// i = {0 .. i_iters_per_task} + (BLOCK_SIZE_I * I_ITER)
// j = {0 .. j_iters_per_task} + (BLOCK_SIZE_J * J_ITER)
static inline 
unsigned int 
compute_i_index(int idx_i, int task_num) 
{
//  const int SHIFT = BLOCK_SIZE_J_SH - OUTPUT_PER_TASK_J_SH;
//  return idx_i + (OUTPUT_PER_TASK_I * (task_num >> SHIFT));
  return idx_i + (OUTPUT_PER_TASK_I * (task_num >> (BLOCK_SIZE_I_SH - OUTPUT_PER_TASK_I_SH)));
}

static inline 
unsigned int 
compute_j_index(int idx_j, int task_num) 
{
//  const int MASK = (1 << (1 + BLOCK_SIZE_J_SH - OUTPUT_PER_TASK_J_SH)) - 1;
//  return idx_j + (OUTPUT_PER_TASK_J * (MASK & task_num));
  return idx_j + (OUTPUT_PER_TASK_J * (task_num & (BLOCK_SIZE_J_MASK >> OUTPUT_PER_TASK_J_SH)));
}

// RTM Cleanup helper function
static inline void
RTM_dmm_flush_tdesc(TQ_TaskDesc * tdesc) 
{
  int idx_i = tdesc->v1;
  int idx_j = tdesc->v2;
  int task_num = tdesc->begin;
  unsigned int i = compute_i_index(idx_i, task_num);
  unsigned int j = compute_j_index(idx_j, task_num);
  unsigned int i_cnt, j_cnt;

  for (i_cnt = 0; i_cnt < OUTPUT_PER_TASK_I; i_cnt++) {
    for (j_cnt = 0; j_cnt < OUTPUT_PER_TASK_J; j_cnt += ELEM_PER_CACHE_LINE) {
        RigelFlushLine(&resultA[i+i_cnt][j+j_cnt]);
    }
  }
}
static inline void
RTM_dmm_inv_tdesc(TQ_TaskDesc * tdesc) 
{
  // Invalidations unnecessary with hybrid scheme.
  if (incohernt_malloc_enabled) return;

  int idx_i = tdesc->v1;
  int idx_j = tdesc->v2;
  int idx_k = tdesc->end;
  int task_num = tdesc->begin;
  unsigned int i = compute_i_index(idx_i, task_num);
  unsigned int j = compute_j_index(idx_j, task_num);
  unsigned int i_cnt, j_cnt, k;

  // Invalidate matrix A1
  for (i_cnt = 0; i_cnt < OUTPUT_PER_TASK_I; i_cnt++) {
    for (k = idx_k; k < idx_k + BLOCK_SIZE_K; k += ELEM_PER_CACHE_LINE)
    {
      RigelInvalidateLine(&matA1[i+i_cnt][k]);
    }
  }
  
  // Invalidate matrix A2
  for (k = idx_k; k < idx_k + BLOCK_SIZE_K; k += 8) {
    for (j_cnt = 0; j_cnt < OUTPUT_PER_TASK_J; j_cnt += ELEM_PER_CACHE_LINE) {
      RigelInvalidateLine(&matA2[k+0][j+j_cnt]);
      RigelInvalidateLine(&matA2[k+1][j+j_cnt]);
      RigelInvalidateLine(&matA2[k+2][j+j_cnt]);
      RigelInvalidateLine(&matA2[k+3][j+j_cnt]);
      RigelInvalidateLine(&matA2[k+4][j+j_cnt]);
      RigelInvalidateLine(&matA2[k+5][j+j_cnt]);
      RigelInvalidateLine(&matA2[k+6][j+j_cnt]);
      RigelInvalidateLine(&matA2[k+7][j+j_cnt]);
    }
  }
}

void BlockedMatMultTask() {
  TQ_TaskDesc tdesc;
  TQ_RetType retval;
  int idx_i, idx_k;
  // Count of tasks done in a particular interval on this core.
  int RTM_WB_Count = 0;
  // Array of start-end pairs to flush.
  TQ_TaskDesc RTM_WB_IndexArray[MAX_TASKS_PER_CORE];
  // As long as there are tasks to process, the loop keeps going.  Note that
  // outer and inner loops may get interleaved.  TODO: The blocking sizes here
  // should be set so that it does not change with matrix size.
  for (idx_k = 0; idx_k < DIM_N; idx_k += BLOCK_SIZE_K)
  {
    for (idx_i = 0; idx_i < DIM_M; idx_i += BLOCK_SIZE_I) 
    {
      int idx_j;
      for (idx_j = 0; idx_j < DIM_O; idx_j += BLOCK_SIZE_J) 
      {
          TQ_TaskDesc tdesc_outer; 
          tdesc_outer.v1 = idx_i; 
          tdesc_outer.v2 = idx_j;
          tdesc_outer.v3 = 0x0; 
          tdesc_outer.v4 = 0x0;
          // Total number of output elements
          const unsigned int total_work = (BLOCK_SIZE_J*BLOCK_SIZE_I) >> OUTPUT_PER_TASK_SH;
  
          // Do a block at a time.
          retval = RIGEL_LOOP(QID, total_work, 1, &tdesc_outer);
  
          while (1) 
          {
            #ifdef FAST_DEQUEUE
            RIGEL_DEQUEUE(QID, &tdesc, &retval);
            #else
            retval = RIGEL_DEQUEUE(QID, &tdesc);
            #endif
            if (retval == TQ_RET_BLOCK) continue;
            if (retval == TQ_RET_SYNC) break;
            if (retval != TQ_RET_SUCCESS) RigelAbort();
            {
              unsigned int i = compute_i_index(idx_i, tdesc.v3); 
              unsigned int j = compute_j_index(idx_j, tdesc.v3);
              unsigned int i_cnt, j_cnt, k;
              // We ignore the end field and overload it to store idx_k for
              // flushing later.
              tdesc.end = idx_k;
  
              for (i_cnt = 0; i_cnt < OUTPUT_PER_TASK_I; i_cnt++) {
                for (j_cnt = 0; j_cnt < OUTPUT_PER_TASK_J; j_cnt++) {
 
                  // Took some of the offsets computations out of the inner loop.
                  const int i_off = i + i_cnt;
                  const int j_off = j + j_cnt;
                  ElemType *matA1_arr = matA1[i_off];
                  //for (k = idx_k; k < DIM_N; k += 8)
                  for (k = idx_k; (k < (idx_k + BLOCK_SIZE_K)) && (k < DIM_N); k += 8)
                  {
                    // I do not think these consts mattered to bring out.
                    const int k0 = k + 0;
                    const int k1 = k + 1;
                    const int k2 = k + 2;
                    const int k3 = k + 3;
                    const int k4 = k + 4;
                    const int k5 = k + 5;
                    const int k6 = k + 6;
                    const int k7 = k + 7;

                     // Added the nop barriers to get better locality in the line
                     // buffer.
                    asm volatile ( "nop" );
                    ElemType matA1_0 = matA1_arr[k0]; 
                    ElemType matA1_1 = matA1_arr[k1]; 
                    ElemType matA1_2 = matA1_arr[k2]; 
                    ElemType matA1_3 = matA1_arr[k3]; 
                    ElemType matA1_4 = matA1_arr[k4]; 
                    ElemType matA1_5 = matA1_arr[k5]; 
                    ElemType matA1_6 = matA1_arr[k6]; 
                    ElemType matA1_7 = matA1_arr[k7]; 
                    asm volatile ( "nop" );
                    // The pointer accesses here are all on the same line.  LLVM
                    // was scattering them all over causing LB shootdown.
                    ElemType *matA2_0p = matA2[k0];
                    ElemType *matA2_1p = matA2[k1];
                    ElemType *matA2_2p = matA2[k2];
                    ElemType *matA2_3p = matA2[k3];
                    ElemType *matA2_4p = matA2[k4];
                    ElemType *matA2_5p = matA2[k5];
                    ElemType *matA2_6p = matA2[k6];
                    ElemType *matA2_7p = matA2[k7];
                    asm volatile ( "nop" );
                    ElemType matA2_0 = matA2_0p[j_off]; 
                    ElemType matA2_1 = matA2_1p[j_off]; 
                    ElemType matA2_2 = matA2_2p[j_off]; 
                    ElemType matA2_3 = matA2_3p[j_off]; 
                    ElemType matA2_4 = matA2_4p[j_off]; 
                    ElemType matA2_5 = matA2_5p[j_off]; 
                    ElemType matA2_6 = matA2_6p[j_off]; 
                    ElemType matA2_7 = matA2_7p[j_off]; 
                    {

                      ElemType sum0  = matA1_0 * matA2_0;
                      ElemType sum1  = matA1_1 * matA2_1;
                      ElemType sum2  = matA1_2 * matA2_2;
                               sum0 += matA1_3 * matA2_3;
                               sum1 += matA1_4 * matA2_4;
                               sum2 += matA1_5 * matA2_5;
                               sum0 += matA1_6 * matA2_6;
                               sum1 += matA1_7 * matA2_7;

                      resultA[i_off][j_off] += sum0 + sum1 + sum2;
                    }
                  }
                }
              }
              //////////  START COHERENCE ACTIONS [[ EAGER ]] ////////////////////////////////
              // Handle the flushing for this iteration.  
              if (TCMM_WRB_EAGER) {
                RTM_dmm_flush_tdesc(&tdesc);
              } 
              if (TCMM_INV_EAGER) {
                RTM_dmm_inv_tdesc(&tdesc);
              } 
              if (TCMM_INV_LAZY || TCMM_WRB_LAZY) {
                // Add tdesc to list to invalidate later.
                RTM_WB_IndexArray[RTM_WB_Count].v1 = tdesc.v1;
                RTM_WB_IndexArray[RTM_WB_Count].v2 = tdesc.v2;
                RTM_WB_IndexArray[RTM_WB_Count].v3 = tdesc.begin;
                RTM_WB_IndexArray[RTM_WB_Count].v4 = tdesc.end;
                RTM_WB_Count++;
              }
              //////////  END COHERENCE ACTIONS  [[ EAGER ]] //////////////////////////////////
            }
          }
//cnt++;          
//if (cnt >= 2) { return;           }
        //////////  START COHERENCE ACTIONS [[ LAZY ]] ////////////////////////////////
        if (TCMM_WRB_LAZY)
        {
          TQ_TaskDesc *walk = &(RTM_WB_IndexArray[RTM_WB_Count]);
          TQ_TaskDesc *end = &(RTM_WB_IndexArray[0]);
          while(walk != end)
          {
            walk--;
            RTM_dmm_flush_tdesc(walk);
          }
        }
        if (TCMM_INV_LAZY) {
          TQ_TaskDesc *walk = &(RTM_WB_IndexArray[RTM_WB_Count]);
          TQ_TaskDesc *end = &(RTM_WB_IndexArray[0]);
          while(walk != end)
          {
            walk--;
            RTM_dmm_inv_tdesc(walk);
          }
        }
        RTM_WB_Count = 0;
        //////////  END COHERENCE ACTIONS  [[ LAZY ]] //////////////////////////////////
  
        // SYNC FOUND, GOTO NEXT BLOCK
  
        // XXX: Stop early to make larger matrix runs finish sooner.
      }
//      return;
      // XXX: Only do one block of the matrix.  Remove the line for a full run.
     // break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////
////  START MAIN
////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int main() {
  int i;
  int thread = RigelGetThreadNum();

  if (thread == 0) {
    SIM_SLEEP_ON();
    
    // Set it so all can see.
    incohernt_malloc_enabled = RigelIncoherentMallocEnabled();

    fprintf(stderr, "Y\n");

    // First parameter is invalidation strategy
    RTM_SetWritebackStrategy();

    // Read input
    fscanf(stdin, "%d", &DIM_M);
    fscanf(stdin, "%d", &DIM_N);
    fscanf(stdin, "%d", &DIM_O);
    fprintf(stderr, "%dx%d * %dx%d\n", DIM_M, DIM_N, DIM_N, DIM_O);

    // Initialize matrices
    // Row pointers (Pointers are static, use incoherent_malloc)
    matA1 = (ElemType **)incoherent_malloc(sizeof(ElemType *)*DIM_M + 32);
    matA1 = (ElemType **)(((uint32_t)(matA1)+31) & (0xFFFFFFE0));
    // Data matrix (Input data is static for hybrid)
    data1 =  (ElemType *)incoherent_malloc(sizeof(ElemType)*DIM_M*DIM_N + 32);
    data1 = (ElemType *)(((uint32_t)(data1)+31) & (0xFFFFFFE0));
    for (i = 0; i < DIM_M; i++) {
      matA1[i] = data1+(i*DIM_N);
    }
    // Row pointers
    matA2 = (ElemType **)incoherent_malloc(sizeof(ElemType *)*DIM_N + 32);
    matA2 = (ElemType **)(((uint32_t)(matA2)+31) & (0xFFFFFFE0));
    // Data matrix
    data2 =  (ElemType *)incoherent_malloc(sizeof(ElemType)*DIM_N*DIM_O + 32);
    data2 = (ElemType *)(((uint32_t)(data2)+31) & (0xFFFFFFE0));
    for (i = 0; i < DIM_N; i++) {
      matA2[i] = data2+(i*DIM_O);
    }
    // Row pointers
    resultA = (ElemType **)incoherent_malloc(sizeof(ElemType *)*DIM_M + 32);
    resultA = (ElemType **)(((uint32_t)(resultA)+31) & (0xFFFFFFE0));
    // Data matrix (Output data must be kept coherent)
    data3 =  (ElemType *)incoherent_malloc(sizeof(ElemType)*DIM_M*DIM_O + 32);
    data3 = (ElemType *)(((uint32_t)(data3)+31) & (0xFFFFFFE0));
    for (i = 0; i < DIM_M; i++) {
      resultA[i] = data3+(i*DIM_O);
    }

    ClearTimer(0);
    ClearTimer(1);
    // Start full run timer
    StartTimer(0);

    RIGEL_INIT(QID, MAX_TASKS);
    // Set up the global block sizes
    BLOCK_SIZE_J = DIM_O / BLOCK_COUNT_J;
    BLOCK_SIZE_I = DIM_M / BLOCK_COUNT_I;

    // Memoize the mask/shift for block size I and J
    for (i = 31; i > 0; i--) {
      if ((0x1UL << i) & BLOCK_SIZE_I) {
        BLOCK_SIZE_I_MASK = (0x1UL << i) -1; 
        BLOCK_SIZE_I_SH = i;
        break;
      }
    }
    for (i = 31; i > 0; i--) {
      if ((0x1UL << i) & BLOCK_SIZE_J) {
        BLOCK_SIZE_J_MASK = (0x1UL << i) -1; 
        BLOCK_SIZE_J_SH = i;
        break;
      }
    }


// Initialize the matricies to something sane.
    {
      int i = 0, j = 0;
      for (i = 0; i < DIM_M; i++) {
        for (j = 0; j < DIM_O; j++) {
          resultA[i][j] = 0;
        }
      }
      for (i = 0 ; i < DIM_M; i++) {
        for (j = 0; j < DIM_N; j++) {
          matA1[i][j] = j+1;
        }
      }
      for (i = 0 ; i < DIM_N; i++) {
        for (j = 0; j < DIM_O; j++) {
          matA2[i][j] = i+j;
        }
      }
    }


    // Do DMM totally data parallel (no GTQ usage)
    SW_TaskQueue_Set_DATA_PARALLEL_MODE();

    SIM_SLEEP_OFF();

    atomic_flag_set(&Init_flag);
  }

  atomic_flag_spin_until_set(&Init_flag);
  BlockedMatMultTask();

  // Stop full-run timer
  if (thread == 0)
  {

  // Dump the matrix output
    int i = 0, j = 0;
    for (i =0; i < DIM_M; i++) {
      for (j = 0; j < DIM_O; j++) {
        RigelPrint(resultA[i][j]);
      }
      RigelPrint(0xABABABAB);
    }

    StopTimer(0);
  }

  return 0;
}
