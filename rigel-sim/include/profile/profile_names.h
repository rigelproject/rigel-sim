#ifndef __PROFILE_NAMES_H__
#define __PROFILE_NAMES_H__

// All of the profiling statistics currently supported.  TODO: We should go back
// and comment these one-by-one eventually
typedef enum {
  // Histogram of number of sharers per directory entry at an invalidation.
  STATNAME_DIR_SHARER_COUNT,
  // FIXME removed until used
  //STATNAME_ATOMICS_INSTR_MIX_RATIO,
  //STATNAME_ATOMICS_INSTR_PER_CYCLE,
  STATNAME_BRANCH_TARGET,
  STATNAME_CCACHE_HWPREFETCH_TOTAL,
  STATNAME_CCACHE_HWPREFETCH_USEFUL,
  STATNAME_CCACHE_OUTBOUND_QUEUE,
  STATNAME_CCACHE_PENDING_OUTBOUND_QUEUE,
  STATNAME_DRAM_BUS_BUSY_CYCLES,
  STATNAME_GCACHE_HWPREFETCH_TOTAL,
  STATNAME_GCACHE_HWPREFETCH_USEFUL,
  STATNAME_GCACHE_PORT,
  STATNAME_BULK_PREFETCH_TOTAL,
  STATNAME_BULK_PREFETCH_USEFUL,
  STATNAME_DIRECTORY,
  STATNAME_OVERFLOW_DIRECTORY_TOUCHES,
  STATNAME_DIRECTORY_EVICTIONS,
  STATNAME_DIRECTORY_EVICTION_SHARERS,
  STATNAME_DIRECTORY_REREQUESTS,
  // FIXME removed until used
  //STATNAME_GLOBAL_INSTR_MIX_RATIO,
  //STATNAME_GLOBAL_ATOMICS_INSTR_MIX_RATIO,
  STATNAME_GLOBAL_CACHE,
  STATNAME_HISTOGRAM_GLOBAL_CACHE_MISSES,
  STATNAME_HISTOGRAM_L2I_CACHE_MISSES,
  STATNAME_HISTOGRAM_L2D_CACHE_MISSES,
  STATNAME_HISTOGRAM_L1I_CACHE_MISSES,
  STATNAME_HISTOGRAM_L1D_CACHE_MISSES,
  // Number of accesses that are for reads and writes.  Does not count cohernece
  // messages and evictions etc.
  STATNAME_GLOBAL_CACHE_RW_ACCESSES,
  STATNAME_GLOBAL_CACHE_OF_ACCESSES,
  STATNAME_INSTR_CC_OCCUPANCY,
  STATNAME_INSTR_DE_OCCUPANCY,
  STATNAME_INSTR_EX_OCCUPANCY,
  STATNAME_INSTR_FP_OCCUPANCY,
  STATNAME_INSTR_GC_FILL,
  STATNAME_INSTR_GC_MSHR,
  STATNAME_INSTR_GC_PENDING,
  STATNAME_INSTR_GC_WRITE_ALLOCATE,
  STATNAME_INSTR_GNET_GCREPLY,
  STATNAME_INSTR_GNET_GCREQUEST,
  STATNAME_INSTR_GNET_REPLYBUFFER,
  STATNAME_INSTR_GNET_REQUESTBUFFER,
  STATNAME_INSTR_GNET_TILEREPLY,
  STATNAME_INSTR_GNET_TILEREQUEST,
  STATNAME_INSTR_IF_OCCUPANCY,
  STATNAME_INSTR_INSTR_STALL_CYCLES,
  STATNAME_INSTR_L1D_MSHR,
  STATNAME_INSTR_L1D_PENDING,
  STATNAME_INSTR_L1I_MSHR,
  STATNAME_INSTR_L1I_PENDING,
  STATNAME_INSTR_L1ROUTER_REPLY,
  STATNAME_INSTR_L1ROUTER_REQUEST,
  STATNAME_INSTR_L2D_ACCESS,
  STATNAME_INSTR_L2D_ARBITRATE,
  STATNAME_INSTR_L2D_MSHR,
  STATNAME_INSTR_L2D_PENDING,
  STATNAME_INSTR_L2GC_REPLY,
  STATNAME_INSTR_L2GC_REQUEST,
  STATNAME_INSTR_L2I_ACCESS,
  STATNAME_INSTR_L2I_ARBITRATE,
  STATNAME_INSTR_L2I_MSHR,
  STATNAME_INSTR_L2I_PENDING,
  STATNAME_INSTR_L2ROUTER_REPLY,
  STATNAME_INSTR_L2ROUTER_REQUEST,
  STATNAME_INSTR_MC_OCCUPANCY,
  STATNAME_INSTR_RESOURCE_CONFLICT_ALU,
  STATNAME_INSTR_RESOURCE_CONFLICT_ALU_SHIFT,
  STATNAME_INSTR_RESOURCE_CONFLICT_BOTH,
  STATNAME_INSTR_RESOURCE_CONFLICT_BRANCH,
  STATNAME_INSTR_RESOURCE_CONFLICT_FPU,
  STATNAME_INSTR_RESOURCE_CONFLICT_MEM,
  STATNAME_INSTR_SENDING_BCAST_NOTIFIES,
  STATNAME_INSTR_STUCK_BEHIND_GLOBAL,
  STATNAME_INSTR_STUCK_BEHIND_OTHER_INSTR,
  STATNAME_INSTR_SYNC_WAITING_FOR_ACK,
  STATNAME_INSTR_TILE_REPLYBUFFER,
  STATNAME_INSTR_TILE_REQUESTBUFFER,
  STATNAME_INSTR_WB_OCCUPANCY,
  STATNAME_L1D_CACHE,
  STATNAME_L1I_CACHE,
  STATNAME_L1_ROUTER_HISTOGRAM_REPLY,
  STATNAME_L1_ROUTER_HISTOGRAM_REQ,
  STATNAME_L2D_CACHE,
  STATNAME_L2I_CACHE,
  STATNAME_L2_ROUTER_HISTOGRAM_REPLY,
  STATNAME_L2_ROUTER_HISTOGRAM_REQ,
  STATNAME_LOCAL_ATOMICS,
  STATNAME_GLOBAL_ATOMICS,
  STATNAME_ATOMICS,
  STATNAME_GLOBAL_MEMORY,
  STATNAME_LOCAL_MEMORY,
  STATNAME_MC_FORCE_SCHEDULE,
  STATNAME_MC_SCHED_ROW_HIT,
  STATNAME_MC_LATENCY,
  STATNAME_MC_REORDERING,
  STATNAME_NBL_QUEUE_LENGTH,
  STATNAME_PROFILER_ACTIVE_CYCLES,
  STATNAME_RETIRED_INSTRUCTIONS,
  STATNAME_ONE_OPR_INSTRUCTIONS,
  STATNAME_ONE_OPR_BYPASSED,
  STATNAME_TWO_OPR_INSTRUCTIONS,
  STATNAME_TWO_OPR_BYPASSED_ONE,
  STATNAME_TWO_OPR_BYPASSED_TWO,
  STATNAME_TILE_INTERCONNECT_INJECTS,
  STATNAME_DRAM_MLP,
  STATNAME_DRAM_BLP,
  STATNAME_DRAM_CHANNEL_LOAD_BALANCE,
  STATNAME_DRAM_BANK_LOAD_BALANCE,
  // FIXME removed until used
  //STATNAME_SIM_CYCLES,
  //STATNAME_CYCLES_PER_SECOND_TOTAL,
  //STATNAME_SIMULATION_TIME_STRING,
  //STATNAME_SIMULATION_TIME_USEC,
  //STATNAME_DUMPFILE_PREFIX,
  //STATNAME_DUMPFILE_PATH,
  //STATNAME_RTM_TASK_LENGTH,
  //STATNAME_RTM_ENQUEUE,
  //STATNAME_RTM_ENQUEUE_GROUP,
  //STATNAME_RTM_DEQUEUE,
  //STATNAME_RTM_BARRIER,
  //STATNAME_RTM_IDLE,
  //STATNAME_TASKS_PER_BARRIER,
  //STATNAME_TQ_ENQUEUE_COUNT,
  //STATNAME_TQ_DEQUEUE_COUNT,
  //STATNAME_TQ_LOOP_COUNT,
  //STATNAME_TQ_INIT_COUNT,
  //STATNAME_TQ_END_COUNT,
  STATNAME_DRAM_CMD_READ,
  STATNAME_DRAM_CMD_IDLE,
  STATNAME_DRAM_CMD_ACTIVATE,
  STATNAME_DRAM_CMD_PRECHARGE,
  STATNAME_DRAM_CMD_WRITE,
  // Mean number of cycles spent for each load miss (L1D)
  STATNAME_ATTEMPTS_LDW_MISS_L1D,
  STATNAME_ATTEMPTS_LDL_MISS_L1D,
  // Mean number of cycles spent for each load hit (L1D)
  STATNAME_ATTEMPTS_LDW_HIT_L1D,
  STATNAME_ATTEMPTS_LDL_HIT_L1D,
  // Mean number of cycles spent for each load miss (L2D)
  STATNAME_ATTEMPTS_LDW_MISS_L2D,
  STATNAME_ATTEMPTS_LDL_MISS_L2D,
  // Mean number of cycles spent for each load hit (L2D)
  STATNAME_ATTEMPTS_LDW_HIT_L2D,
  STATNAME_ATTEMPTS_LDL_HIT_L2D,
  // Mean number of cycles spent for each store miss (L1D)
  STATNAME_ATTEMPTS_STW_MISS_L1D,
  STATNAME_ATTEMPTS_STC_MISS_L1D,
  // Mean number of cycles spent for each store hit (L1D)
  STATNAME_ATTEMPTS_STW_HIT_L1D,
  STATNAME_ATTEMPTS_STC_HIT_L1D,
  // Mean number of cycles spent for each store miss (L2D)
  STATNAME_ATTEMPTS_STW_MISS_L2D,
  STATNAME_ATTEMPTS_STC_MISS_L2D,
  // Mean number of cycles spent for each store hit (L2D)
  STATNAME_ATTEMPTS_STW_HIT_L2D,
  STATNAME_ATTEMPTS_STC_HIT_L2D,
  // The number of evictions from the directory.
  STATNAME_COHERENCE_EVICTS,
  // Count of instructions to commit from each functional unit.
  STATNAME_COMMIT_FU_NONE,
  STATNAME_COMMIT_FU_ALU,
  STATNAME_COMMIT_FU_ALU_VECTOR,
  STATNAME_COMMIT_FU_FPU,
  STATNAME_COMMIT_FU_FPU_VECTOR,
  STATNAME_COMMIT_FU_FPU_SPECIAL,
  STATNAME_COMMIT_FU_ALU_SHIFT,
  STATNAME_COMMIT_FU_SPFU,
  STATNAME_COMMIT_FU_COMPARE,
  STATNAME_COMMIT_FU_MEM,
  STATNAME_COMMIT_FU_BRANCH,
  STATNAME_COMMIT_FU_BOTH,
  STATNAME_COMMIT_FU_COUNT,
  // Histogram the maximum number of concurrent sharers a directory entry has before being evicted.
  STATNAME_DIR_MAX_SHARERS,
  // Count the number and type of invalidation messages sent.
  STATNAME_DIR_INVALIDATE_REG,
  STATNAME_DIR_INVALIDATE_BCAST,
  STATNAME_DIR_INVALIDATE_WR,
  // The number of attempted HW prefetches at the cluster cache that get dropped.
  STATNAME_DROPPED_HW_CCACHE_PREFETCHES,
  STATNAME_DIR_WR2WR_TXFR,
  // Number of evictions from the cluster cache that are dirty.
  STATNAME_DIR_L2D_DIRTY_EVICTS,
  // Number of evictions from the cluster cache that require read releases.
  STATNAME_DIR_L2D_READ_RELEASES,
  // Number of accesses that are ruled incoherent by the hybrid coherence
  // module.
  STATNAME_DIR_INCOHERENT_ACCESSES,
  // Histogram of wait time to arb on the L2 bus.
  STATNAME_L2_ARB_TIME,
  // Message classes for in/out of L2.
  STATNAME_L2OUT_TOTAL,
  STATNAME_L2OUT_READ_NONSTACK_REQ,
  STATNAME_L2OUT_READ_STACK_REQ,
  STATNAME_L2OUT_INSTR_REQ,
  STATNAME_L2OUT_WRITE_NONSTACK_REQ,
  STATNAME_L2OUT_WRITE_STACK_REQ,
  STATNAME_L2OUT_WRITEBACK_REQ,
  STATNAME_L2OUT_COHERENCE_REPLY,
  STATNAME_L2OUT_GLOBAL_REQ,
  STATNAME_L2OUT_ATOMIC_REQ,
  STATNAME_L2OUT_PREFETCH_REQ,
  STATNAME_L2OUT_EVICT_NONSTACK_REQ,
  STATNAME_L2OUT_EVICT_STACK_REQ,
  STATNAME_L2OUT_READ_RELEASE_NONSTACK,
  STATNAME_L2OUT_READ_RELEASE_STACK,
  STATNAME_L2IN_TOTAL,
  STATNAME_L2IN_READ_REPLY,
  STATNAME_L2IN_INSTR_REPLY,
  STATNAME_L2IN_WRITE_REPLY,
  STATNAME_L2IN_WRITEBACK_REPLY,
  STATNAME_L2IN_GLOBAL_REPLY,
  STATNAME_L2IN_ATOMIC_REPLY,
  STATNAME_L2IN_PREFETCH_REPLY,
  STATNAME_L2IN_EVICT_REPLY,
  STATNAME_L2IN_COHERENCE_PROBE,
  STATNAME_L2IN_READ_RELEASE,
  // Count of how many programmed invalidates/writebacks were useful.
  STATNAME_L2_INV_USEFUL,
  STATNAME_L2_INV_WASTED,
  STATNAME_L2_WB_USEFUL,
  STATNAME_L2_WB_WASTED,
  // Histogram of directory entry types.
  STATNAME_DIR_ENTRIES_CODE,
  STATNAME_DIR_ENTRIES_STACK,
  STATNAME_DIR_ENTRIES_OTHER,
  STATNAME_DIR_ENTRIES_INVALID,
  // Histogram of number of outstanding coherence broadcasts
  STATNAME_OUTSTANDING_COHERENCE_BCASTS,
  // Count the number of L2 misses over L2D_MISS_LAT_PROFILE_MAX cycles
  STATNAME_L2D_PROFILE_IGNORE,
  // Number of allocations for the overflow lists.
  STATNAME_OVERFLOW_DIRECTORY_ALLOCS,
  // END OF LIST
  STATNAME_PROFILE_STAT_COUNT
} profile_stat_name_t;

#endif
