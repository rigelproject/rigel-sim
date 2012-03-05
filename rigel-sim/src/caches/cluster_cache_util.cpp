////////////////////////////////////////////////////////////////////////////////
// src/caches/cluster_cache_util.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the implementation of the L2D utility methods.  Mostly
//  non-critical functions < 100 lines in length.
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu64
#include <stddef.h>                     // for NULL, size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for fprintf, stderr, printf
#include <iostream>                     // for operator<<, cerr, endl, etc
#include <set>                          // for set, etc
#include <string>                       // for char_traits
#include "cache_tags.h"     // for CacheTag
#include "caches/cache_base.h"  // for CacheAccess_t, etc
#include "caches/l2d.h"     // for L2Cache, etc
#include "caches/l2i.h"     // for L2ICache
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for DEBUG_HEADER, etc
#include "locality_tracker.h"  // for LocalityTracker
#include "mshr.h"           // for MissHandlingEntry, etc
#include "sim.h"            // for CURR_CYCLE, etc

using namespace rigel;

#if 0
#define __DEBUG_ADDR 0x0491cac0
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#define __DEBUG_CYCLE 0x7FFFFFFF
#endif

void
L2Cache::nonatomic_ccache_op_unlock(const int thread_num, const uint32_t addr)
{
  //FIXME Turned this off for incoherent runs because it was making pseudo-mt run slow.
  //let's figure out if we can keep it and make it work.
  if(!rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
    return;
  }
  using namespace rigel;

  assert(thread_num < THREADS_PER_CLUSTER);
  assert(thread_num >= 0);

  const uint32_t masked_addr = addr & LINE_MASK;

  // Check if we already have lock.
  if (nonatomic_tbl[thread_num].valid) {
    // We should not have a valid entry outstanding to a different address.
    assert(nonatomic_tbl[thread_num].addr == masked_addr && "Releasing unlocked line?!");
    nonatomic_tbl[thread_num].reset();
    // Decrement all of the other counters to move others to the front of the
    // queue.  If any are valid, one should become zero.
    for (int i = 0; i < THREADS_PER_CLUSTER; i++) {
      if (nonatomic_tbl[i].valid && masked_addr == nonatomic_tbl[i].addr) {
        if (nonatomic_tbl[i].queue_num > 0) { nonatomic_tbl[i].queue_num--; }
      }
    }
    return;
  }
  // Note: For read accesses, it is possible to return successfully without
  // releasing the line.
}

