////////////////////////////////////////////////////////////////////////////////
/// src/caches/cache_model_read.cpp
////////////////////////////////////////////////////////////////////////////////
///
///  This file contains the includes data cache interaction with core model.
///
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                 // for assert
#include <inttypes.h>               // for PRIu64
#include <stdint.h>                 // for uint32_t
#include <stdio.h>                  // for fprintf, NULL, stderr
#include "memory/address_mapping.h" // for AddressMapping
#include "broadcast_manager.h"      // for BroadcastManager
#include "caches_legacy/cache_model.h"            // for CacheModel
#include "caches_legacy/cache_base.h"      // for CacheAccess_t, etc
#include "caches_legacy/global_cache.h"    // for GlobalCache
#include "caches_legacy/l1d.h"             // for L1DCache, NoL1DCache
#include "caches_legacy/l2d.h"             // for L2Cache
#include "util/debug.h"             // for DebugSim, GLOBAL_debug_sim
#include "define.h"                 // for DEBUG_HEADER, etc
#include "icmsg.h"                  // for ICMsg
#include "instr.h"                  // for InstrLegacy
#include "instrstats.h"             // for InstrStats
#include "mshr.h"                   // for MissHandlingEntry
#include "profile/profile.h"        // for ProfileStat
#include "profile/profile_names.h"
#include "sim.h"                    // for L1D_ENABLED, etc
#include "memory/backing_store.h"   // for GlobalBackingStoreType definition
#if 0
#define __DEBUG_ADDR 0x00000000
#ifdef __DEBUG_CYCLE
#undef __DEBUG_CYCLE
#endif
#define __DEBUG_CYCLE 0x0
#else
#undef __DEBUG_ADDR
#undef __DEBUG_ADDR_SHORT
#undef __DEBUG_CYCLE 
//#define __DEBUG_CYCLE 0xa0000
//#define __DEBUG_ADDR_SHORT 0x004a3a0
// //#define __DEBUG_ADDR 0x004a3a0
#endif

#define DEBUG_READ_ACCESS_DUMP() \
    DEBUG_HEADER(); \
    { \
      fprintf(stderr, "[[ READ DATA ]](warn-use L1DCacheID not tid) " \
                      "cluster: %d core: %4d tid: %2d addr: 0x%08x pc: 0x%08x L2.set: %d L2.valid: %2x " \
                      "L2.IsPending() %1d L2.IsValidWord() %1d L2.IsValidLine() %1d L2.NotInterlocked %1d " \
                      "L1D.IsPending() %1d L1D.IsValidWord() %1d L1D.IsValidLine() %1d " \
                      "L2.IsDirtyLine() %1d \n", \
                        cluster_num, core, tid, addr, instr.get_currPC(), L2.get_set(addr), L2.getValidMask(addr),\
                        (L2.IsPending(addr) != NULL), L2.IsValidWord(addr), \
                        L2.IsValidLine(addr), L2.nonatomic_ccache_op_lock(tid, addr), (L1D[tid].IsPending(addr) != NULL),\
                        L1D[tid].IsValidWord(addr), L1D[tid].IsValidLine(addr), \
                        L2.IsDirtyLine(addr));\
    }

#ifdef __DEBUG_ADDR
  #define DEBUG_READ_ACCESS(str) \
    if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) { \
      if (cluster_num == 11) { \
        DEBUG_HEADER(); \
        fprintf(stderr, "[CACHEMODEL READ] %s  addr: 0x%08x core: %4d " \
                      "cluster: %d\n", str, addr, core, cluster_num);  \
      }\
    }
#else
  #define DEBUG_READ_ACCESS(str)
#endif


