/////////////////////////////////////////////////////////////////////////////////
/// cache_model.cpp
/////////////////////////////////////////////////////////////////////////////////
/// 'cache_model.cpp'  Logic that drives the cluster-level caches including the
/// core caches (L1D, L1I) and the cluster cache.  Also includes random logic
/// for CacheTag, MSHR, and other cache utility functions.  Interactions with the
/// cache model that do not warrant their own file are also defined here.
/////////////////////////////////////////////////////////////////////////////////

//#define DEBUG_BCAST

#include <assert.h>                     // for assert
#include <stddef.h>                     // for NULL
#include <stdint.h>                     // for uint32_t
#include <list>                         // for list
#include "broadcast_manager.h"  // for BroadcastManager
#include "cache_model.h"    // for CacheModel, ldlstc_req_t
#include "caches/cache_base.h"  // for CacheAccess_t, etc
#include "caches/l1d.h"     // for L1DCache, NoL1DCache
#include "caches/l1i.h"     // for L1ICache
#include "caches/l2d.h"     // for L2Cache, Profile (ptr only), etc
#include "caches/l2i.h"     // for L2ICache
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for RES_TYPE, icmsg_type_t, etc
#include "icmsg.h"          // for ICMsg
#include "instr.h"          // for InstrLegacy
#include "profile/profile.h"        // for ProfileStat, CacheStat, etc
#include "profile/profile_names.h"  // for ::STATNAME_L2_WB_WASTED, etc
#include "core/regfile_legacy.h"        // for RegisterBase, etc
#include "sim.h"            // for ArbL2Type, stats, etc

/////////////////////////////////////////////////////////////////////////////////
///        class CacheModel       (Implementations)
/////////////////////////////////////////////////////////////////////////////////
/// Models caching at the cluster level (all intra-cluster caching)

/////////////////////////////////////////////////////////////////////////////////
/// Constructor: CacheModel()
/////////////////////////////////////////////////////////////////////////////////
/// Initialize the cache model for a cluster.  It needs to set up the arbiters,
/// the cluster- and core-level caches.  Each of the core-level caches also needs
/// to be made aware of its next level in the cache.
/////////////////////////////////////////////////////////////////////////////////
CacheModel::CacheModel(rigel::ConstructionPayload cp, Profile *profile, ClusterLegacy *cl) : 
  backing_store(*(cp.backing_store)),
  L2(this, cp.tile_network, profile, cp.component_index), 
  profiler(profile),
  broadcast_manager(cp.broadcast_manager), 
  cluster_num(cp.component_index),
  ArbL2(rigel::cache::L2D_BANKS,
        rigel::cache::L2D_PORTS_PER_BANK,
        rigel::cache::L2D_BANKS,
        rigel::cache::L2D_PORTS_PER_BANK,
        rigel::CORES_PER_CLUSTER,
        ARB_ROUND_ROBIN ),
  ArbL2I(rigel::cache::L2I_BANKS,
        rigel::cache::L2I_PORTS_PER_BANK,
        rigel::cache::L2I_BANKS,
        rigel::cache::L2I_PORTS_PER_BANK,
        rigel::CORES_PER_CLUSTER,
        ARB_ROUND_ROBIN ),
  ArbL2Return(rigel::cache::L2D_BANKS,
        rigel::cache::L2D_PORTS_PER_BANK,
        rigel::cache::L2D_BANKS,
        rigel::cache::L2D_PORTS_PER_BANK,
        rigel::CORES_PER_CLUSTER,
        ARB_ROUND_ROBIN ),
        cluster(cl)
{

  // first, handle anything that needs to be dynamically allocated
  // allocate L1D caches
  // use L1DS_PER_CLUSTER for the number of L1D caches
  L1D   = new L1DCache[rigel::L1DS_PER_CLUSTER];
  NoL1D = new NoL1DCache[rigel::L1DS_PER_CLUSTER];
  // use number of THREADS_PER_CLUSTER for number of watchdog counters, one per
  // thread for now
  memory_retry_watchdog_count = new int[rigel::THREADS_PER_CLUSTER];
  memory_retry_watchdog_count_instr = new int[rigel::THREADS_PER_CLUSTER];

  // Initialize the cache by making all cache lines invalid.  Blunt :-)
  invalidate_all();

  // Set all the next level pointers from the cores to point to the Cluster
  // Cache so that when requests are pending, the cores can check for completion
  // by calling Schedule() on the CCache
  // icaches per core
  for (int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    L1I[i].SetNextLevelI(&L2I);
    L1I[i].SetNextLevel(&L2);
  }
  // L1D caches (line buffers) per-core or per-cluster depending on value
  for (int i = 0; i < rigel::L1DS_PER_CLUSTER; i++) {
    L1D[i].SetNextLevel(&L2);
  }
  for (int i = 0; i < rigel::THREADS_PER_CLUSTER; i++) {
    memory_retry_watchdog_count[i] = 0;
    memory_retry_watchdog_count_instr[i] = 0;
  }

  // The L2I uses the L2 (the cluster cache) to handle requests into the
  // tile-level network.
  L2I.SetNextLevel(&L2);

  // Allocated the table of load-link (LDL) requests.  Only one request is
  // allowed per hardware context.
  LoadLinkTable = new ldlstc_req_t[rigel::THREADS_PER_CLUSTER];

  // Allocate the bitvector of completed arbitrations by-thread.
  thread_arb_complete = new bool[rigel::THREADS_PER_CLUSTER];
  for (int i = 0; i < rigel::THREADS_PER_CLUSTER; i++) { thread_arb_complete[i] = false; }

  rigel::GLOBAL_debug_sim.cluster_cache_models.push_back(this);
}

