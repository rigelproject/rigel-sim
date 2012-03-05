////////////////////////////////////////////////////////////////////////////////
// core_caches.cpp
////////////////////////////////////////////////////////////////////////////////
///
///  This file contains the implementation of the cache models built into the
///  core. 
///
/// Cleanup pass and fixed variable latency cache issues.  These
/// caches must have their scheduled bit set on a pend() so that they do not
/// rerequest the line.
///
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, printf, stderr
#include <iomanip>                      // for operator<<, setfill, setw
#include <ostream>                      // for operator<<, etc
#include "cache_model.h"    // for CacheModel
#include "cache_tags.h"     // for CacheTag
#include "caches/l1d.h"     // for L1DCache
#include "caches/l1i.h"     // for L1ICache
#include "caches/l2d.h"     // for L2Cache
#include "caches/l2i.h"     // for L2ICache
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy, etc
#include "define.h"         // for ::IC_INVALID_DO_NOT_USE, etc
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrCycleStats, InstrStats
#include "mshr.h"           // for MissHandlingEntry
#include "profile/profile.h"        // for CacheStat, Profile, etc
#include "sim.h"            // for LINESIZE, etc

////////////////////////////////////////////////////////////////////////////////
// L1ICache::PerCycle()
////////////////////////////////////////////////////////////////////////////////
/// step the L1 Icache for a cycle
///
/// Contains two independent operations - arbitrate for L2 access for any
/// posted miss that is not yet handled, and attempt to fill for any that
/// are being handled by L2. This involved polling to see if L2 has set the
/// request_ack'd flag, and when it has counting down the latency timer
/// and then filling the line.
///
/// For MT, choose which mshr to present to L2 arbitration. Use a round-robin
/// scan for fairness
///////////////////////////////////////////////////////////////////////////////////
void 
L1ICache::PerCycle() 
{
  using namespace rigel::cache;
  
  // Allow only a fixed number of fills per cycle.
  int allowed_fills_remaining = 1;
  // can only receive one new grant per cycle also
  bool done_arb=false;
  int current_idx;

#ifdef DEBUG_CACHES
  DEBUG_HEADER();
  cerr << "L1ICache PerCycle validBits First Set: " << validBits.findFirstSet() << std::endl;
#endif
  
  // Iterate across all MSHRs.
  for (int i = 0; i < MSHR_COUNT; i++) {
    // start scan in round-robiin manner until finding one we can handle
    current_idx = (i+miss_buffer_head) % MSHR_COUNT;
    MissHandlingEntry<LINESIZE> &mshr = pendingMiss[current_idx];
    
    // If invalid, nothing to do, try the next one
    if (!mshr.check_valid()) { 
      continue; 
    }
    Profile::accum_cache_stats.L1I_cache_stall_cycles++;

    //check if this has successfully arbitrated
    if (!mshr.IsArbComplete()&&!done_arb){
      mshr.instr->stats.cycles.l2d_arbitrate++;

      if (!NextLevel->Caches->helper_arbitrate_L2D_instr(mshr.get_addr(), mshr.GetCoreID(), mshr.GetThreadID(), L2_READ_CMD)){
#ifdef DEBUG_CACHES
	DEBUG_HEADER();
	cerr << "L1I MSHR["<< current_idx << "] has not won arbitration\n";
#endif
        continue;
      }
#ifdef DEBUG_CACHES
      DEBUG_HEADER();
      cerr << "L1I MSHR["<< current_idx << "] won arbitration\n";
#endif
      done_arb = true;
      mshr.SetArbComplete();
      continue;
    }

    
    // Skip if still waiting on the next level of the cache.  Should not be an
    // issue for the L1D since it is fixed-latency.
    if (!mshr.all_lines_ready()) {
      // sanity check valid MSHR with meaningful message type
      mshr.sanity_check();

      // Try to schedule again
      // If C$ is unified, these should fill into the C$, else either separate
      // L2 I$ or no L2 I$.  In either case, these should not fill into the C$.
      if (USE_L2I_CACHE_UNIFIED) { 
        mshr.SetICMsgType(IC_MSG_READ_REQ); 
      } else {
         mshr.SetICMsgType(IC_MSG_INSTR_READ_REQ); 
      }

      // Try to schedule again
      if (USE_L2I_CACHE_SEPARATE) { 
        NextLevelI->Schedule(LINESIZE, &mshr); 
      } else {
        NextLevel->Schedule(LINESIZE, &mshr); 
      }
    }
    
    // Only allow one fill per cycle
    if (allowed_fills_remaining <= 0) { 
      continue; 
    }
    
    if(mshr.has_ready_line()) {
      // Try to fill the request into the cache.  If it fails, we try again the
      // next cycle.  This should not happen.
      uint32_t fill_addr = mshr.get_fill_addr();
      if (false == Fill(fill_addr))
      {
        continue;
      }
      else //Fill completed
      {
#ifdef DEBUG_CACHES
	DEBUG_HEADER();
	cerr << "L1ICache Fill success";
#endif
        allowed_fills_remaining--;
	miss_buffer_head = (miss_buffer_head + 1) % MSHR_COUNT;
        mshr.notify_fill(fill_addr);
      }
    }
    if(mshr.IsDone()) {
#ifdef DEBUG_CACHES
      cerr << "L1ICache Fill done, clear MSHR ID " << current_idx << " miss_buffer_head now " << miss_buffer_head << std::endl;
#endif
      mshr.clear();
      // set ready to fetch status now that fill has completed so thread can proceed
      // FIXME may want to add a ptr to this cache's owner core to simplify this
      int local_tid = mshr.GetThreadID() % rigel::THREADS_PER_CORE;
      int core_id = mshr.GetCoreID() % rigel::CORES_PER_CLUSTER;
      NextLevel->Caches->GetCluster()->GetCore(core_id).get_thread_state(local_tid)->req_to_fetch=true;
#ifdef DEBUG_MT
      DEBUG_HEADER();
      fprintf(stderr,"Setting req_to_fetch true for core %d thread %d\n",core_id,local_tid);
#endif
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//        class L2ICache         (Implementations)
////////////////////////////////////////////////////////////////////////////////
void 
L2ICache::PerCycle() 
{
  assert(rigel::cache::USE_L2I_CACHE_SEPARATE && "L2 I$ not enabled");

  // Allow only a fixed number of fills per cycle.
  int allowed_fills_remaining = 1;
  
  // Iterate across all MSHRs.
  for (int i = 0; i < MSHR_COUNT; i++)
  {
    MissHandlingEntry<LINESIZE> &mshr = pendingMiss[i];
    
    // If invalid, nothing to do
    if (!mshr.check_valid()) { continue; }
    Profile::accum_cache_stats.L1D_cache_stall_cycles++;
    
    // Skip if still waiting on the next level of the cache.  Should not be an
    // issue for the L1D since it is fixed-latency.
    if(!mshr.all_lines_ready()) {
      if (mshr.GetICMsgType() == IC_MSG_NULL) {
        DEBUG_HEADER();
        fprintf(stderr, "Found NULL IC message in L1D::PerCycle()\n");
        mshr.dump();
        assert(0);
      }
      // Note that this used to turn the ICMsgType into a read.  I think it
      // was a terrible hack to work around a bug.  Bug #47 addresses this
      // issue.  So far, turning it back to an abort() has not been an issue.
      if (mshr.GetICMsgType() == IC_INVALID_DO_NOT_USE) {
        mshr.dump();
        assert(0 && "Invalid message type found");
      }
      mshr.SetICMsgType(IC_MSG_INSTR_READ_REQ);
      // Try to schedule again
      NextLevel->Schedule(LINESIZE, &mshr);
    }
    
    // Only allow one fill per cycle
    if (allowed_fills_remaining <= 0) { continue; }
    
    if(mshr.has_ready_line())
    {
      // Try to fill the request into the cache.  If it fails, we try again the
      // next cycle.  This should not happen.
      uint32_t fill_addr = mshr.get_fill_addr();
      if (false == Fill(fill_addr)) continue;
      else //Fill completed
      {
        allowed_fills_remaining--;
        mshr.notify_fill(fill_addr);
      }
    }
    if(mshr.IsDone())
    {
      mshr.clear();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//        class L1DCache         (Implementations)
////////////////////////////////////////////////////////////////////////////////
void L1DCache::PerCycle() {
  using namespace rigel::cache;
  
  assert(L1D_ENABLED && "Error: L1D is Disabled!");
  
  // After a single access completes, invalidate the line. Used to ensure
  // forward progress on coherence requests.
  if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
    if (temporary_invalidate_pending) {
      TagArray[temporary_access_set][temporary_access_way].invalidate();
      temporary_access_pending = false;
      temporary_invalidate_pending = false;
      temporary_access_set = -1;
      temporary_access_way = -1;
    }
  }
  
  // Allow only a fixed number of fills per cycle.
  int allowed_fills_remaining = 1;
  
  // Iterate across all MSHRs.
  for (int i = 0; i < MSHR_COUNT; i++) {
    MissHandlingEntry<LINESIZE> &mshr = pendingMiss[i];
    
    // If invalid, nothing to do
    if (!mshr.check_valid()) { continue; }
    Profile::accum_cache_stats.L1D_cache_stall_cycles++;
    
    // Skip if still waiting on the next level of the cache.  Should not be an
    // issue for the L1D since it is fixed-latency.
    if(!mshr.all_lines_ready()) {

      if (mshr.GetICMsgType() == IC_MSG_NULL) {
        DEBUG_HEADER();
        fprintf(stderr, "Found NULL IC message in L1D::PerCycle()\n");
        mshr.dump();
        assert(0);
      }
      
      if (mshr.GetICMsgType() == IC_INVALID_DO_NOT_USE) {
        mshr.dump();
        assert(0 && "Invalid message type found");
      }
      
      // Try to schedule again
      NextLevel->Schedule(LINESIZE, &mshr);
    }
    
    // Only allow one fill per cycle
    if (allowed_fills_remaining <= 0) { 
      continue; 
    }
    
    if(mshr.has_ready_line()) {

      // Try to fill the request into the cache.  If it fails, we try again the
      // next cycle.  This should not happen.
      uint32_t fill_addr = mshr.get_fill_addr();
      if (false == Fill(fill_addr)) continue;
      else //Fill completed
      {
        allowed_fills_remaining--;
        mshr.notify_fill(fill_addr);
      }
    }
    
    if(mshr.IsDone()) {
      mshr.clear();
    }

  }
}
// end L1DCache::PerCycle()

/////////////////////////////////////////////////////////////////////////////////
/// L1DCache::Fill()
/////////////////////////////////////////////////////////////////////////////////
/// Used to allocate an entry in the cache
///
/// INPUT : addr of data that needs to be added to cache
/// OUTPUT: True if able to evict and add addr or addr already in cache
///         False if unable to evict and make room
bool 
L1DCache::Fill(uint32_t addr)
{
  // Already in the cache, this is a nop.
  if (IsValidLine(addr)) {
    int set = get_set(addr);
    int way = get_way_line(addr);
    TagArray[set][way].make_all_valid(addr);
    return true;
  }
  
  // Find an empty line.  Eviction occurs if none are available.
  bool stall;
  int set = get_set(addr);
  int way = evict(addr, stall);
  
  // If slot could not be made free this cycle, stall.
  if (stall) { 
    return false; 
  }
  
  // Make everything valid, nothing dirty.
  uint32_t dirty_mask = 0;
  uint32_t valid_mask = CACHE_ALL_WORD_MASK;
  TagArray[set][way].insert(addr, dirty_mask, valid_mask);
  
  return true;
}
// end L1DCache::Fill()
//
