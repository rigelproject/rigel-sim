////////////////////////////////////////////////////////////////////////////////
/// src/caches/cache_model_instr.cpp
////////////////////////////////////////////////////////////////////////////////
///
///  This file contains the Includes instruction cache interaction with core
///  model.
///
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for NULL, fprintf, stderr
#include "cache_model.h"    // for CacheModel
#include "caches/cache_base.h"  // for CacheAccess_t, etc
#include "caches/l1d.h"     // for L1DCache
#include "caches/l1i.h"     // for L1ICache
#include "caches/l2d.h"     // for L2Cache
#include "caches/l2i.h"     // for L2ICache
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for DEBUG_HEADER, etc
#include "profile/profile.h"        // for CacheStat, Profile, etc
#include "sim.h"            // for USE_L2I_CACHE_UNIFIED, etc
#include "memory/backing_store.h" //for GlobalBackingStoreType definition

#define DEBUG_INSTR_ACCESS_DUMP() \
    DEBUG_HEADER(); \
    fprintf(stderr, "[[ IFETCH THIS IS BUSTED!!! L1I[core] invalid ]] " \
                    "cluster: %d core: %4d addr: 0x%08x L2.set: %d L2.valid: %2x " \
                    "L2.IsPending() %1d L2.IsValidWord() %1d L2.IsValidLine() %1d " \
                    "L1D.IsPending() %1d L1D.IsValidWord() %1d L1D.IsValidLine() %1d \n", \
                      cluster_num, core, addr, L2.get_set(addr), L2.getValidMask(addr),\
                      (L2.IsPending(addr) != NULL), L2.IsValidWord(addr), \
                      L2.IsValidLine(addr), (L1D[core].IsPending(addr) != NULL),\
                      L1D[core].IsValidWord(addr), L1D[core].IsValidLine(addr));