////////////////////////////////////////////////////////////////////////////////
/// PerCycle()
/// 
/// Clock all of the caches associated with a cluster.  Called each cycle.  Also
/// calls the core-level caches.  There is no required ordering of calls, but I
/// would not touch them.
////////////////////////////////////////////////////////////////////////////////
void
CacheModel::PerCycle()
{
  using namespace rigel;
  using namespace rigel::cache;

  // Check MSHR full status
  if (L2.mshr_full(false)) { profiler->cache_stats.CCache_mshr_full++; }

  // Clock the cluster cache.
  L2.PerCycle();

  // Clock the cluster instruction cache if separate from the L2D.
  if (USE_L2I_CACHE_SEPARATE) {
    L2I.PerCycle();
  }

  // handle core dcaches
  for (int i = 0; i < L1DS_PER_CLUSTER; i++) {
    // using 'L1' aka core-local cache
    if (L1D_ENABLED) {
      L1D[i].PerCycle();
    } else {
      NoL1D[i].PerCycle();
    }
  }

  // handle core icaches
  for (int i = 0; i < CORES_PER_CLUSTER; i++) {
    // Always use an L1I
    L1I[i].PerCycle();
  }

  // Clock the cluster-level arbiters.
  ArbL2.PerCycle();

  ArbL2Return.PerCycle();
  
  if(rigel::cache::USE_L2I_CACHE_SEPARATE)
  {
    //printf("Begin L2I Arb\n");
    ArbL2I.PerCycle();
    //printf("End L2I Arb\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// memory_barrier()
/// 
/// Insert a memory barrier.  Block all requests from entering the cluster
/// cache controller and wait for writeback count to drop to zero and
/// pendingMisses to go to zero before releasing.  The barrier itself is handled
/// in 'cluster_cache.cpp'.
/// 
/// RETURNS: true when stalled, false when complete
////////////////////////////////////////////////////////////////////////////////
bool
CacheModel::memory_barrier(InstrLegacy &instr, int local_core_id)
{
  // Check if a barrier is pending.  If so, stall the core.
  if (L2.GetMemoryBarrierPending(local_core_id)) {
    profiler->cache_stats.CCache_memory_barrier_stalls++;
    return true;
  }

  // Check if the memory barrier we are attempting has completed already
  if (L2.GetMemoryBarrierCompleted(local_core_id)) {
    // Clear the barrier
    L2.AckMemoryBarrierCompleted(local_core_id);
    // No stall, done with MB
    return false;
  }

  // Insert memory barrier and stall the core
  L2.SetMemoryBarrierPending(local_core_id);

  profiler->cache_stats.L2_cache_memory_barriers++;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// broadcast()
/// 
/// Issue a broadcast operation from a core.  This is the memory system half of
/// the BCAST.INV and BCAST.UPDATE instructions.  The broadcast registers itself
/// as a global operation and blocks until the GCache returns an ack.  It does
/// not wait until all other cores recieve the BCAST.NOTIFY, however.
/// 
/// RETURNS: true when stalled, false when complete
/// 
////////////////////////////////////////////////////////////////////////////////
bool
CacheModel::broadcast( const CacheAccess_t ca, uint32_t &data)
{
  // FIXME: Most of the logic is here for broadcast operations, but we still
  // need to finish dealing with propagating the messages from the GCache (see
  // interconnect.cpp for that FIXME).  We also may need to fix how the cluster
  // cache deals with the NOTIFY messages that the GCache/Routers generate.
  // (see cluster_cache.cpp for that FIXME).
  bool stall = true;

  // Disallow mulitple broadcasts.  Since the pipeline in front of the BCAST may
  // stall, broadcast() might get called twice with the BCAST already completed.
  // This makes sure that if the current instruction already issued a BCAST that
  // was accepted by the cache controller, it cannot reissue to the memory
  // system.
  if ( ca.get_instr()->IsCompletedBCAST_UPDATE()) { return false; }

  // For now we can just handle this like any other global memory operation.
  icmsg_type_t icmsg_type = ICMsg::instr_to_icmsg( ca.get_instr()->get_type() );

  CacheAccess_t ca_out = ca;     // same parameters as input
  ca_out.set_icmsg_type( icmsg_type ); // now, with icmsg_type defined

  // BCAST operation has completed (Overloads global op, but only one can be in
  // flight at a time anyway)
  // Make sure L2 has received reply AND broadcast manager says all clusters
  // have ACKed their NOTIFYs
  // core_id is LOCAL core_id / tid
  if (L2.GlobalMemOpCompleted[ ca.get_tid() ] == true &&
            !broadcast_manager->isBroadcastOutstandingToLine( ca.get_addr() ))
  {
    #ifdef DEBUG_BCAST
    DEBUG_HEADER();
    fprintf(stderr, "((BROADCAST)) GlobalMemOpCompleted[tid] == true "
                    "local core: %d tid: %d type: %d [[COMPLETE OP]]\n",
                    ca.get_core_id(), ca.get_tid(),
                    ca.get_instr()->get_type()
    );
    #endif

    // Lower the global operation fence for the core (now, per thread)
    L2.GlobalMemOpCompleted[   ca.get_tid() ] = false;
    L2.GlobalMemOpOutstanding[ ca.get_tid() ] = false;

    // FIXME: Do RMW here to remain atomic.  This should be done at GCache
    // and sent back with the message
    if ( ca.get_instr()->get_type() == I_BCAST_UPDATE) {
      // XXX: bcast.update - Everyone sees the value!
      #ifdef DEBUG_BCAST
      DEBUG_HEADER(); fprintf(stderr, "[[BCAST_UPDATE]] data: 0x%08x addr: 0x%08x\n",
        data, ca.get_addr());
      #endif
      //FIXME FIXME CHECK THIS
      //GlobalMemory.write_word_no_stall(addr, data);
    } else {
      if ( ca.get_instr()->get_type() == I_BCAST_INV) {
        // XXX: bcast.inv -  All of the invalidates go out and replies so nothing
        // to do here, technically
        assert(0 && "Need to add BCAST INV");
      } else {
        assert(0 && "Unknown broadcast operation!");
      }
    }

    // Done with broadcast inject.  The GCache has acked our request.
    ca.get_instr()->SetCompletedBCAST_UPDATE();
    stall = false;
    return stall;
  }

  // If a global operation is already outstanding, try again later.
  // If the line already has an oustanding broadcast, try again.
  if(!L2.GlobalMemOpOutstanding[ ca.get_tid() ] &&
            !broadcast_manager->isBroadcastOutstandingToLine( ca.get_addr() ) )
  {

    if ((L2.IsPending( ca.get_addr() ) != NULL) && !L2.IsPendingNoCCFill( ca.get_addr() ) )
    {
      stall = true;
      return stall;
    }

    // FIXME If we broadcast an invalidate, we should do it locally too.  It
    // will get invalidated again on the IC_MSG_BCAST_NOTIFY...may want to fix
    // that.
    bool inv_stall = false;

    // For coherence, we need to always make sure that the line is invalidated in
    // case a probe request comes in for the line.
    //    if (icmsg_type == IC_MSG_BCAST_INV_REQ && L2.IsValidLine(addr)) {
    if (L2.IsValidLine( ca.get_addr() )) {
      //invalidate_line(local_core_id, ca.get_addr(), inv_stall);
      invalidate_line(ca, inv_stall);
    }

    if(!L2.mshr_full(false))  // is_eviction == false
    {
      #ifdef DEBUG_BCAST
      DEBUG_HEADER();
      fprintf(stderr, "((BROADCAST)) GlobalMemOpOutstanding[ tid ] == "
                    "false local tid: %d [[PENDING]]\n", ca.get_tid() );
      #endif

      // Tell the broadcast manager
      broadcast_manager->insertBroadcast( ca.get_addr() ,data,cluster_num);

      L2.pend(ca_out, false);

      // Once pend()'ed at the L2, we poll on GlobalMemOpCompleted[tid]
      L2.GlobalMemOpOutstanding[ ca.get_tid() ] = true;
      assert((L2.GlobalMemOpCompleted[ ca.get_tid() ] == false) &&
        "Pending a broadcast operation.  It cannot already be completed!");
    } else {
      #ifdef DEBUG_BCAST
      DEBUG_HEADER();
      fprintf(stderr, "((BROADCAST)) GlobalMemOpOutstanding[tid] == false "
                      "core: %d tid:%d ((MSHR full)) [[PEND FAILED]]\n",
                      ca.get_core_id(), ca.get_tid());
      #endif

      Profile::accum_cache_stats.L2D_MSHR_stall_cycles++;
    }
  } else {
    #ifdef DEBUG_BCAST
    DEBUG_HEADER();
    fprintf(stderr, "((BROADCAST)) GlobalMemOpOutstanding[tid] == true "
                    "local_core_id: %d tid: %d [[WAITING]]\n", ca.get_core_id(), ca.get_tid());
    #endif
    Profile::accum_cache_stats.L2D_pending_misses++;
  }

  return (stall = true);
}
// end broadcast()
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
///   prefetch_line
////////////////////////////////////////////////////////////////////////////////
///
///  Checks for a CCache hit, if so it stops.  On a CCache miss, sends a message
///  to the network (pend()'s).  If pend fails, it returns false
///
////////////////////////////////////////////////////////////////////////////////
bool
CacheModel::prefetch_line(InstrLegacy &instr, int core, uint32_t addr, int tid)
{
  icmsg_type_t icmsg_type;
  instr_t instr_type = instr.get_type();

  // Inject a message into the network by calling L2 pend().  If there are no
  // MSHR's, we quit.  If L2.pend() fails for any reason, we quit.
  if ( L2.mshr_full(false) ) { return false; }

  switch (instr_type) {
    case I_PREF_NGA:  icmsg_type = IC_MSG_PREFETCH_NGA_REQ; break;
    case I_PREF_L:    icmsg_type = IC_MSG_PREFETCH_REQ;     break;
    case I_PREF_B_GC: icmsg_type = IC_MSG_PREFETCH_BLOCK_GC_REQ;   break;
    case I_PREF_B_CC: icmsg_type = IC_MSG_PREFETCH_BLOCK_CC_REQ;   break;
    default: assert(0 && "Unknown prefetch type");
  }

  // Return without prefetching if the address is already pending or already
  // valid in the L2.  Does not apply to block prefetches: If some lines in a
  // block prefetch are valid and some aren't, the C$ will filter out the valid
  // lines.  IsPending() is not meaningful for block prefetches because the MSHR
  // is dropped as soon as the request is injected into the network (fire and
  // forget)

  bool is_bulk_prefetch = (icmsg_type == IC_MSG_PREFETCH_BLOCK_GC_REQ ||
                           icmsg_type == IC_MSG_PREFETCH_BLOCK_CC_REQ);
  if(!is_bulk_prefetch && L2.IsPending(addr)) { return true; }
  if((!is_bulk_prefetch && L2.IsValidLine(addr)) || rigel::cache::PERFECT_L2D) {
    return true;
  }
  if(!is_bulk_prefetch && L2.is_being_evicted(addr)) { return true; }

  // FIXME: this should just be an input parameter for this function
  CacheAccess_t ca(
    addr,              // address
    core,              // core number (faked)
    tid,               // thread id
    icmsg_type,        // interconnect message type
    rigel::NullInstr // instruction correlated to this access
  );

  //Need to add more lines for bulk prefetches
  if(is_bulk_prefetch) {
    uint32_t numLines = instr.get_result_reg().data.i32;
    uint32_t startAddr = instr.get_result_ea();
    for(uint32_t i = 0; i < numLines; i++) {
      //Filter out lines being evicted to avoid bypassing the eviction.
      //FIXME Not sure how to implement this particular operation (masking off lines)
      //in hardware.
      uint32_t a = startAddr + (i*rigel::cache::LINESIZE);
      if(!L2.is_being_evicted(a)) {
        ca.add_addr(a);
      }
    }
  }

  // Send prefetch request off to the GCache.
  L2.pend(ca, false);

  return true;
}


/////////////////////////////////////////////////////////////////////////////////
/// writeback_all
/////////////////////////////////////////////////////////////////////////////////
void
CacheModel::writeback_all() {
  assert(0 && "Not yet implemented!");
}

/////////////////////////////////////////////////////////////////////////////////
/// invalidate_all
/////////////////////////////////////////////////////////////////////////////////
/// invalidates both the cluster (L2) cache and the core level L1D caches
/// instruction caches are not touched here
void
CacheModel::invalidate_all() {
  // Invalidate every data line at the cores.
  if( rigel::cache::L1D_ENABLED ) { // skip if no L1D
    for (int i = 0; i < rigel::L1DS_PER_CLUSTER; i++) {
      L1D[i].invalidate_all();
    }
  }
  // Invalidate L2 data cache.  May blow away instructions if UNIFIED L2 is set.
  L2.invalidate_all();
}

////////////////////////////////////////////////////////////////////////////////
/// invalidate_instr_all
////////////////////////////////////////////////////////////////////////////////
/// invalidate all instruction caches, both at the core and the cluster level
/// (if cluster-level icaching enabled)
void
CacheModel::invalidate_instr_all() {
  using namespace rigel::cache;

  // Invalidate every instruction line at the core.
  for (int i = 0; i < rigel::CORES_PER_CLUSTER; i++) { L1I[i].invalidate_all(); }
  // Invalidate L2.  If UNIFIED, data is blown away as well.
  if (USE_L2I_CACHE_UNIFIED) { 
    L2.invalidate_all(); 
  } else {
    L2I.invalidate_all(); 
  }
}

////////////////////////////////////////////////////////////////////////////////
/// writeback_line
////////////////////////////////////////////////////////////////////////////////
void
CacheModel::writeback_line( const CacheAccess_t ca_in, bool &stall )
{
  using namespace rigel;

  int L1CacheID = GetL1DID(ca_in.get_tid());

  stall = true;
  bool explicit_wb = false;
  icmsg_type_t icmsg_type = IC_MSG_NULL;

  switch ( ca_in.get_instr()->get_type() ) {
    // Comes from an eviction
    case (I_NULL):
      // TODO: I am not sure that it ever gets called in this manner.
      assert(0 && "REMOVE ME IF FOUND IN CODE!");
      explicit_wb = false;
      //icmsg_type = IC_MSG_EVICT_REQ;
      break;
    // Comes from explicit instruction forcing writeback.
    case (I_LINE_WB):
    case (I_LINE_FLUSH):
    case (I_CC_WB):
    case (I_CC_FLUSH):
      explicit_wb = true;
      icmsg_type = ICMsg::instr_to_icmsg( ca_in.get_instr()->get_type() );
      break;
    default:
      assert(0 && "Unknown instruction type in writeback!");
      break;
  }

  assert(icmsg_type != IC_MSG_NULL);

  if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
    // In the case the coherence is enabled, we cannot writeback lines that are
    // only held as clean since we do not have write permission to the lines.
    if( rigel::cache::L1D_ENABLED ) {
      if (L1D[ L1CacheID ].IsValidLine(ca_in.get_addr()) && !L1D[ L1CacheID ].IsDirtyLine(ca_in.get_addr())) {
        // Any non-stalling WB implies a wasted WB instruction.
        profiler::stats[STATNAME_L2_WB_WASTED].inc();
        stall = false;
        return;
      }
      // FIXME: This gives free BW into the L2 to do squashed writebacks.
      if ( L2.IsValidLine(ca_in.get_addr()) && !L2.IsDirtyLine(ca_in.get_addr())) {
        // Any non-stalling WB implies a wasted WB instruction.
        profiler::stats[STATNAME_L2_WB_WASTED].inc();
        stall = false;
        return;
      }
    } else {
      if (  L2.IsValidLine(ca_in.get_addr()) && !L2.IsDirtyLine(ca_in.get_addr())) {
        // Any non-stalling WB implies a wasted WB instruction.
        profiler::stats[STATNAME_L2_WB_WASTED].inc();
        stall = false;
        return;
      }
    }
  }

  // If writeback, we need up update memory/caches here
  // 1. Skip if there is no L1D.
  if( rigel::cache::L1D_ENABLED ) {
    // 2. Skip if the line is not in the L1D.
    if (L1D[ L1CacheID ].IsValidWord(ca_in.get_addr())) {
      L1D[ L1CacheID ].clearDirtyLine(ca_in.get_addr());
      // We have to make sure that if we clear the line, that the access bit remains
      // set to avoid a READ -> WRITE coherence deadlock.
      L1D[ L1CacheID ].invalidate_line(ca_in.get_addr());
    }
  }

  // If it is not cached, request is dropped.
  if (!L2.IsValidLine(ca_in.get_addr()))
  {
    #ifdef DEBUG_CACHES
    DEBUG_HEADER();
    fprintf(stderr, "I_LINE_WB: core (%d) tid (%d) addr: 0x%08x (NOT VALID, REQ DROPPED)\n",
      ca_in.get_core_id(), ca_in.get_tid(), ca_in.get_addr() );
    #endif

    // Any non-stalling WB implies a wasted WB instruction.
    profiler::stats[STATNAME_L2_WB_WASTED].inc();
    stall = false;
    return;
  }
  // If it is not dirty, there is nothing to writeback.  Note that this can
  // diverge from what one might want in silicon since there may be a reason to
  // overwrite the G$ with what we have cached even if it is not dirty.  Some
  // other core may have written to the G$ in the time between when we loaded
  // the value and when we issue the WB.  However, that is dicy at best and
  // represents a race between the other cluster doing the write, our read, and
  // when the eviction policy decides to drop this line on the floor.  If it is
  // dropped then there is nothing to writeback and hence the little maneuver
  // here fails.
  if ( !L2.IsDirtyLine( ca_in.get_addr() ) ) {
    #ifdef DEBUG_CACHES
    DEBUG_HEADER();
    fprintf(stderr, "I_LINE_WB: core (%d) addr: 0x%08x (NOT DIRTY, REQ DROPPED)\n",
      ca_in.get_core_id(), ca_in.get_addr() );
    #endif

    // Any non-stalling WB implies a wasted WB instruction.
    profiler::stats[STATNAME_L2_WB_WASTED].inc();
    stall = false;
    return;
  }

  // Do not allow to proceed if an existing memory operation is in effect.  If
  // we have not MSHR's, stall.
  if ((L2.IsPending( ca_in.get_addr() ) != NULL) || L2.mshr_full(false))
  {  // is_eviction == 0
    #ifdef DEBUG_CACHES
    DEBUG_HEADER();
    fprintf(stderr, "I_LINE_WB: core (%d) addr: 0x%08x (RETRY)\n",
      ca_in.get_core_id(), ca_in.get_addr() );
    #endif

    stall = true;
    return;
  }

  // The only time a writeback is useful is when it pends a message.  If the
  // writeback does not stall, a wasted writeback is implied.
  profiler::stats[STATNAME_L2_WB_USEFUL].inc();

  #ifdef DEBUG_CACHES
  DEBUG_HEADER();
  fprintf(stderr, "I_LINE_WB: core (%d) addr: 0x%08x (SUCCESS: PENDING)\n",
    ca_in.get_core_id(), ca_in.get_addr() );
  #endif

  CacheAccess_t ca_out = ca_in;
  ca_out.set_icmsg_type( icmsg_type ); // set new icmsg_type
  // CacheAccess_t ca(
  //   addr,              // address
  //   core,              // core number (faked)
  //   FIXME_TID,         // thread id
  //   icmsg_type,        // interconnect message type
  //   rigel::NullInstr // instruction correlated to this access
  // );

  // If we are doing writeback counting and this is an explicit writeback, the
  // pend will allocate an MSHR just until it is taken by the network.
  L2.pend(ca_out, false);

  // This clearing is a bit premature.  We may want to wait until the reply
  // returns to ensure that the dirty bits are consistent with the G$.
  L2.clearDirtyLine(ca_in.get_addr());
  // We have to make sure that if we clear the line, that the access bit remains
  // set to avoid a READ -> WRITE coherence deadlock.
  L2.set_accessed_bit_read(ca_in.get_addr());
  L2.set_accessed_bit_write(ca_in.get_addr());
  stall = false;
  // With coherence, we need to invalidate the line to ensure correctness since
  // once the writeback leaves the cluster cache, we have relinquished ownership
  // to the line.
  if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY)  {
    // put WrtTrack call to invalidation handling here???
    L2.invalidate_line( ca_in.get_addr() );
  }

  return;
}
// end writeback_line()
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// invalidate_line
////////////////////////////////////////////////////////////////////////////////
///  Make the line no longer valid in the cluster cache.  Independent of what the
///  status of the line is in the caches currently.
////////////////////////////////////////////////////////////////////////////////
void
CacheModel::invalidate_line( const CacheAccess_t ca_in, bool &stall) {

  using namespace rigel;

  int L1CacheID = GetL1DID(ca_in.get_tid());
  // If there is a temporary access pending, that will clean up this request
  // in the next cycle.
  if (!L1D[ L1CacheID ].get_temporary_access_pending()) {
    L1D[ L1CacheID ].invalidate_line( ca_in.get_addr() );
  }

  if (L2.IsValidWord( ca_in.get_addr() )) {
    if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {

      if (L2.mshr_full(false) || (L2.IsPending( ca_in.get_addr() ) != NULL)) {
        stall = true;
        return;
      }

      // Check data back in with the global cache directory before proceding
      // with global operation.
      icmsg_type_t msg_type =
        (L2.IsDirtyLine( ca_in.get_addr() )) ? IC_MSG_EVICT_REQ : IC_MSG_CC_RD_RELEASE_REQ;

      CacheAccess_t ca_out = ca_in;              // same params as input, with additions below
      ca_out.set_icmsg_type( msg_type );      // interconnect message type
      ca_out.set_instr( rigel::NullInstr ); // instruction correlated to this access
      L2.pend(ca_out, true /* is eviction! */);
    }
    // Invalidations are useful when the line is valid in the L2.  Skip stall
    // condition for HWcc.
    profiler::stats[STATNAME_L2_INV_USEFUL].inc();
    // Without coherence, we can simply drop the line.  With coherence, we
    // still need to invalidate.
    L2.invalidate_line( ca_in.get_addr() );
    // put WrtTrack call to invalidation handling here???
    return;
  }
  // Line was not valid in a cache.
  profiler::stats[STATNAME_L2_INV_WASTED].inc();
}

////////////////////////////////////////////////////////////////////////////////
/// EvictDown
////////////////////////////////////////////////////////////////////////////////
/// Evict lines from the core-level caches when the cluster cache must evict the
/// line.  The model assumes a write-through cluster cache.  This is imprecise,
/// but will work in simulation since we keep a unified memory model.
////////////////////////////////////////////////////////////////////////////////
void
CacheModel::EvictDown(uint32_t addr) {

  #ifdef DEBUG_CACHES
  DEBUG_HEADER();
  fprintf(stderr, "EVICT DOWN: addr 0x:%08x\n", addr);
  #endif

  // If there is a temporary access pending, that will clean up this request
  // in the next cycle.  The goal is to avoid evicting the line from the L1D
  // before the core has a chance to access it.
  // check each L1D the core has (all L1D caches in the whole cluster)
  for(int d = 0; d < rigel::L1DS_PER_CLUSTER; d++) {
    if (!L1D[d].get_temporary_access_pending()) {
      L1D[d].invalidate_line(addr);
    }
  }

  // assumed only one L1I per core
  // FIXME: performance bug doing this if we don't need to? forcing inclusion?
  for (int core = 0; core < rigel::CORES_PER_CLUSTER; core++) {
    // NOTE: this is not(!) required if code is read-only and there is no HW
    // coherence, and MAY or MAY NOT be required for the coherent case (again,
    // assuming code is read-only)
    //if(rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
      L1I[core].invalidate_line(addr);
    //}
  }

}

unsigned int
CacheModel::GetL1DID(int tid) {
  assert(tid >= 0 && "tids must be non-negative to have an L1D$ ID!");
  unsigned int retval = (tid << rigel_log2(rigel::L1DS_PER_CORE))
                        >> (rigel_log2(rigel::THREADS_PER_CORE));
  return retval;
}

////////////////////////////////////////////////////////////////////////////////
/// ArbEarly
////////////////////////////////////////////////////////////////////////////////
/// Called (currently) from Exec to arb early when we will miss in L1D or if we do
/// not have an L1D
////////////////////////////////////////////////////////////////////////////////
void
CacheModel::ArbEarly(uint32_t addr, uint32_t core, int tid, RES_TYPE res_type ) {

  int L1CacheID = GetL1DID(tid);

  //use addr to arb a cycle early
  // FIXME 'cheat' and pre-check L1D for a hit
  if ( rigel::cache::L1D_ENABLED && // skip this if no L1D
      ( L1D[L1CacheID].IsValidWord(addr) || rigel::cache::PERFECT_L1D) ) {
    // do nothing on a hit to L1, no need for arb
  } else { // if we miss or have no L1, arb for a port
    // arb a cycle early to avoid stalls in the nonconflict case
    int res_id = ArbL2.GetResourceID(addr,res_type); // lookup what resource to request
    ArbL2.MakeRequest(res_id,core,res_type); // Arbitrate for a Read Port on L2
  }
}

////////////////////////////////////////////////////////////////////////////////
/// load_link_request()
/// 
/// Insert a load-linked (LDL) into the list of pending load links.  If an STC to
/// the same address comes in before the core that issues this request issues an
/// STC, the LDL request will be invalidated and a future STC will return false.
/// If an STC is never issued by the core issuing the LDL, it does not hurt
/// anything.  If a second LDL is issued by the core before an STC, the second
/// LDL overwrites the first without regard for the status of the pending LDL.
/// 
///  expects tid to be the cluster-level thread ID
///
/// RETURNS: --
/// 
////////////////////////////////////////////////////////////////////////////////
void
CacheModel::load_link_request(InstrLegacy &instr, int tid, uint32_t addr, int size) {
  // Only allowing cacheline and word-sized requests for now.
  assert(size == 4 || size == 32);
  LoadLinkTable[tid].valid = true;
  LoadLinkTable[tid].size = size;
  LoadLinkTable[tid].addr = addr;
}

////////////////////////////////////////////////////////////////////////////////
/// store_conditional_check()
/// 
/// Check for a pending STC.  If one is found and it is still valid, return
/// success and invalidate all other requests.
/// 
/// RETURNS: success is set to true if a request is found and it is still valid,
/// i.e., no other core has issued a STC to the line since the LL was issued by
/// this core.
/// 
/// 
////////////////////////////////////////////////////////////////////////////////
void
CacheModel::store_conditional_check(InstrLegacy &instr, int tid, uint32_t addr, bool &success) {
  // Allow for sub-line sized LDL/STC.  Right now it is only cache line sized.
  uint32_t size_mask = ~(LoadLinkTable[tid].size - 1);
  // By default, we fail.
  success = false;
  // Fail if we do not have a previous load link that is still valid.
  if (false == LoadLinkTable[tid].valid) return;
  // Fail if we do not match on the address.
  if ((addr & size_mask) != (LoadLinkTable[tid].addr & size_mask)) return;
  // Success.  Invalidate other LL requests.
  ldlstc_probe(tid, addr);

  success = true;
  return;
}

////////////////////////////////////////////////////////////////////////////////
/// ldlstc_probe()
/// 
/// Invalidate any pending LDL requests at the cluster that are pending to the
/// same line.  Only called if the store conditional succeeds.
/// 
/// thread based
/// expects tid to be the cluster-level thread ID
/// 
/// RETURNS: --
/// 
////////////////////////////////////////////////////////////////////////////////
void
CacheModel::ldlstc_probe(int tid, uint32_t addr) {
  // Cycle through each thread checking only the threads that have pending
  // reservations for ldl-stc
  for (int i = 0; i < rigel::THREADS_PER_CLUSTER; i++) {
    // Skip the requesting thread
    if (tid == i) continue;

    if (LoadLinkTable[i].valid) {
      uint32_t size_mask = ~(LoadLinkTable[i].size - 1);
      if ((addr & size_mask) == (LoadLinkTable[i].addr & size_mask)) {
        LoadLinkTable[i].valid = false;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// helper_arbitrate_L2D_access()
/// 
/// Handle arbitration for the thread.
/// 
/// RETURNS: true if arbitration was success, false otherwise
/// 
/// SIDE EFFECTS: sets thread_arb_complete[tid] on success.
/// 
////////////////////////////////////////////////////////////////////////////////
bool
CacheModel::helper_arbitrate_L2D_access(uint32_t addr, int core, int tid, RES_TYPE type)
{
  // Already won arbitration, waiting on result.
  if (true == thread_arb_complete[tid]) { return true; }

  if (rigel::CMDLINE_MODEL_CONTENTION)
  {
    // lookup what resource to request
    int res_id = ArbL2.GetResourceID(addr, type);
    // Check if we have acquired the port.
    if (ArbL2.GetArbStatus(res_id, core, type))
    {
      // Only allow it to get used once.
      //printf("Core %d Claiming L2 resource\n", core);
      ArbL2.ClaimResource(res_id, core, type);
      // Won arbitration.
      thread_arb_complete[tid] = true;
      // With the resource claimed, we can hit the L2.
      return true;
    } else {
      // Arbitrate for a write port on L2.
      ArbL2.MakeRequest(res_id,core,type);
      return false;
    }
  } else {
    // Won arbitration.
    thread_arb_complete[tid] = true;
    return true;
  }
}



bool
CacheModel::helper_arbitrate_L2D_instr(uint32_t addr, int core, int tid, RES_TYPE type)
{
  // Already won arbitration, waiting on result.
  //if (true == thread_arb_complete[tid]) { return true; }

  if (rigel::CMDLINE_MODEL_CONTENTION)
  {
    // lookup what resource to request
    int res_id = ArbL2.GetResourceID(addr, type);
    // Check if we have acquired the port.
    if (ArbL2.GetArbStatus(res_id, core, type))
    {
      // Only allow it to get used once.
      //printf("Core %d Claiming L2 resource\n", core);
      ArbL2.ClaimResource(res_id, core, type);
      // Won arbitration.
      //thread_arb_complete[tid] = true;
      // With the resource claimed, we can hit the L2.
      return true;
    } else {
      // Arbitrate for a write port on L2.
      ArbL2.MakeRequest(res_id,core,type);
      return false;
    }
  } else {
    // Won arbitration.
    //thread_arb_complete[tid] = true;
    return true;
  }
}


// ???
// FIXME: Seriously, what does this do?
bool
CacheModel::helper_arbitrate_L2D_return(uint32_t addr, int core, int tid, RES_TYPE type)
{
  if (rigel::CMDLINE_MODEL_CONTENTION)
  {
    // lookup what resource to request
    int res_id = ArbL2Return.GetResourceID(addr, type);
    // Check if we have acquired the port.
    if (ArbL2Return.GetArbStatus(res_id, core, type))
    {
      // Only allow it to get used once.
      ArbL2Return.ClaimResource(res_id, core, type);
      // With the resource claimed, we can hit the L2.
      return true;
    } else {
      // Arbitrate for a write port on L2.
      ArbL2Return.MakeRequest(res_id,core,type);
      return false;
    }
  }
  else {
    return true;
  }
}