MemoryAccessStallReason
CacheModel::helper_return_read_success(bool &stall,
    MemoryAccessStallReason masr, uint32_t &data, uint32_t addr,
    InstrLegacy &instr, int core, int tid)
{
  using namespace rigel;

  int L1CacheID = GetL1DID(tid);

  // Touch the cache line to insert this cycle as the last-touch cycle in
  // the CacheTag for the line on the L1Cache for this core/thread
  if (L1D_ENABLED) {
    L1D[L1CacheID].touch(addr);
  }

  thread_arb_complete[tid] = false;

  // Usually, there is only one canonical value for each memory address in the
  // simulator.  Technically, different cluster/L1 caches could contain different
  // values for the same location, but this would be difficult to make work correctly,
  // as "your" value could be overwritten by somebody else's at any time, subject to
  // the whims of your (and your neighbors') cache replacement policy.
  // NOTE that this is not true if we have a way to implicitly or explicitly lock data into the cache,
  // so if we implement data locking (and we should, at some point), we need to upgrade the memory
  // model in the simulator to deal with distinct values for the same location in each cache.
  // ANYWAY: in the current memory model, the only time a location can have multiple values is during
  // a broadcast update operation.  We ask the broadcast_manager (a simulator artifact that knows
  // about all broadcasts in the system) if a broadcast update is pending to the requested address.
  // If it is, we ask the broadcast manager to give us the correct version of the data by passing in the
  // address and our cluster number.  The broadcast manager knows whether or not we've seen the update
  // (because we call it's ack() method when we get the ic_msg), so it can pass us the new or old value
  // appropriately.
  // If there's no broadcast pending to this address, just read the value out of the global memory model.
  if (broadcast_manager->isBroadcastOutstanding(addr)) {
    data = broadcast_manager->getData(addr, cluster_num);
  } else {
    data = backing_store.read_data_word(addr);
  }

  // Reset the NoL1D so that no real caching occurs
  if (!L1D_ENABLED) { NoL1D[L1CacheID].clear(addr); }

  // Save the number of attempts for profiling later.
  instr.stats.mem_polling_retry_attempts = memory_retry_watchdog_count[tid]; // track per thread
  // Reset WDT on successful read.
  memory_retry_watchdog_count[tid] = 0; // track per thread
  // We succeeded, set the accessed bit.
  L2.set_accessed_bit_read(addr);
  L1D[L1CacheID].set_accessed_bit_read(addr);
  L1D[L1CacheID].set_temporary_access_complete(addr);

  DEBUG_READ_ACCESS("Success.");

#ifdef __DEBUG_ADDR_SHORT
if (__DEBUG_ADDR_SHORT == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
  DEBUG_HEADER();
  fprintf(stderr, "[CACHEMODEL READ] Setting Accessed bit addr: 0x%08x core: %4d "
                "cluster: %d\n", addr, core, cluster_num);
}
#endif
  // We can unlock the address for the core once we succeed.
  L2.nonatomic_ccache_op_unlock(tid, addr);

  #if 0
  // Simple debug check.  Make sure that this read does not exist in another
  // cache dirty.
  list<CacheModel *>::iterator iter;
  for ( iter = GLOBAL_debug_sim.list_cluster_cache_models.begin(); 
        iter != GLOBAL_debug_sim.list_cluster_cache_models.end();
        iter++)
  {
    // Skip yourself
    if ((*iter)->cluster_num != cluster_num) {
      if ((*iter)->L2.IsDirtyWord(addr) ) {
        DEBUG_HEADER();
        fprintf(stderr, "From PC %08x, Addr %08x is dirty in cluster: %d and read in cluster: %d!\n",
          instr.get_currPC(), addr, (*iter)->cluster_num, cluster_num);
      }
    }
  }
  #endif

  stall = false;
  return masr;
}