///////////////////////////////////////////////////////////////////////////////
/// read_access_instr()
///////////////////////////////////////////////////////////////////////////////
///
/// Cache access for INSTRUCTIONS
///   sets stall to true on stall (miss)
///   sets data to returned data when valid (not stalled)
///
/// On L2I miss, track ICMsg stall reasons to attach to instruction
/// This is not currently done because the InstrLegacy doesn't exist until the data is
/// returned from the cache model.
///
///////////////////////////////////////////////////////////////////////////////
MemoryAccessStallReason
CacheModel::read_access_instr(int core, uint32_t addr,
  uint32_t &data, bool &stall, bool &l2i_hit, bool &l1i_hit, int tid)
{
  using namespace rigel::cache;
  using namespace rigel;

  // Idealized RTM D$ accesses.
  bool is_freelibpar =    (addr > CODEPAGE_LIBPAR_BEGIN 
                        && addr < CODEPAGE_LIBPAR_END 
                        && CMDLINE_ENABLE_FREE_LIBPAR_ICACHE) ? true : false;

  // Check the WDT first for the access.
  if (memory_retry_watchdog_count_instr[core]++ > (2*CACHEBASE_MEMACCESS_ATTEMPTS_WDT)) {
    DEBUG_HEADER();
    fprintf(stderr, "[[ CacheModel::instr_access() WDT ]] Retry count exceeded (%d)\n",
                    2*CACHEBASE_MEMACCESS_ATTEMPTS_WDT);
    DEBUG_INSTR_ACCESS_DUMP();
    rigel::GLOBAL_debug_sim.dump_all();
    assert(0);
  }

  MemoryAccessStallReason masr = MemoryAccessStallReasonBug;

  // For profiling.  Include cases when we have perfect caches enabled.
  l1i_hit = L1I[core].IsValidLine(addr) || PERFECT_L1I || is_freelibpar;
  if (USE_L2I_CACHE_UNIFIED) {
    l2i_hit = L2.IsValidLine(addr) || PERFECT_L2I || PERFECT_L2D;
  } else {
    l2i_hit = L2I.IsValidLine(addr) || PERFECT_L2I;
  }

  // Increment the appropriate ICache profiler stat.
  if (USE_L2I_CACHE_SEPARATE) { L2I.ICacheProfilerTouch(addr, tid); }
  if (USE_L2I_CACHE_UNIFIED)  { L2.ICacheProfilerTouch(addr, tid); }

  // Default values
  stall = true;
  data = 0xDEADB11F;
  icmsg_type_t icmsg_type;

  // If a unified L2 is used, instruction fills and data reads are treated the
  // same way.  If the message type is IC_MSG_INSTR_READ_REQ, the fill will go
  // to the L2I.  If it is IC_MSG_READ_REQ, it will go to the L2D (Unified).
  icmsg_type = USE_L2I_CACHE_UNIFIED ? IC_MSG_READ_REQ :
                                       IC_MSG_INSTR_READ_REQ;

  // FIXME: make a similar input parameter... (some new fields are filled in
  // within this func, such as icmsg_type)
  CacheAccess_t ca(
    addr,              // address
    core,              // core number
    tid,               // thread id
    icmsg_type,        // interconnect message type
    rigel::NullInstr // no instruction correlated to this access
  );

  // Disallow unaligned instruction cache accesses.
  assert(((addr & 0x00000003) == 0)  && "Unaligned icache access!");


  // If there are pending requests, treat the access as a miss and try later.
  // In a real system we would check the status at the L1I and, if pending,
  // stall.  Then we would move on to checking the L2.  We check for pending
  // status here to keep the cache model from allocating another MSHR to the
  // same address.  It is partially vestigial, partially redundant.  We could
  // look into removing it at some point but I would rather not break the
  // simulator over it.
  //check the L2(D) if unified, L2I if separate, neither if no second-level I$
  MissHandlingEntry<rigel::cache::LINESIZE> *mshr =
        USE_L2I_CACHE_UNIFIED ? L2.IsPending(addr) : L2I.IsPending(addr);

  if (mshr != NULL)
  {  // do nothing now, leaving in some debug hooks
#ifdef DEBUG_CACHES
    fprintf(stderr,"INSTR L2 Pending\n");
#endif
    //if(mshr->_ic_msg_type == IC_MSG_PREFETCH_BLOCK_GC_REQ)
    //  printf("INSTR PENDING ON BULKPF\n");
    //TODO: Does sleeping on L2I misses make sense?
    //mshr->AddCore(core);
    //Profile::accum_cache_stats.L2I_pending_misses++;
    //masr = L2IPending;
    //fprintf(stderr,"Cycle:%d %d %08X L2 PENDING\n", rigel::CURR_CYCLE, core, addr);
    //goto L2ICacheMiss;
  }

  // If the core requests an address that is already an outstanding miss at the
  // L1I cache, it returns a stall.
  if (L1I[core].IsPending(addr) != NULL)
  {
    Profile::accum_cache_stats.L1I_pending_misses++;
		//FIXME 20110212 We used to use masr for profiling of *why* cache accesses stalled.
		//Now they appear to be unused. Why?
    //masr = L1IPending;
		stall = true;
#ifdef DEBUG_CACHES
    DEBUG_HEADER();
    fprintf(stderr,"Cycle:%d %d %08X L1I L1PENDING\n", rigel::CURR_CYCLE, core, addr);
#endif
    return helper_return_instr_stall(stall, L1IPending);
  }

  /////////////////////////////////////////////////////////////////////////////
  // ******************* L1 I-cache hit ********************
  /////////////////////////////////////////////////////////////////////////////
  if (l1i_hit) {
    // Found the instruction cached in the L1I.  Touch it so that the LRU status
    // is updated.
    L1I[core].touch(addr);
    // Read the actual bits from the memory model.
    data = backing_store.read_word(addr);
    // Since it is a hit, no stalling.  Return the fetch this cycle.
    return helper_return_instr_success(stall, NoStall, data, addr, core, tid);
  }
  // L1I Hits never get to this point.

  else {
    // There are no pending MSHR's for the L1I so we will have to replay the
    // request next cycle.
    // FIXME is_being_evicted() might be better off as its own stall reason,
    // but the closest existing one is L1IMSHR, since is_being_evicted() should only be
    // caused by the eviction waiting on an MSHR.
    if ( L1I[core].mshr_full() || L1I[core].is_being_evicted(addr)) {
      Profile::accum_cache_stats.L1I_MSHR_stall_cycles++;
      return helper_return_instr_stall(stall, L1IMSHR);
    }

    // post L1I miss to an MSHR. Latency from this call will be ignored
    // since it is not scheduled yet. pend() will use variable latency instead
    L1I[core].pend( ca, -1, -1, false, true );
    //L1I[core].pend( ca );

    // FIXME do prefetches here?...
    
    #ifdef DEBUG_CACHES
    DEBUG_HEADER();
    fprintf(stderr, "L1ICache MISS addr 0x%08x core %d thread %d\n", addr, core,tid);
    #endif

    // Stall this cycle, but will hit in the L1I in a later cycle.
    return helper_return_instr_stall(stall, L2IAccess);

  }

}