bool
L2Cache::nonatomic_ccache_op_lock(const int thread_num, const uint32_t addr)
{
  //FIXME Turned this off for incoherent runs because it was making pseudo-mt run slow.
  //let's figure out if we can keep it and make it work.
  if(!rigel::ENABLE_EXPERIMENTAL_DIRECTORY)
  {
    return true;
  }
  using namespace rigel;

  assert(thread_num < THREADS_PER_CLUSTER);
  assert(thread_num >= 0);

  const uint32_t masked_addr = addr & LINE_MASK;

  // Check if we already have lock.
  if (nonatomic_tbl[thread_num].valid) {
    // We should not have a valid entry outstanding to a different address.
    assert(nonatomic_tbl[thread_num].addr == masked_addr);
    // Only allow access when we are at head of queue.
    if (0 == nonatomic_tbl[thread_num].queue_num) return true;
    else                                          return false;
  }

  // Try to obtain lock if no other core holds it.
  int max_queue_num = -1;
  for (int i = 0; i < THREADS_PER_CLUSTER; i++) {
    if (nonatomic_tbl[i].valid) {
      if (masked_addr == nonatomic_tbl[i].addr) {
        if (nonatomic_tbl[i].queue_num > max_queue_num) {
          max_queue_num = nonatomic_tbl[i].queue_num;
        }
      }
    }
  }
  if (-1 == max_queue_num) {
    // We have the lock.  SUCCESS!
    nonatomic_tbl[thread_num].valid = true;
    nonatomic_tbl[thread_num].addr = masked_addr;
    nonatomic_tbl[thread_num].queue_num = 0;
    return true;
  } else {
    // We have to insert ourselves at the end of the queue.
    nonatomic_tbl[thread_num].valid = true;
    nonatomic_tbl[thread_num].addr = masked_addr;
    nonatomic_tbl[thread_num].queue_num = max_queue_num + 1;
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// CCacheProfilerTouch()
/// ICacheProfilerTouch()
/// 
/// Add thread to saturating counter for cache tag.  There are two for L2Cache
/// in the case that a unified cluster cache is used.  There are two versions
/// because they function slightly differently and replication is syntactically
/// easier to swallow than doing something hacky with template classes here.
/// 
////////////////////////////////////////////////////////////////////////////////
void
L2Cache::CCacheProfilerTouch(uint32_t addr, int tid, bool is_read_request)
{
  int set = get_set(addr);
  int way = get_way_line(addr, set);
  if (-1 == way) return;

  // For now, do not track writes.
  if (is_read_request) {
    // Update the tag. Only if valid.
    if (TagArray[set][way].valid()) {
      TagArray[set][way].profile_ccache_core_access(tid);
    }
  }
}

void
L2Cache::ICacheProfilerTouch(uint32_t addr, int tid)
{
  int set = get_set(addr);
  int way = get_way_line(addr, set);
  if (-1 == way) return;

  // Update the tag. Only if valid.
  if (TagArray[set][way].valid()) {
    TagArray[set][way].profile_icache_core_access(tid);
  }
}

void
L2ICache::ICacheProfilerTouch(uint32_t addr, int tid)
{
  int set = get_set(addr);
  int way = get_way_line(addr, set);
  if (-1 == way) return;

  // Update the tag. Only if valid.
  if (TagArray[set][way].valid()) {
    TagArray[set][way].profile_icache_core_access(tid);
  }
}
bool
L2Cache::network_wdt_check(MissHandlingEntry<LINESIZE> mshr)
{
  if (network_stall_WDT++ >  rigel::cache::CLUSTER_REMOVE_WDT) {
    DEBUG_HEADER();
    fprintf(stderr, "CLUSTER: %d\n", cluster_num);
		std::cerr << "Found message pending at ClusterRemovePeek() for " << std::dec
      << rigel::cache::CLUSTER_REMOVE_WDT << " cycles!\n";
    int i, count = 0;
    while (-1 != (i = pendingMissFifoGetEntry(count))) {
      fprintf(stderr, "%d -> real: %d pend: %d\n", count, i, ConvPendingToReal(i));
      count++;
    }
    DEBUG_HEADER();
    fprintf(stderr, "STUCK MSG: \n");
    mshr.dump();
    if (IsValidLine(mshr.get_addr())) {
      fprintf(stderr, "Matching address!  CacheTag: \n");
      size_t set = get_set(mshr.get_addr());
      size_t way = get_way_line(mshr.get_addr());
      TagArray[set][way].dump();
    }
    rigel::GLOBAL_debug_sim.dump_all();
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// L2Cache::dump_pending_msgs()
////////////////////////////////////////////////////////////////////////////////
// Dump the state of all of the pending misses in the cluster cache.
void
L2Cache::dump_pending_msgs() const
{
  for (int i = 0; i < MSHR_COUNT; i++) {
    if (pendingMiss[i].check_valid() ) { pendingMiss[i].dump(); }
  }
}

////////////////////////////////////////////////////////////////////////////////
// dump_cache_state()
////////////////////////////////////////////////////////////////////////////////
// prints cache state for each way,set
void
L2Cache::dump_cache_state() const
{
  for (int set = 0; set < rigel::cache::L2D_SETS; set++) {
    for (int way = 0; way < rigel::cache::L2D_WAYS; way++) {
      fprintf(stderr, "[%4d][%4d] (%1d)", set, way, (TagArray[set][way].valid()));
      TagArray[set][way].dump();
      fprintf(stderr, " (cl: %4d)\n", cluster_num);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// dump_locality()
////////////////////////////////////////////////////////////////////////////////
void
L2Cache::dump_locality() const
{
  assert(rigel::ENABLE_LOCALITY_TRACKING &&
         "Locality tracking disabled, C$::dump_locality() called");
  uint64_t sumOfPerCoreConflicts = 0UL;
  for(int i = 0; i < (rigel::CORES_PER_CLUSTER+1); i++)
  {
    perCoreLocalityTracker[i]->dump();
    sumOfPerCoreConflicts += perCoreLocalityTracker[i]->getNumConflicts();
  }
  aggregateLocalityTracker->dump();
  uint64_t aggregateConflicts = aggregateLocalityTracker->getNumConflicts();
  printf("Cluster %d: %"PRIu64" aggregate conflicts, %"PRIu64" per-stream conflicts\n",
          cluster_num, aggregateConflicts, sumOfPerCoreConflicts);
}

////////////////////////////////////////////////////////////////////////////////
// dump_tlb()
////////////////////////////////////////////////////////////////////////////////
void
L2Cache::dump_tlb() const
{
  assert(rigel::ENABLE_TLB_TRACKING &&
         "TLB tracking disabled, C$::dump_tlb() called");
  for(size_t i = 0; i < num_tlbs; i++)
    tlbs[i]->dump(stderr);
}

////////////////////////////////////////////////////////////////////////////////
// PM_set_owning_cluster()
////////////////////////////////////////////////////////////////////////////////
void L2Cache::PM_set_owning_cluster(ClusterLegacy *owner)
{
  for(int i = 0; i < rigel::cache::MAX_L2D_OUTSTANDING_MISSES; i++)
  {
    pendingMiss[i].owning_cluster = owner;
  }
}


// Method Name: ccache_wdt_expired() (HELPER)
//
// Called when the WDT expires for the cluster cache as determined by CCACHE_WDT
void
L2Cache::ccache_wdt_expired()
{
  for (int i = 0; i < MSHR_COUNT; i++) {
    if (pendingMiss[i].check_valid()) {
      if ((rigel::CURR_CYCLE - pendingMiss[i].watchdog_last_set_time )
          > (uint32_t)rigel::cache::CCACHE_WDT_TIMEOUT)
      {
        rigel::GLOBAL_debug_sim.dump_all();
        DEBUG_HEADER();
        fprintf(stderr, "[[ERROR]] Found CCache pending mshr for more than %d cycles! "
                        " cluster number: %d\n",
                        rigel::cache::CCACHE_WDT_TIMEOUT, cluster_num);
        pendingMiss[i].dump(); std::cerr << "\n";
				std::cerr << "Pending FIFO:\n";
        int count = 0;
        int i;
        while (-1 != (i = pendingMissFifoGetEntry(count))) {
          fprintf(stderr, "%d -> real: %d pend: %d ", count, i, ConvPendingToReal(i));
          fprintf(stderr, " wait time: %d\n",
           (int)( rigel::CURR_CYCLE - pendingMiss[i].watchdog_last_set_time ));
          count++;
         pendingMiss[i].dump(); std::cerr << "\n";
        }
        dumpPendingMissFIFO(stderr);
        assert(0);
      }
    }
  }
}

// Method Name: helper_check_pending_miss_invariants 
//
// This checks invariants in message types while doing profiling.  It is
// heavyweight, so we do not run it at all times, but it can be useful
// diagnosing coherence bugs. 
void
L2Cache::helper_check_pending_miss_invariants(const int idx)
{
  for (int j = idx+1; j < MSHR_COUNT; j++)
  {
    if (pendingMiss[j].check_valid()) {
      if (pendingMiss[idx].get_addr() == pendingMiss[j].get_addr()) {
        // Now we need to check if the two messages are allowed to be present in
        // the pending miss queue simultaneously. These should all be semetric.
        switch (pendingMiss[idx].GetICMsgType())
        {
          case IC_MSG_READ_REQ:
            switch (pendingMiss[j].GetICMsgType()) {
              case (IC_MSG_CC_INVALIDATE_NAK):
                continue;
              default: goto abort_sim;
            }
          case IC_MSG_LINE_WRITEBACK_REQ:
            switch (pendingMiss[j].GetICMsgType()) {
              case (IC_MSG_CC_WR_RELEASE_NAK):
                continue;
              default: goto abort_sim;
            }
          case IC_MSG_CCACHE_HWPREFETCH_REQ:
            switch (pendingMiss[j].GetICMsgType()) {
              case (IC_MSG_CC_INVALIDATE_NAK):
                continue;
              default: goto abort_sim;
            }
          case IC_MSG_CC_INVALIDATE_NAK:
            switch (pendingMiss[j].GetICMsgType()) {
              case (IC_MSG_CC_RD_RELEASE_REQ):
              case (IC_MSG_CCACHE_HWPREFETCH_REQ):
              case (IC_MSG_READ_REQ):
                continue;
              default: goto abort_sim;
            }
          case IC_MSG_CC_RD_RELEASE_REQ:
            switch (pendingMiss[j].GetICMsgType()) {
              case (IC_MSG_CC_INVALIDATE_NAK):
                continue;
              default: goto abort_sim;
            }
          case IC_MSG_EVICT_REQ:
            switch (pendingMiss[j].GetICMsgType()) {
              case (IC_MSG_CC_WR_RELEASE_NAK):
                continue;
              default: goto abort_sim;
            }
          case IC_MSG_CC_WR_RELEASE_NAK:
            switch (pendingMiss[j].GetICMsgType()) {
              case (IC_MSG_EVICT_REQ):
              case (IC_MSG_WRITE_REQ):
              case (IC_MSG_LINE_WRITEBACK_REQ):
                continue;
              default: goto abort_sim;
            }
          case IC_MSG_WRITE_REQ:
            switch (pendingMiss[j].GetICMsgType()) {
              case IC_MSG_CC_WR_RELEASE_NAK:
                continue;
              default: goto abort_sim;
            }
          default: goto abort_sim;
        }
  abort_sim:
        //We have two outstanding requests to the same address.
        rigel::GLOBAL_debug_sim.dump_all();
        DEBUG_HEADER();
        fprintf(stderr, "[BUG] Found two requests from same cluster outstanding to "
                        "same address! cluster: %04d addr: %08x\n", cluster_num,
                          pendingMiss[idx].get_addr());
        assert(0);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//  L2Cache::Schedule()
////////////////////////////////////////////////////////////////////////////////
void
L2Cache::Schedule(int size, MissHandlingEntry<rigel::cache::LINESIZE> *MSHR)
{
  if (MSHR->GetICMsgType() == IC_MSG_NULL) {
    DEBUG_HEADER();
    fprintf(stderr, "Found NULL IC message in L2Cache::Schedule! addr: 0x%08x\n",
            MSHR->get_addr());
            MSHR->dump();
            assert(0);
  }

  bool mshrs_full = mshr_full(false); //not an eviction
  bool someLinesInvalid = false;
  bool foundLineToReturn = false; //Only want to return one line from this request
                                  //per cycle, even if multiple happen to be valid.
  std::set<uint32_t> *addressesToPend = NULL;
  //  Only schedule once we have the value valid in the CCache
  for(std::set<uint32_t>::const_iterator it = MSHR->get_pending_addrs_begin(),
                                         end = MSHR->get_pending_addrs_end();
      it != end; ++it)
  {
    const uint32_t &addr = (*it);
    if(IsValidLine(addr))
    {
      if(!foundLineToReturn)
      {
        //FIXME only one line returned per cycle?
        if(!MSHR->all_lines_ready()) //FIXME implement a mshr.fill_pending() function instead
        {
          MSHR->set_line_ready(addr, rigel::CURR_CYCLE + rigel::cache::L2D_ACCESS_LATENCY);
        }
        foundLineToReturn = true; //Prevents us from hitting this path again
      }
    }
    else
    {
      someLinesInvalid = true;
      if ((IsPending(addr) == NULL) && !mshrs_full && !is_being_evicted(addr)) {  //Build up a set of addresses to pend
        if(addressesToPend == NULL) addressesToPend = new std::set<uint32_t>();
        addressesToPend->insert(addr);
      }
    }
  }
  if(someLinesInvalid && addressesToPend != NULL && !mshrs_full)
  {
    CacheAccess_t ca(
                    *addressesToPend,        // addresses
                    MSHR->GetCoreID(),       // core number
                    MSHR->GetThreadID(),     // thread id
                    MSHR->GetICMsgType(),    // interconnect message type
                    rigel::NullInstr       // no instruction correlated to this access
                    );
    pend(ca, false);
    //FIXME see note in L2Cache::pend(), we can fire and forget in this case.
    //We shouldn't clear() the MSHR itself; the caller wants to do that when
    //it sees that the MSHR IsDone().
    if (!ENABLE_EXPERIMENTAL_DIRECTORY && MSHR->GetICMsgType() == IC_MSG_WRITE_REQ)
    {
      MSHR->addrs.clear();
      MSHR->ready_lines.clear();
      MSHR->dump();
    }
  }
  if(addressesToPend != NULL && mshrs_full) //Don't ack it, they need to retry
  { }
  else
  {
    MSHR->SetRequestAcked();
  }
}

////////////////////////////////////////////////////////////////////////////////
//  L2ICache::Schedule()
////////////////////////////////////////////////////////////////////////////////
void L2ICache::Schedule(int size, MissHandlingEntry<rigel::cache::LINESIZE> *MSHR)
{
  if (MSHR->GetICMsgType() == IC_MSG_NULL) {
    DEBUG_HEADER();
    fprintf(stderr, "Found NULL IC message in L2Cache::Schedule! addr: 0x%08x\n",
            MSHR->get_addr());
            MSHR->dump();
            assert(0);
  }
  
  bool mshrs_full = mshr_full(false); //not an eviction
  bool someLinesInvalid = false;
  bool foundLineToReturn = false; //Only want to return one line from this request
  //per cycle, even if multiple happen to be valid.
  std::set<uint32_t> *addressesToPend = NULL;
  //  Only schedule once we have the value valid in the CCache
  for(std::set<uint32_t>::const_iterator it = MSHR->get_pending_addrs_begin(),
    end = MSHR->get_pending_addrs_end();
  it != end; ++it)
  {
    const uint32_t &addr = (*it);
    if(IsValidLine(addr))
    {
      if(!foundLineToReturn)
      {
        //FIXME only one line returned per cycle?
        if(!MSHR->all_lines_ready()) //HACK HACK HACK should have a mshr.fill_pending() function
          MSHR->set_line_ready(addr, rigel::CURR_CYCLE + rigel::cache::L2D_ACCESS_LATENCY);
        foundLineToReturn = true; //Prevents us from hitting this path again
      }
    }
    else
    {
      someLinesInvalid = true;
      if ((IsPending(addr) == NULL) && !mshrs_full && !is_being_evicted(addr)) {  //Build up a set of addresses to pend
        if(addressesToPend == NULL) addressesToPend = new std::set<uint32_t>();
        addressesToPend->insert(addr);
      }
    }
  }
  if(someLinesInvalid && addressesToPend != NULL && !mshrs_full)
  {
    CacheAccess_t ca(
    *addressesToPend,                    // addresses
                     MSHR->GetCoreID(),       // core number
                     MSHR->GetThreadID(),     // thread id
                     MSHR->GetICMsgType(),    // interconnect message type
                     rigel::NullInstr       // no instruction correlated to this access
                     );
    pend(ca, rigel::cache::L2I_ACCESS_LATENCY, MSHR->GetPendingIndex(), false, true);      
  }
  if(someLinesInvalid && addressesToPend != NULL && mshrs_full) { } //Don't ack it, they need to retry 
  else
  {
    MSHR->SetRequestAcked();
  }
}