///////////////////////////////////////////////////////////////////////////////
// read_access
///////////////////////////////////////////////////////////////////////////////
// returns:
//   sets "data" if data is available
//   sets stall to true or false
///////////////////////////////////////////////////////////////////////////////
MemoryAccessStallReason
CacheModel::read_access(InstrLegacy &instr, int core, uint32_t addr,
  uint32_t &data, bool &stall, bool &l3d_hit, bool &l2d_hit, bool &l1d_hit, int cluster, int tid)
{
  using namespace rigel::cache;
  using namespace rigel;

#ifdef DEBUG_CACHES
  DEBUG_HEADER();
  cerr << " Addr=0x" << std::hex << addr << std::endl;
#endif

  int L1CacheID = GetL1DID(tid);
  assert(L1CacheID>=0 && L1CacheID<rigel::L1DS_PER_CLUSTER && "Illegal L1CacheID!");

  // Idealized RTM D$ accesses.
  bool is_freelibpar =    (instr.get_currPC() > CODEPAGE_LIBPAR_BEGIN 
                        && instr.get_currPC() < CODEPAGE_LIBPAR_END 
                        && CMDLINE_ENABLE_FREE_LIBPAR) ? true : false;
  
  // don't allow anyone but core/cluster/thread 0 to run in SIM_SLEEP mode  
  // this should not increment the watchdog because we are not really running
  // fixes the problem of other threads on core 0 running when in SIM_SLEEP mode
  if (rigel::SPRF_LOCK_CORES && !(core == 0 && cluster == 0)) // && tid == 0)) 
  {
    stall = true;
    return  L2DPending;
  }

  // Check the WDT first for the access.
  if (memory_retry_watchdog_count[tid]++ > CACHEBASE_MEMACCESS_ATTEMPTS_WDT) {
    rigel_dump_backtrace();
    rigel::GLOBAL_debug_sim.dump_all();
    DEBUG_HEADER();
    fprintf(stderr, "[[ CacheModel::read_access() WDT ]] Retry count exceeded (%d)\n",
                    CACHEBASE_MEMACCESS_ATTEMPTS_WDT);
    fprintf(stderr, "PC: 0x%08x ADDR: 0x%08x Cycle: %"PRIu64" \n", instr.get_currPC(), addr, rigel::CURR_CYCLE );
    DEBUG_READ_ACCESS_DUMP();
    for (int i = 0; i < L1DS_PER_CLUSTER; i++) {
      fprintf(stderr, "L1D: %d L1D: isPending %1d isValid %1d isDirty %1d\n",
        i, (NULL != L1D[i].IsPending(addr)), L1D[i].IsValidLine(addr), L1D[i].IsDirtyLine(addr));
      fprintf(stderr, "GMemOpOutstanding %1d GMemOpCompleted %1d\n",
        L2.GlobalMemOpOutstanding[i], L2.GlobalMemOpCompleted[i]);
    }
    L2.dump_nonatomic_state();
    for (int i = 0; i < L1DS_PER_CLUSTER; i++) {
      fprintf(stderr, "L1D[%3d] pending: %1d valid: %1d addr: %08x\n ", i, NULL != L1D[i].IsPending(addr),  L1D[i].IsValidWord(addr), addr );
    }
    assert(0 && "memory_retry_watchdog_count exceeds CACHEBASE_MEMACCESS_ATTEMPTS_WDT!");
  }

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_READ_ACCESS_DUMP();
  }
  #endif

  // We can do the profiling touch every access since it is idempotent.  The
  // CCacheProfiler is used to track the number of cores that access a cache
  // line while it is resident in the L2.
  L2.CCacheProfilerTouch(addr, tid, true /* read access */);

  // Any time we read a value, check the utility of the prefetch, and then clear
  // the prefetch bit.  We also check to see if we need to add more prefetches
  // so we can get a stream going.
  if (L2.IsHWPrefetch(addr))
  {
    L2.ClearHWPrefetch(addr);
    profiler::stats[STATNAME_CCACHE_HWPREFETCH_USEFUL].inc();
  }

  // For profiling
  l1d_hit = L1D[L1CacheID].IsValidWord(addr) || PERFECT_L1D || is_freelibpar;
  l2d_hit = L2.IsValidWord(addr) || PERFECT_L2D;
  // Only used for approximating global cache perf.  It is slightly inaccurate.
  l3d_hit = (GLOBAL_CACHE_PTR[AddressMapping::GetGCacheBank(addr)]->IsValidLine(addr)) || PERFECT_GLOBAL_CACHE;

  // Set the start time for the operation.  The method is idempodent.  Note that
  // we wait until this request is not going to stall to start the timer that
  // way we do not accumulate the time for a long-latency op in front of us too.
  instr.set_first_memaccess_cycle();


  // Default values
  stall = true;
  data = 0xDEADBEEF;
  icmsg_type_t icmsg_type = rigel::instr_to_icmsg(instr.get_type());

  // If a miss request is already pending at the L2, the core waits.  There is a
  // little bit of an inaccuracy in modeling the L2 this way since a core does
  // not generate a request to the CCache controller on its first access if
  // another core already issued a request to the same core.  The inaccuracy will
  // be corrected when we integrate the new ccache model.
  // MissHandlingEntry<rigel::cache::LINESIZE> *mshr = L2.IsPending(addr);
  // if (mshr != NULL) { mshr->AddCore(core); return helper_return_read_stall(stall, L2DPending); }

  // If there is an L1D, check for a pending miss.
  if (L1D_ENABLED)
  {
    if (L1D[L1CacheID].IsPending(addr) != NULL) {
#ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L1D Pending.\n");
      }
#endif
      return helper_return_read_stall(stall, L1DPending);
    } else {
#ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L1D Not pending.\n");
      }
#endif
    }
  } else {
    // In the case where there is no L1D, we use a "fake" L1D to track when a
    // request from the core is returned.  As soon as it is returned, it is
    // invalidated again so no real caching occurs.
    if (NoL1D[L1CacheID].IsValidWord(addr))
    {
      // If it is valid and ready, we return this cycle.
      if (NoL1D[L1CacheID].IsReady(addr))
      {
        // SUCCESS!
        return helper_return_read_success(stall, NoStall, data, addr, instr, core, tid);
      } else {
        // Not ready yet, but it has been request.  Try back later.
        return helper_return_read_stall(stall, L2DAccess);
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////
  // Check for an L1 hit, L2 hit, L3 hit, Memory
  ///////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////
  // ******************** L1 Data Hit *********************
  ///////////////////////////////////////////////////////////////////////////
  if ( L1D_ENABLED && l1d_hit) // skip if no L1D
  {
#ifdef __DEBUG_ADDR
if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
  DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L1D HIT.\n");
}
#endif
    // SUCCESS!
    return helper_return_read_success(stall, NoStall, data, addr, instr, core, tid);
  }

  // If we are modeling contention for the cluster cache, make a request to
  // the arbiter here.  We may want to fix this so that we do not even make a
  // request to the L2D on a miss to signal a fill until we win arbitration.
  // We may want to move this arbitration block out one level to make it
  // indepepdent of whether the request is valid in the L2 or not.
  #ifdef DEBUG_ARB
  DEBUG_HEADER();
  cerr <<"\nL2Dcache Arb call helper L2D arb core " << core <<" tid " << tid << std::endl;
  #endif
  if (!helper_arbitrate_L2D_access(addr, core, tid, L2_READ_CMD)) {
    #ifdef DEBUG_ARB
    cerr <<"L2DCache arb LOST ARB \n";
    #endif
    DEBUG_READ_ACCESS("[STALL] Lost arbitration for request bus.");
    // Lost arbitration, stall.
    return helper_return_read_stall(stall, L2DArbitrate);
  }
#ifdef __DEBUG_ADDR
if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
  DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L2D Won arbitration.\n");
}
#endif

  // If the line is currently interlocked by another core, we have to stall.
  // Note that we let arbitration and L1D accesses avoid getting stalled at the
  // interlock.
  if (!L2.nonatomic_ccache_op_lock(tid, addr)) {
    stall = true;
    return L2DAccessBitInterlock;
  }
#ifdef __DEBUG_ADDR
if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
  DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] Atomic lock not set.\n");
}
#endif

  ///////////////////////////////////////////////////////////////////////////
  // *********************** L2 Data Hit *************************
  ///////////////////////////////////////////////////////////////////////////
  if (l2d_hit)
  {
#ifdef __DEBUG_ADDR
if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
  DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L2D Hit.\n");
}
#endif
    // If there is an L1D, we will only try to fill from the L2D if there are
    // free MSHRs for the L1D.  If they are all full, we will wait until a reply
    // from the L2D clears one and try again later.
    // Also stall if addr is currently being evicted from L1D.
    if( L1D_ENABLED ) {
      if (L1D[L1CacheID].mshr_full() || L1D[L1CacheID].is_being_evicted(addr)) {
#ifdef __DEBUG_ADDR
if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
  DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L1D -- No MSHRs.\n");
}
#endif
        DEBUG_READ_ACCESS("[STALL] No L1D MSHRS.");
        // Replay request in a later cycle.
        return helper_return_read_stall(stall, L1DMSHR);
      }
    }

    if (!helper_arbitrate_L2D_return(addr, core, tid, L2_READ_RETURN)) {
#ifdef __DEBUG_ADDR
if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
  DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L2D Lost Arb.\n");
}
#endif
      DEBUG_READ_ACCESS("[STALL] Lost arbitration for return bus.");
      // Lost arbitration, stall.
      return helper_return_read_stall(stall, L2DArbitrate);
    }

    // We have access to the cluster cache this cycle.  Touch the line to update
    // LRU status.  We are going to try and fill into the L1D at this point
    // which should not fail.  The next cycle (or possibly a few later) the
    // cache model will hit the L1D and see the value is valid and return in to
    // the core.
    L2.touch(addr);
    L2.set_accessed_bit_read(addr);

    #ifdef __DEBUG_ADDR_SHORT
    if (__DEBUG_ADDR_SHORT == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[CACHEMODEL READ] Setting Accessed bit addr: 0x%08x core: %4d "
                    "cluster: %d\n", addr, core, cluster_num);
    }
    #endif

//    L2.hw_prefetcher->cache_access(&instr, addr, core, tid);


    // pass to cacheaccess funcs
    CacheAccess_t ca(
      addr,         // address
      core,         // core number
      tid,          // thread id
      icmsg_type,   // interconnect message type
      &instr        // instruction correlated to this access
    );

    if (L1D_ENABLED) {
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L1D Pending Request.\n");
      }
      #endif
      #ifdef DEBUG_CACHES
        DEBUG_HEADER();
	cerr << " L1D Cache pend mshr\n";
      #endif
      L1D[L1CacheID].pend(ca, L2D_ACCESS_LATENCY,-1, false, false);
    } else {
      NoL1D[L1CacheID].pend(ca, L2D_ACCESS_LATENCY, -1, false);
    }

    // Pick the line up from the L1D in a later cycle.  Line is now in the
    // cluster cache.
    return helper_return_read_stall(stall, L2DAccess);
  } else {
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER(); fprintf(stderr, "[READ ACCESS DEBUG] L2D *MISS*.\n");
    }
    #endif
    // At this point, the line that we are looking for is not present in the
    // cluster cache nor is it present in the core-level L1D.  A request must be
    // made to the global cache via the TileInterconnect and then the GNet using
    // the L2.pend() method

    // We have an L1D MSHR, time to try the L2.  If we succeed with the
    // L2.pend() call, that means we have the MSHR and we have inserted a
    // request at the cluster cache controller for the miss.
    MissHandlingEntry<rigel::cache::LINESIZE> *mshr = L2.IsPending(addr);
    if (mshr == NULL)
    {
      // Check for free MSHR's.
      if (!L2.is_being_evicted(addr) && !L2.mshr_full(false /* Not an Eviction */))
      {
        // pass to cacheaccess funcs
        CacheAccess_t ca(
          addr,         // address
          core,         // core number
          tid,          // thread id
          icmsg_type,   // interconnect message type
          &instr        // instruction correlated to this access
        );
	#ifdef DEBUG_CACHES
	DEBUG_HEADER();
	cerr << "L2 Cache Miss pend mshr\n";
	#endif
        L2.pend(ca, false);
      } else {
        // No MSHR's, so the core will have to replay later.
        return helper_return_read_stall(stall, L2DMSHR);
      }
    }
    else //already pending, add this core to the waiting list and go to sleep
    {
      mshr->AddCore(core); // FIXME for MT ? what does this do?
      //FIXME If the MSHR is a bulk prefetch, it is just waiting to be injected
      //onto the network.  Once it is, the MSHR will be squashed.  Currently,
      //That's ok because the clear() call will wake this core up and it will
      //try again, seeing that IsPending() now returns NULL.  However, really
      //we don't want to add other cores to fire-and-forget MSHRs, they should
      //pend themselves.  This gets gnarly, since you'd then have 2 pending
      //MSHRs for the same address, but it may be necessary.  Either that, or
      //we can have some separate thing for fire-and-forget requests (separate
      //bank of MSHRs or another data structure entirely) which will not match
      //calls to IsPending().
    }

    return helper_return_read_stall(stall, L2DPending);
  }
}
