////////////////////////////////////////////////////////////////////////////////
//class GCache          (Implementations)
////////////////////////////////////////////////////////////////////////////////
//
// 'global_cache.cpp' Global cache logic.
//
////////////////////////////////////////////////////////////////////////////////
#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu64
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include <cstdio>                       // for fprintf, stderr, NULL, etc
#include <list>                         // for _List_iterator, list, etc
#include <set>                          // for set, etc
#include <string>                       // for string
#include <vector>                       // for vector, vector<>::iterator
#include "memory/address_mapping.h"  // for AddressMapping
#include "caches_legacy/cache_tags.h"     // for CacheTag
#include "caches_legacy/cache_base.h"  // for CacheAccess_t, etc
#include "caches_legacy/global_cache.h"  // for GlobalCache, etc
#include "caches_legacy/global_cache_debug.h"  // for __DEBUG_ADDR, etc
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for DEBUG_HEADER, etc
#include "directory.h"      // for CacheDirectory
#include "memory/dram.h"           // for BANKS, CONTROLLERS, RANKS, etc
#include "util/dynamic_bitset.h"  // for DynamicBitset
#include "caches_legacy/gcache_request_state_machine.h"
#include "global_network.h"  // for GlobalNetworkBase
#include "icmsg.h"          // for ICMsg
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrCycleStats, InstrStats
#include "interconnect.h"
#include "locality_tracker.h"  // for LocalityTracker, etc
#include "mshr.h"           // for MissHandlingEntry
#include "overflow_directory.h"  // for dir_of_timing_t, etc
#include "profile/profile.h"        // for ProfileStat
#include "profile/profile_names.h"  // for ::STATNAME_GCACHE_PORT, etc
#include "sim.h"            // for CURR_CYCLE, stats, LINESIZE, etc
#include "util/util.h"           // for SimTimer
#include "memory_timing.h"  // for MemoryTimingDRAM
#include "tlb.h"            // for TLB
#include "util/construction_payload.h"
using namespace rigel;

GlobalCache ** GLOBAL_CACHE_PTR;

////////////////////////////////////////////////////////////////////////////////
///GlobalCache::PerCycle()
////////////////////////////////////////////////////////////////////////////////
///  Called every cycle by the timing model.
void
GlobalCache::PerCycle()
{
  //Clock the directory.  For now, this just does stats-gathering,
  //so it can be clocked before or after G$ actions.
  cache_directory->PerCycle();
  //FIXME Refactor out the bank<->queue mapping into a separate piece of policy code.
  int myQueueNum = bankID/rigel::cache::GCACHE_BANKS_PER_MC;
  // Allow the GCache bank to only service a certain number of replies per
  // cycle.  Includes both regular replies, BCASTS, and any coherence traffic.
  replies_this_cycle = rigel::cache::GCACHE_REPLY_PORTS_PER_BANK;
  requests_this_cycle = rigel::cache::GCACHE_REQUEST_PORTS_PER_BANK;
  // Handle any pending directory responses from previous cycles.
  helper_handle_directory_responses(myQueueNum);
  // At this point there may be pending messages in the RequestBuffer.  We
  // should generate pend()s for any of them that are not either valid (in which
  // case we  should generate a reply now) or already pending (in which case we
  // will just ignore the request for now and let it get taken care of when the
  // line does become valid.  Since we check for IsValid() here and reply if it
  // is true here, we do not insert messages down below.  The actual reply
  // messages are handled in PerCycle for GlobalNetwork so all we have to do
  // here is move the request message to the reply buffer and
  // GlobalNetwork->PerCycle() will handle it next cycle
  helper_handle_new_requests(myQueueNum);
  // Now we handle all of the pending requests on the G$ and schedule them,
  // i.e., have them begin their descent back to the cluster via the GNET, if
  // the request is valid in the G$ and it has had its readCycle counter
  // decremented to zero.
  // Fire WDT if a message has been waiting on memory at the GCache for too
  // long.  The GCache will force the memory controller to schedule the oldest
  // operations if any exceed the WDT value.
  helper_handle_pending_requests(myQueueNum);
}

////////////////////////////////////////////////////////////////////////////////
/// GlobalCache::get_set()
////////////////////////////////////////////////////////////////////////////////
/// overloads the CacheBase version to deal with weird DRAM address mapping.
/// calls out to AddressMapping which contains the meat of the implementation
int
GlobalCache::get_set(uint32_t addr) const {
  return AddressMapping::GetGCacheSet(addr, numSets);
}

////////////////////////////////////////////////////////////////////////////////
/// GlobalCache::evict()
////////////////////////////////////////////////////////////////////////////////
///  Handle an eviction from the global cache.  If stall is set, the requestor
///  must wait.  If stall is clear, then the return value is the way within the
///  set where the requester may fill the line.
int
GlobalCache::evict(uint32_t addr, bool &stall)
{
  int victim = 0;
  bool dirty_victim = false;
  int set = get_set(addr);
  uint32_t victim_addr = 0;
  stall = true;

  // Check for locked line waiting for fill first.  We need to lock lines so
  // that we avoid the livelock condition where we continually allocate and then
  // steal a slot.  When we first realize we need a slot, we evict and then lock
  // the line.
  for (int i = 0; i < numWays; i++) {
    if (TagArray[set][i].locked()) {
      victim_addr = TagArray[set][i].victim_addr();
      victim = i;
      // We would not be writing it back if it were not dirty
      dirty_victim = true;
      // Since we already have a victim pending, we try to handle it first.
      goto try_pend;
    }
  }
  // The following statements are enclosed in a block since it crosses the
  // definition of try_pend.
  // If we are to this point, no ways in the set were locked.
  {
    // Always evict an invalid line first
    for (int i = 0; i < numWays; i++) {
      if (!TagArray[set][i].valid()) { victim = i; goto done; }
    }

    // No invalid lines, we must iterate through the LRU stack to find the leas
    // recently used line to replace.
    uint64_t lru_cycle = TagArray[set][0].last_touch();
    uint64_t t;
    int unaccessed_lines = 0;
    // The loop below must start at zero to force each access bit to be checked.
    for (int i = 0;  i < numWays; i++)
    {
      // Skip over lines that were non-speculatively requested, but not yet accessed.
      if (!TagArray[set][i].get_accessed_bit()) {
        unaccessed_lines++;
        //If all lines in the set are locked in the G$ because they haven't been accessed, give up.
        if (unaccessed_lines == numWays) { stall = true; return 0; }
        // Otherwise, skip this way.
        continue;
      }
      // Find the victim via the replacement policy
      if (rigel::CMDLINE_GLOBAL_CACHE_MRU) {
        // Use MRU policy.
        if ((t = TagArray[set][i].last_touch()) > lru_cycle) { lru_cycle = t; victim = i; }
      } else {
        // Default LRU policy.
        if ((t = TagArray[set][i].last_touch()) < lru_cycle) { lru_cycle = t; victim = i; }
      }
    }
  }
  victim_addr = TagArray[set][victim].get_tag_addr();
  dirty_victim = TagArray[set][victim].dirty(victim_addr, CACHE_ALL_WORD_MASK);

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == victim_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER();
    if(dirty_victim)
      fprintf(stderr, "Evicting dirty address 0x%08x at the G$\n", victim_addr);
    else
      fprintf(stderr, "Evicting clean address 0x%08x at the G$\n", victim_addr);
  }
  #endif
  
try_pend:
  //Is the victim dirty and still waiting for a G$ MSHR?
  if ((IsPending(victim_addr) == NULL) && dirty_victim) {
    //Are we out of MSHRs? The true parameter means that it is an eviction.
    if (mshr_full(true)) {
      //Lock this way in as the victim for next cycle.
      //If you don't remember which way you tried last cycle, you can get deadlock.
      stall = true;
      TagArray[set][victim].set_victim_addr(victim_addr);
      TagArray[set][victim].lock(addr);
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == victim_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "Trying to evict dirty address 0x%08x at the G$, but no MSHRs available\n", victim_addr);
      }
      #endif
      return 0;
    }
    // There is an MSHR we can give to the writeback.
    // The Core and Thread numbers passed into pend() are fake because the access is not
    // directly on behalf of a thread.
    // FIXME We should have pseudo core/thread IDs for various non-thread entities in our
    // system (caches especially) so we can do statistics.
    CacheAccess_t ca_out(
      victim_addr,          // address
      -1,                   // core number (faked)
      rigel::POISON_TID,  // thread id
      IC_MSG_EVICT_REQ    , // interconnect message type
      rigel::NullInstr    // no instruction correlated to this access
    );

    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == victim_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "Pend()ing eviction for dirty address 0x%08x at the G$\n", victim_addr);
    }
    #endif
    
    pend(ca_out, 0, -1, true );
    // Line is no longer dirty but it is locked until fill happens from higher
    // level of the cache
    // The idea here is that as soon as we choose to evict a line, it is gone.
    // The problem is that it may not have made its way out to the memory
    // controller where it gets allocated an MSHR and it floats off into the
    // memory.  What we do is immediately invalidate it so any future requests
    // will generate read requests and (hopefully) search the MSHRs for the GCache
    // and find it there if needed.
    TagArray[set][victim].invalidate();
  }
done:
  stall = false;
  return victim;
}
// end GlobalCache::evict()

////////////////////////////////////////////////////////////////////////////////
/// GlobalCache::Fill()
////////////////////////////////////////////////////////////////////////////////
/// Attemp to fill a line into the global cache.  If the fill cannot occur this
/// cycle, the call returns false and must be retried.  The fill may generate an
/// eviction.
bool
GlobalCache::Fill(uint32_t addr, bool was_hwprefetch, bool was_bulkpf, bool dirty, bool nonspeculative)
{
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER();
    fprintf(stderr, "Fill()ing address 0x%08x at the G$\n", addr);
  }
  #endif

  bool valid = IsValidLine(addr);
  int set = get_set(addr);
  bool stall;
  int way = valid ? get_way_line(addr) : evict(addr, stall);
  CacheTag &tag = TagArray[set][way];
  
  // Already in the cache.
  if (valid) {
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "Fill()ing address 0x%08x at the G$ -- Already valid, clearing dirty bits\n", addr);
    }
    #endif

    // For safety, a Fill() on a valid line will set accessed bits.
    tag.set_accessed_bit();
    tag.touch();
    tag.make_all_valid(addr);
    if (dirty) { tag.set_dirty(addr,0xFFFFFFFF); }
  }
  else
  {
    // If the eviction could not occur this cycle, we stall.
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "Fill()ing address 0x%08x at the G$ -- ", addr);
      if(stall)
        fprintf(stderr, "Stalling for eviction\n");
      else
        fprintf(stderr, "Eviction is done or not necessary, proceed with Fill()\n");
    }
    #endif
    if (stall) { return false; }
    // Dirty the entire line if the dirty argument is true
    // This should be the case for writebacks, writes, and C$ evictions.
    uint32_t dirty_mask = dirty ? 0xFFFFFFFF : 0x00000000;
    uint32_t valid_mask = CACHE_ALL_WORD_MASK;
    tag.insert(addr, dirty_mask, valid_mask);
    if (was_hwprefetch) {
      tag.set_hwprefetch();
      tag.clear_bulkprefetch();
      profiler::stats[STATNAME_GCACHE_HWPREFETCH_TOTAL].inc();
      // Speculative accesses do not get their bits set.
      tag.set_accessed_bit();
    } else if (was_bulkpf) {
      tag.set_bulkprefetch();
      tag.clear_hwprefetch();
      profiler::stats[STATNAME_BULK_PREFETCH_TOTAL].inc();
      // Speculative accesses do not get their bits set.
      tag.set_accessed_bit();
    } else {
      tag.clear_hwprefetch();
      tag.clear_bulkprefetch();
      // Non-speculative accesses get their ACC bits cleared.  Until they are set
      // by a demand read, the line is stuck.
      if (nonspeculative) { tag.clear_accessed_bit(); }
      else              { tag.set_accessed_bit(); }
    }
  }

  //We know the fill succeeded if we reached here.  Look through all the pending ICMsgs and
  //send the ones waiting on this address dataBack() messages
  //FIXME this is SLOW.
  //FIXME Refactor bank <-> queue mapping into separate code, keep a pointer to this bank's queue.
  int myQueueNum = bankID/rigel::cache::GCACHE_BANKS_PER_MC;
  std::list<ICMsg> &req_buf = GlobalNetwork->RequestBuffers[myQueueNum][VCN_DEFAULT];
  for(std::list<ICMsg>::iterator rmsg_iter = req_buf.begin(), rmsg_end = req_buf.end();
      rmsg_iter != rmsg_end; ++rmsg_iter) {
    for(std::vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                         end = rmsg_iter->get_sms_end();
        it != end; ++it)
    {
      if(it->getAddr() == addr && it->isPending()) { it->dataBack(); }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// GlobalCache::pend()
////////////////////////////////////////////////////////////////////////////////
void
GlobalCache::pend(
  CacheAccess_t ca_in, 
  int latency,
  int ccache_pending_index, 
  bool is_eviction)
{
  using namespace rigel::cache;

  // Index of new request
  int idx;
  bool vlat = MemoryTimingModel.CheckWaitOnMemSched();
  MissHandlingEntry<rigel::cache::LINESIZE> new_mshr;
  int addr =  ca_in.get_addr() & LINE_MASK;

  assert((!validBits.allSet()) &&
    "Attempting to pend a request but there are no MSHRs available");
    
  if (ca_in.get_icmsg_type() == IC_MSG_NULL)
  {
    DEBUG_HEADER();
    fprintf(stderr, "Found NULL message in pend()! addr: 0x%08x core_id: %d\n",
      addr, ca_in.get_core_id() );
    assert(0);
  }
  // Find an available MSHR in the pending array to replace with a new request.
  if (!PM_find_unused_mshr(idx)) { assert(0 && "Could not find a valid MSHR!"); }

  #ifdef __DEBUG_ADDR
  for(std::set<uint32_t>::const_iterator it = ca_in.get_addrs_begin(),
                                         end = ca_in.get_addrs_end();
      it != end; ++it)
  {
    if (__DEBUG_ADDR == (*it) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "Pend()ing address 0x%08x at the G$\n", (*it));
    }
  }
  #endif

  CacheAccess_t ca_out = ca_in; // same parameters as input
  //FIXME I don't think we actually need to do this masking right now, but we might
  //if we end up having different L2 and L3 cache line sizes.  I commented it out
  //for now because the semantics of set_addr break bulk prefetching because they
  //clear _addrs, then insert addr into the _addr and _addrs members of the CacheAccess_t.
  //See the FIXME in CacheAccess_t about getting rid of separate _addr and _addrs.
  //ca_out.set_addr( addr );
  // Insert the new request.
  new_mshr = PM_insert_new_request(ca_out, idx, ccache_pending_index, is_eviction, vlat);
  // Send request to memory controller
    // This returns a bool representing success or failure, but we ignore it
    // because it will set the ACK bit on the MSHR if it succeeds and we will
    // see that next cycle when we run through the MSHRs.
  scheduleWithGlobalMemory(ca_out.get_addrs(), LINESIZE, new_mshr, idx);

  return;
}
// end GCache::Pend()
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// helper_handle_bcasts()
////////////////////////////////////////////////////////////////////////////////
///returns whether or not the broadcast is complete
bool
GlobalCache::helper_handle_bcasts(const std::list<ICMsg>::iterator &rmsg_iter, std::list<ICMsg> &reply_buffer)
{
  using namespace rigel::interconnect;
  // FIXME: Clean this up
  // We just iterate through each cycle as far as we can inserting broadcasts
  // to all the cluster when one comes to the head of the RequestBuffer.
  if (rmsg_iter->get_bcast_count() < rigel::NUM_CLUSTERS)
  {
    while (reply_buffer.size() < (size_t)MAX_ENTRIES_GCACHE_REPLY_BUFFER &&
           rmsg_iter->get_bcast_count() < rigel::NUM_CLUSTERS)
    {
      // Restrict the number of replies that can be serviced at the GCache
      // each cycle.
      if (replies_this_cycle <= 0) { break; }
      #ifdef DEBUG_BCAST
      DEBUG_HEADER();
      fprintf(stderr, "Inserting BCAST NOTIFY type %d for cluster: %d\n",
              rmsg_iter->GetType(), rmsg_iter->get_bcast_count());
      #endif
      profiler::stats[STATNAME_GCACHE_PORT].inc_histogram();
      replies_this_cycle--;
      ICMsg notify_msg = *rmsg_iter;
      notify_msg.set_cluster_num(rmsg_iter->inc_bcast_count());
      // Give these a unique message number so they don't get rid of the
      // request when the GlobalNetwork tries to inject from ReplyBuffer to
      // GCReplyLinks
      notify_msg.update_msg_number();
      //Will error out if rmsg_iter doesn't point to a bcast request
      notify_msg.set_type(ICMsg::bcast_notify_convert(rmsg_iter->get_type()));
      notify_msg.set_ready_cycle(0);
      reply_buffer.push_back(notify_msg);
    }
    // At the end either we have inserted all of the NOTIFY messages and we
    // can clean up the BCAST_REQ with a REPLY next cycle or we still have
    // more to do and we will have to come back
    return false;
  }
  return true;
}

void
GlobalCache::helper_check_msg_timers(const std::list<ICMsg>::iterator &rmsg_iter) const
{
  if (rmsg_iter->timer_memory_wait.GetRunningTime()
          == (uint32_t)NETWORK_MEM_WAIT_WDT_COUNT)
  {
    DEBUG_HEADER();
    fprintf(stderr, "Interconnect Request Pending for %d cycles!\n",
      NETWORK_MEM_WAIT_WDT_COUNT);
    rmsg_iter->dump(__FILE__, __LINE__);
    fprintf(stderr, "-------Dumping G$ MSHRs-------\n");
    dump();
    fprintf(stderr, "-------End G$ MSHR Dump-------\n");
  }
  if (rmsg_iter->timer_network_occupancy.GetRunningTime()
          == (uint32_t)NETWORK_OCCUPANCY_WDT_COUNT)
  {
    DEBUG_HEADER();
    fprintf(stderr, "Interconnect Request in Network for %d cycles!\n",
      NETWORK_OCCUPANCY_WDT_COUNT);
    rmsg_iter->dump(__FILE__, __LINE__);
    fprintf(stderr, "-------Dumping G$ MSHRs-------\n");
    dump();
    fprintf(stderr, "-------End G$ MSHR Dump-------\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
// helper_handle_prefetches()
////////////////////////////////////////////////////////////////////////////////
void
GlobalCache::helper_handle_prefetches(const std::list<ICMsg>::iterator &rmsg_iter)
{
  bool bulk = rigel::cache::GCACHE_BULK_PREFETCH;
  // Disable prefetches on perfect G$.
  if (rigel::cache::PERFECT_GLOBAL_CACHE) { return; }
  // If we're in bulk prefetch and there are no MSHRs, return
  if (bulk && mshr_full(false)) { return; }

  //If bulk prefetch is on, pend all of them at once
  std::set<uint32_t> s;

  uint32_t addr = rmsg_iter->get_addr();
  unsigned int ctrl = AddressMapping::GetController(addr), rank = AddressMapping::GetRank(addr),
      bank = AddressMapping::GetBank(addr), row = AddressMapping::GetRow(addr);
  
  for (int prefetch_count = 0; prefetch_count < CMDLINE_GCACHE_PREFETCH_SIZE; prefetch_count++)
  {
    uint32_t pref_addr = rmsg_iter->get_addr()+(LINESIZE*(1+prefetch_count));
    // If the line is already in the cache, do not prefetch it.
    // FIXME: May need to drop this behavior due to HW implementation concerns
    if (IsValidLine(pref_addr)) continue;
    if (IsPending(pref_addr) != NULL) continue;
    // If the line is currently being evicted, do not snag an MSHR.
    if (is_being_evicted(pref_addr)) continue;
    // Make sure that this address maps to this bank.
    // If it doesn't, subsequent lines won't either, so we can break;
    if (AddressMapping::GetGCacheBank(pref_addr) != bankID) break;

    if (rigel::cache::GCACHE_BULK_PREFETCH)
    {
      if(AddressMapping::GetRow(pref_addr) == row && AddressMapping::GetBank(pref_addr) == bank &&
         AddressMapping::GetRank(pref_addr) == rank && AddressMapping::GetController(pref_addr) == ctrl)
        s.insert(pref_addr);
      else
        break;
    }
    else
    {
        // If we run out of MSHRs, we can just drop the request.
        if (mshr_full(false))
          break;
      // Insert the prefetch
      // core_id will contain the global core number of the core which generated the request
      // (note that we multiply by CORES_PER_CLUSTER+1 because C$ evictions/writebacks are given
      // their own "core number"),
      // and ccache_pending_index is meaningless
      // TODO: Track G$ evictions and overflow directory-related DRAM accesses separately
      CacheAccess_t ca_out(
        pref_addr,                    // address
        rmsg_iter->GetCoreID()+rmsg_iter->get_cluster()*(rigel::CORES_PER_CLUSTER+1),
        rmsg_iter->GetThreadID(),     // thread id
        IC_MSG_GCACHE_HWPREFETCH_REQ, // interconnect message type
        rigel::NullInstr            // no instruction correlated to this access
      );
      pend(ca_out,
          100000000, // latency
          -42,       // pending index
          false      // is_eviction
      );
    }
  }
  //For bulk, pend one request
  if(bulk)
  {
    if(s.empty()) return;
    //Else, s has addresses in it.
    CacheAccess_t ca_out(
      s,                    // addresses
      rmsg_iter->GetCoreID()+rmsg_iter->get_cluster()*(rigel::CORES_PER_CLUSTER+1),
      rmsg_iter->GetThreadID(),     // thread id
      IC_MSG_GCACHE_HWPREFETCH_REQ, // interconnect message type
      rigel::NullInstr            // no instruction correlated to this access
    );
    pend(ca_out,
         100000000, // latency
         -42,       // pending index
         false      // is_eviction
    );
  }
}

////////////////////////////////////////////////////////////////////////////////
// helper_handle_gcache_request_hit()
////////////////////////////////////////////////////////////////////////////////
void
GlobalCache::helper_handle_gcache_request_hit(std::list<ICMsg>::iterator &rmsg_iter)
{
  using namespace rigel::interconnect;
  //FIXME Keep a pointer to this bank's queue instead of recomputing every time.
  int myQueueNum = bankID/rigel::cache::GCACHE_BANKS_PER_MC;
  //fprintf(stderr, "%08"PRIx64": Servicing a hit:\n");
  //rmsg_iter->dump(__FILE__, __LINE__);
  // Restrict the number of requests into the GCache bank that we will allow.
  if (requests_this_cycle <= 0) {
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == rmsg_iter->get_addr() && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "Ran out of requests while servicing a hit:\n");
      rmsg_iter->dump(__FILE__, __LINE__);
    }
    #endif
    return;
  }
  if (GlobalNetwork->ReplyBuffers[myQueueNum][VCN_DEFAULT].size()
        < (size_t)MAX_ENTRIES_GCACHE_REPLY_BUFFER &&
        rmsg_iter->get_type() != IC_MSG_PREFETCH_BLOCK_GC_REQ)
  {
    profiler::stats[STATNAME_GCACHE_PORT].inc_histogram();
    requests_this_cycle--;
  }
  for(std::vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                       end = rmsg_iter->get_sms_end();
      it != end; ++it)
  {
    // Only consider requests in the WAITING_TO_REPLY state.
    if(!it->isWaitingToReply()) { continue; }
    const uint32_t addr = it->getAddr();
    //If ReplyBuffer is full, return.
    if(GlobalNetwork->ReplyBuffers[myQueueNum][VCN_DEFAULT].size() >= (size_t)MAX_ENTRIES_GCACHE_REPLY_BUFFER) {
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "G$ Hit for addr %08X ReplyBuffer FULL!: size: %zu num replies: %zu", addr,
                GlobalNetwork->ReplyBuffers[myQueueNum][VCN_DEFAULT].size(), MAX_ENTRIES_GCACHE_REPLY_BUFFER);
        rmsg_iter->dump(__FILE__, __LINE__);
      }
      #endif
      return;
    }
    // Restrict the number of replies that can be serviced at the GCache
    // each cycle.
    if (replies_this_cycle <= 0) { return; }
    profiler::stats[STATNAME_GCACHE_PORT].inc_histogram();
    replies_this_cycle--;
    
    #ifdef __DEBUG_ADDR
    if ((__DEBUG_ADDR == addr) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "Handling reply (address %08X).", addr);
      rmsg_iter->dump(__FILE__, __LINE__);
    }
    #endif

    // We are about to reply to the request so clear the bit in the directory
    // entry locking this line.
    if (ENABLE_EXPERIMENTAL_DIRECTORY) {
      if (rmsg_iter->get_type() != IC_MSG_BCAST_UPDATE_REQ &&
          rmsg_iter->get_type() != IC_MSG_BCAST_INV_REQ    &&
         !ICMsg::check_is_global_operation(rmsg_iter->get_type()))
      {
        cache_directory->clear_fill_pending(addr, rmsg_iter->get_cluster());
      }
    }

    // Since this is the only place that a message can get turned around, we
    // can snarf statistics here.
    profiler::stats[STATNAME_GLOBAL_CACHE_RW_ACCESSES].inc();
    // If this was a G$ prefetch, a hit here means a core is actually trying to read from this line,
    // and the prefetch was useful.  Clear the HWPrefetch bit to prevent
    // double-counting on subsequent accesses.
    if (IsHWPrefetch(addr)) {
      ClearHWPrefetch(addr);
      profiler::stats[STATNAME_GCACHE_HWPREFETCH_USEFUL].inc();
    }

    if (IsBulkPrefetch(addr)) {
      ClearBulkPrefetch(addr);
      profiler::stats[STATNAME_BULK_PREFETCH_USEFUL].inc();
    }    

    // NOTE: This used to be after the Inject() call, which could leave a
    // wild pointer in the messaged pushed into the ReplyBuffer. [20090909]
    if (ENABLE_EXPERIMENTAL_DIRECTORY) {
      // Free the dir timer if it was allocated.
      if (rmsg_iter->get_dir_timer()) {
        delete rmsg_iter->get_dir_timer();
        rmsg_iter->set_dir_timer(NULL);
      }
      // Handle the logical channel sequence number on a read.
      if (IC_MSG_READ_REQ == rmsg_iter->get_type() ||
          IC_MSG_CCACHE_HWPREFETCH_REQ == rmsg_iter->get_type() ||
          IC_MSG_PREFETCH_BLOCK_CC_REQ == rmsg_iter->get_type())
      {
        cache_directory->update_logical_channel(
          rmsg_iter->get_cluster(),
          rmsg_iter->get_seq_channel());
      }
    }
    using namespace rigel::profiler;
    // Tabulate directory hits/misses:
    if (rmsg_iter->check_directory_access()) {
      if (rmsg_iter->directory_access_hit()) { stats[STATNAME_DIRECTORY].inc_hits(); } 
      else                                   { stats[STATNAME_DIRECTORY].inc_misses(); }
    }
    // Tabulate global cache hits/misses:
    if (rmsg_iter->check_global_cache_access()) {
      if (rmsg_iter->global_cache_access_hit()) { stats[STATNAME_GLOBAL_CACHE].inc_hits(); } 
      else                                      { stats[STATNAME_GLOBAL_CACHE].inc_misses(); }
    }
    // Set the accessed bits for the line in the GCache.  The line can be evicted now.
    set_accessed_bits(addr);
    // Return the message to the requesting cluster.
    rmsg_iter->set_ready_cycle(0);
    // FIXME Deal with injection failure!
    // Don't do this for bulk prefetches (just kill the message)
    if(rmsg_iter->get_type() != IC_MSG_PREFETCH_BLOCK_GC_REQ) {
      //Copy-construct new message.
      ICMsg newMsg(*rmsg_iter);
      //Remove all addresses, add the one we want.
      newMsg.remove_all_addrs();
      newMsg.add_addr(addr);

      //Put addr in the DONE state in *rmsg_iter.
      //NOTE: Should only be done if inject succeeds.  FIXME fix this when we put the
      //backpressure mechanism back into the GNet.

      /*
      //Remove all addresses from newMsg except those that are in WAITING_TO_REPLY
      std::vector<GCacheRequestStateMachine>::iterator nmit = newMsg.get_sms_begin();
      while(nmit != newMsg.get_sms_end()) {
        if(!nmit->isWaitingToReply())
        {
          newMsg.remove_addr(nmit);
          nmit = newMsg.get_sms_begin();
        }
        else
          nmit->replySent();
      }
      for(std::vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                        end = rmsg_iter->get_sms_end();
          it != end; ++it)
      {
        if(it->isWaitingToReply())
        {
          it->replySent();
        }
      }
      */
      GlobalNetwork->ReplyBufferInject(myQueueNum, VCN_DEFAULT, newMsg);
    }
   
    it->replySent();
    // Unlock pending request
    cache_directory->clear_atomic_lock(addr, rmsg_iter->get_cluster());
    // Dirty the line if this was a write
    switch(rmsg_iter->get_type())
    {
      case IC_MSG_DIRECTORY_WRITE_REQ:
      case IC_MSG_WRITE_REQ:
      case IC_MSG_EVICT_REQ:
      case IC_MSG_GLOBAL_WRITE_REQ:
      case IC_MSG_LINE_WRITEBACK_REQ:
        setDirtyLine(addr);
        break;
      default: { break; }
    }
    // Make sure we touch the line in the G$ to update LRU.
    touch(addr);

    if (rmsg_iter->get_incoherent()) { profiler::stats[STATNAME_DIR_INCOHERENT_ACCESSES].inc(); }
    // If all addresses are in DONE state, remove the message from the RequestBuffer.
    if(rmsg_iter->all_addrs_done()) {
      rmsg_iter = GlobalNetwork->RequestBufferRemove(myQueueNum, VCN_DEFAULT, rmsg_iter);
      return;
    }
  }
}

bool GlobalCache::scheduleWithGlobalMemory(uint32_t addr, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  int PendingMissEntryIndex)
{
  std::set<uint32_t> s;
  s.insert(addr);
  return scheduleWithGlobalMemory(s, size, MSHR, PendingMissEntryIndex);
}

bool GlobalCache::scheduleWithGlobalMemory(const std::set<uint32_t> addrs, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  int PendingMissEntryIndex)
{
  bool ret = MemoryTimingModel.Schedule(addrs, size, MSHR, this, PendingMissEntryIndex);
  if (ret)
  {
    //Set the ack bit on the MSHR so we stop trying to schedule this request
    PM_set_acked(PendingMissEntryIndex);
    // Track locality
    // Directory accesses and evictions for now have negative core id's.  we may want to track
    // them in future. FIXME
    if (rigel::ENABLE_LOCALITY_TRACKING)
    {
      if (MSHR.GetCoreID() >= 0)
      {
        unsigned int cluster = MSHR.GetCoreID()/(rigel::CORES_PER_CLUSTER+1);
        //unsigned int core = MSHR.GetCoreID()%(rigel::CORES_PER_CLUSTER+1);
        for(std::set<uint32_t>::iterator it = addrs.begin(); it != addrs.end(); ++it)
        {
          //perCoreLocalityTracker[cluster][core]->addAccess(*it);
          perClusterLocalityTracker[cluster]->addAccess(*it);
          aggregateLocalityTracker->addAccess(*it);
        }
      }
    }
    //Track global TLB behavior
    if (rigel::ENABLE_TLB_TRACKING)
    {
      for(size_t i = 0; i < num_tlbs; i++)
        for(std::set<uint32_t>::iterator it = addrs.begin(); it != addrs.end(); ++it)
          tlbs[i]->access(*it);
    }
  }
  return ret;
}

void GlobalCache::dump_locality() const
{
  assert(rigel::ENABLE_LOCALITY_TRACKING &&
         "Locality tracking disabled, G$::dump_locality() called");
  //uint64_t sumOfPerCoreConflicts = 0UL;
  uint64_t sumOfPerClusterConflicts = 0UL;

  for(int i = 0; i < rigel::NUM_CLUSTERS; i++)
  {
    //for(int j = 0; j < (rigel::CORES_PER_CLUSTER+1); j++)
    //{
    //  perCoreLocalityTracker[i][j]->dump();
    // sumOfPerCoreConflicts += perCoreLocalityTracker[i][j]->getNumConflicts();
    //}
    perClusterLocalityTracker[i]->dump();
    sumOfPerClusterConflicts += perClusterLocalityTracker[i]->getNumConflicts();
  }
  aggregateLocalityTracker->dump();
  uint64_t aggregateConflicts = aggregateLocalityTracker->getNumConflicts();
  //printf("G$ Bank %d: %"PRIu64" aggregate conflicts, %"PRIu64" per-cluster conflicts"
  //       ", %"PRIu64" per-stream conflicts\n",
  //bankID, aggregateConflicts, sumOfPerClusterConflicts, sumOfPerCoreConflicts);
  printf("G$ Bank %u: %"PRIu64" aggregate conflicts, %"PRIu64" per-cluster conflicts\n",
  bankID, aggregateConflicts, sumOfPerClusterConflicts);
}

void GlobalCache::dump_tlb() const
{
  assert(rigel::ENABLE_TLB_TRACKING &&
         "TLB tracking disabled, G$::dump_tlb() called");
  for(size_t i = 0; i < num_tlbs; i++)
    tlbs[i]->dump(stderr);
}

GlobalCache::GlobalCache(rigel::ConstructionPayload cp) :
      CacheBase< GCACHE_WAYS, GCACHE_SETS, LINESIZE, GCACHE_OUTSTANDING_MISSES,
                 CACHE_WRITE_THROUGH, GCACHE_EVICTION_BUFFER_SIZE >::CacheBase(),
      bankID(cp.component_index),
      MemoryTimingModel(*(cp.memory_timing))
{
  // Initialize directory for the GCache bank.
  cache_directory = new CacheDirectory;
  cache_directory->init(bankID);
  probe_dir_timer = new dir_of_timing_t;

  if(rigel::ENABLE_LOCALITY_TRACKING)
  {
    //Initialize locality trackers
    //perCoreLocalityTracker = new LocalityTracker **[rigel::NUM_CLUSTERS];
    perClusterLocalityTracker = new LocalityTracker *[rigel::NUM_CLUSTERS];
    for(int i = 0; i < rigel::NUM_CLUSTERS; i++)
    {
      char name[128];
      snprintf(name, 128, "G$ Bank %u Cluster %d DRAM Accesses", bankID, i);
      std::string cppname(name);
      perClusterLocalityTracker[i] = new LocalityTracker(cppname, 8, LT_POLICY_RR,
        rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
        rigel::DRAM::BANKS, rigel::DRAM::ROWS);
      //perCoreLocalityTracker[i] = new LocalityTracker *[rigel::CORES_PER_CLUSTER+1];
      //for(int j = 0; j < (rigel::CORES_PER_CLUSTER+1); j++)
      //{
      //  char name2[128];
      //  snprintf(name2, 128, "G$ Bank %d Cluster %d Core %d DRAM Accesses", bankID, i, j);
      //  std::string cppname2(name2);
      //  perCoreLocalityTracker[i][j] = new LocalityTracker(cppname2, 8, LT_POLICY_RR,
      //    rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
      //    rigel::DRAM::BANKS, rigel::DRAM::ROWS);
      //}
    }
    char name[128];
    snprintf(name, 128, "G$ Bank %u Aggregate DRAM Accesses", bankID);
    std::string cppname(name);
    aggregateLocalityTracker = new LocalityTracker(cppname, 8, LT_POLICY_RR,
      rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
      rigel::DRAM::BANKS, rigel::DRAM::ROWS);
  }

  if(rigel::ENABLE_TLB_TRACKING) {
    //num_tlbs = 8;
    //num_tlbs = 16+14+12;
    //num_tlbs = 4*3*17*2;
		//num_tlbs = (6+5+4+3+2+1)*9;
    num_tlbs = 11*16;

    tlbs = new TLB *[num_tlbs];
    TLB **walker = tlbs;

    char name[128];
    snprintf(name, 128, "G$ Bank %u", bankID);

    // *(walker++) = new TLB(name, 4, 1, 12, LRU);
    // *(walker++) = new TLB(name, 4, 1, 12, LFU);
    // *(walker++) = new TLB(name, 8, 1, 12, LRU);
    // *(walker++) = new TLB(name, 8, 1, 12, LFU);
    // *(walker++) = new TLB(name, 16, 1, 12, LRU);
    // *(walker++) = new TLB(name, 16, 1, 12, LFU);
    // *(walker++) = new TLB(name, 32, 1, 12, LRU);
    // *(walker++) = new TLB(name, 32, 1, 12, LFU);
    // *(walker++) = new TLB(name, 64, 1, 12, LRU);
    // *(walker++) = new TLB(name, 64, 1, 12, LFU);
    // *(walker++) = new TLB(name, 128, 1, 12, LRU);
    // *(walker++) = new TLB(name, 128, 1, 12, LFU);
    // *(walker++) = new TLB(name, 256, 1, 12, LRU);
    // *(walker++) = new TLB(name, 256, 1, 12, LFU);
    // *(walker++) = new TLB(name, 512, 1, 12, LRU);
    // *(walker++) = new TLB(name, 512, 1, 12, LFU);

    // *(walker++) = new TLB(name, 1, 1, 12, LRU);
    // *(walker++) = new TLB(name, 1, 1, 12, LFU);
    // *(walker++) = new TLB(name, 1, 2, 12, LRU);
    // *(walker++) = new TLB(name, 1, 2, 12, LFU);
    // *(walker++) = new TLB(name, 1, 4, 12, LRU);
    // *(walker++) = new TLB(name, 1, 4, 12, LFU);
    // *(walker++) = new TLB(name, 1, 8, 12, LRU);
    // *(walker++) = new TLB(name, 1, 8, 12, LFU);
    // *(walker++) = new TLB(name, 1, 16, 12, LRU);
    // *(walker++) = new TLB(name, 1, 16, 12, LFU);
    // *(walker++) = new TLB(name, 1, 32, 12, LRU);
    // *(walker++) = new TLB(name, 1, 32, 12, LFU);
    // *(walker++) = new TLB(name, 1, 64, 12, LRU);
    // *(walker++) = new TLB(name, 1, 64, 12, LFU);

    // *(walker++) = new TLB(name, 1, 32, 2, LRU);
    // *(walker++) = new TLB(name, 1, 32, 2, LFU);
    // *(walker++) = new TLB(name, 1, 32, 6, LRU);
    // *(walker++) = new TLB(name, 1, 32, 6, LFU);
    // *(walker++) = new TLB(name, 1, 32, 10, LRU);
    // *(walker++) = new TLB(name, 1, 32, 10, LFU);
    // *(walker++) = new TLB(name, 1, 32, 14, LRU);
    // *(walker++) = new TLB(name, 1, 32, 14, LFU);
    // *(walker++) = new TLB(name, 1, 32, 18, LRU);
    // *(walker++) = new TLB(name, 1, 32, 18, LFU);
    // *(walker++) = new TLB(name, 1, 32, 22, LRU);
    // *(walker++) = new TLB(name, 1, 32, 22, LFU);

		//From 1 to 1024 entries, from direct mapped to fully associative,
		//From 64B to 4MB page size
    for(unsigned int i = 0; i <= 0; i += 1) {
      for(unsigned int j = 0; j <= 10; j += 1) {
        for(unsigned int k = 5; k <= 20; k += 1) {
          *(walker++) = new TLB(name, (1 << i), (1 << j), k, LRU);
          //*(walker++) = new TLB(name, (1 << i), (1 << j), k, MRU);
          //*(walker++) = new TLB(name, (1 << i), (1 << j), k, LFU);
        }
      }
    }
  }

  rigel::GLOBAL_debug_sim.gcache.push_back(this);
}

inline void
GlobalCache::set_accessed_bits(uint32_t addr)
{
  int set = get_set(addr);

  for (int i = 0; i < numWays; i++) {
    if (TagArray[set][i].get_tag_addr() == addr) {
        if (TagArray[set][i].valid()) {
          TagArray[set][i].set_accessed_bit();
        }
      }
  }
}

void
GlobalCache::helper_handle_pending_requests(int myQueueNum)
{
  int force_mshr_sched = -1;
  int force_mshr_time_max = -1;
  // Early exit for the case when nothing is valid. TODO: Move this block to a
  // separate function.
  if (!PM_none_pending())
  {

    // Check WDTs, force iteration through MSHRs to start with longest pending
    for(int i = 0; i < MSHR_COUNT; i++)
    {
      if(!PM_get_valid(i))
        continue;
      int wait_time = PM_get_WDTwait(i);
      if (wait_time > GCACHE_PENDING_WDT_COUNT) {
        if (wait_time % CACHE_PENDING_PRINT_THROTTLE == 0) {
          DEBUG_HEADER();
          fprintf(stderr, "[[WARNING]] Found GCache MSHR #%d pending"
          " for %d cycles!\n", i, wait_time);
          PM_dump(i);
        }
        if (wait_time > force_mshr_time_max) {
          force_mshr_time_max = wait_time;
          force_mshr_sched = i;
        }
      }
    }
    
    // Iterate over all MSHRs starting with the forced MSHR if needed
    int start_iter = (force_mshr_sched >= 0) ?  force_mshr_sched : CURR_CYCLE & (MSHR_COUNT-1);
    for (int i = start_iter, count = 0; count < MSHR_COUNT; i = ((i + 1) & (MSHR_COUNT-1)), count++)
    {
      using namespace rigel::cache;
      // If invalid, nothing to do.  FIXME: switch to PM_valid eventually.
      if (!PM_get_valid(i)){ continue; }
      
      // If the request has not been acked yet, it means that the memory
      // controller has not responded to our request.  Ask again.
      if (!PM_get_acked(i))
      {
          scheduleWithGlobalMemory(PM_get_addrs(i), LINESIZE, PM_get_mshr(i), i);
          continue;
      }

      // Check for pending miss having a line ready to fill.  If so, update cache and clear MSHR
      if (PM_has_ready_line(i))
      {
        if (IC_MSG_CCACHE_HWPREFETCH_REPLY == PM_get_icmsg_type(i)) {
          DEBUG_HEADER();
          fprintf(stderr, "Found REPLY where only REQ was expected for CC PF");
          PM_dump(i);
        }
        //If this was an eviction, it is now complete and we can just notify_fill()
        //(nothing actually fills into the cache.)
        if (IC_MSG_EVICT_REQ == PM_get_icmsg_type(i)) {
          pendingMiss[i].notify_fill(PM_get_fill_addr(i));
        }
        else if (IC_MSG_PREFETCH_NGA_REQ == PM_get_icmsg_type(i))
        {
          //We don't need to do a fill on a G$ writeback (evicting dirty data)
          std::list<ICMsg>::iterator rmsg_iter;
          for ( rmsg_iter = GlobalNetwork->RequestBuffers[myQueueNum][VCN_DEFAULT].begin();
             rmsg_iter != GlobalNetwork->RequestBuffers[myQueueNum][VCN_DEFAULT].end();
             rmsg_iter++)
          {
            if ((rmsg_iter->get_addr() & rigel::cache::LINE_MASK) ==
                  (PM_get_fill_addr(i) & rigel::cache::LINE_MASK))
            {
              // Communicate that this request is ready to be reinjected into the
              // network since it will not be seen as valid in the global cache.
              for(std::vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                                   end = rmsg_iter->get_sms_end();
                  it != end; ++it)
              {
                if(it->getAddr() == rmsg_iter->get_addr())
                  it->dataBack();
              }
            }
          }
        }
        else
        {
          bool was_hwprefetch, was_bulkpf, nonspeculative;
          const icmsg_type_t msg_type = PM_get_icmsg_type(i);
          switch(msg_type)
          {
            case IC_MSG_GCACHE_HWPREFETCH_REQ:
            case IC_MSG_GCACHE_HWPREFETCH_REPLY:
              was_hwprefetch = true;
              was_bulkpf = false;
              break;
            case IC_MSG_PREFETCH_BLOCK_GC_REQ:
            case IC_MSG_PREFETCH_BLOCK_GC_REPLY:              
              was_bulkpf = true;
              was_hwprefetch = false;
              break;
            default:
              was_hwprefetch = false;
              was_bulkpf = false;
              break;
          }
          
          // If it is a directory request or speculative fill, we need to unlock the line by
          // touching the accessed bit.
          switch(msg_type)
          {
            case IC_MSG_PREFETCH_BLOCK_GC_REQ:
            case IC_MSG_PREFETCH_BLOCK_GC_REPLY:
            case IC_MSG_DIRECTORY_READ_REQ:
              nonspeculative = false;
              break;
            default:
              nonspeculative = true;
              break;
          }
          
          if (PM_get_instr(i)) PM_get_instr(i)->stats.cycles.gc_fill++;
          
          // This is the only fill that may lock a line into the cache.
          uint32_t fill_address = PM_get_fill_addr(i);
          if (false == Fill(fill_address, was_hwprefetch, was_bulkpf, false /* don't dirty */, nonspeculative)) {
            continue;
          }
          else
          {
            pendingMiss[i].notify_fill(fill_address);
          }
        }

        if(pendingMiss[i].IsDone())
        {
          PM_clear(i);
        }
      }
    }
  }
}

void
GlobalCache::helper_handle_new_requests(int myQueueNum)
{
  using namespace rigel::cache;
  // Handle directory access.  If dir_req is true, the request to process is
  // not the actual demand address, but the value for the directory.
  bool dir_req = false;
  uint32_t dir_req_addr = 0x0;
  icmsg_type_t dir_req_msg_type = IC_MSG_NULL;
  // Address to handle this cycle.
  uint32_t lookup_addr = 0x0;
  // This loop must be structured as a while, with explicit ++rmsg_iter everytime we want
  // to continue to the next message (as opposed to a for loop which automatically does
  // ++rmsg_iter every iteration), because an element may be deleted from the list midway
  // through the loop.  if the last element in the list is deleted, a ++rmsg_iter on
  // Global->RequestBuffers[myQueueNum][VCN_DEFAULT].end() (as defined by STL list::erase())
  // will yield something undefined and probably not work.  See Bugzilla Bug #158

  std::list<ICMsg> &req_buf = GlobalNetwork->RequestBuffers[myQueueNum][VCN_DEFAULT];
  std::list<ICMsg>::iterator rmsg_iter = req_buf.begin();
  while (rmsg_iter != req_buf.end())
  {
    // Only worry about those requests which map to this bank
    // FIXME Deal with requests that have some addresses in this bank and some in other banks.
    if (AddressMapping::GetGCacheBank(rmsg_iter->get_addr()) != bankID) { ++rmsg_iter; continue; }
    // CHECK: Has this been stuck in the network buffer for too long?
    helper_check_msg_timers(rmsg_iter);
    // Set message as being seen by the GCache scheduler this cycle.
    rmsg_iter->set_gcache_last_touch_cycle();
    // Handle broadcasts as needed.
    if (ICMsg::check_is_bcast_req(rmsg_iter->get_type())) {
      //If the broadcast is not yet complete
      if (!helper_handle_bcasts(rmsg_iter, GlobalNetwork->ReplyBuffers[myQueueNum][VCN_DEFAULT]))
      {
        ++rmsg_iter;
        continue;
      }
    }
    // If the directory entry for the address is locked, we do not allow other
    // requests to the line to proceed here.
    uint32_t req_addr = rmsg_iter->get_addr();
    int req_cluster = rmsg_iter->get_cluster();
    if (cache_directory->check_atomic_lock(req_addr, req_cluster)) { ++rmsg_iter; continue; }
    // Track whether this is a G$ hit or miss.
    // FIXME does not work for multi-address messages
    rmsg_iter->set_global_cache_hit(IsValidLine(req_addr));
    // If we need to access memory on behalf of the coherence implementation, it
    // takes place here.   The dir_req will keep trying until it succeeds and
    // then the actual request can move forward.
    dir_req = false;
    bool dir_access_succ = helper_handle_directory_access(*rmsg_iter);

    if ((CMDLINE_ENABLE_HYBRID_COHERENCE || ENABLE_OVERFLOW_DIRECTORY) &&
        !rmsg_iter->gcache_dir_proxy_accesses_complete)
    {
      // Check to see if we are done with the look up.
      if (rmsg_iter->get_dir_timer()->done()) {
        rmsg_iter->gcache_dir_proxy_accesses_complete = true;
        if (!dir_access_succ) { ++rmsg_iter; continue; }
      } else {
        // Grab the next address in the trace to sent to the cache model/memory
        // model.  Only delete it once it is a hit.
        dir_of_timing_entry_t nextAccess = rmsg_iter->get_dir_timer()->next();
        dir_req_addr = nextAccess.addr;
        dir_req_msg_type = nextAccess.type;
        dir_req = true;
        // Burn a cycle on a hit, otherwise process miss.
        if (IsValidLine(dir_req_addr)) {
          // Add in the number of cycles used for overflow requests.
          profiler::stats[STATNAME_GLOBAL_CACHE_OF_ACCESSES].inc();
          touch(dir_req_addr);
          rmsg_iter->get_dir_timer()->remove();
          ++rmsg_iter;
          continue;
        }
      }
    } else {
      // If the directory access succeeded and there are no more misses pending,
      // move forward.
      if (!dir_access_succ) { ++rmsg_iter; continue; }
    }

    //We want a vector<GCRSM> that we're trying to pend this cycle.  It will either be the vector
    //from the current ICMsg, or a dummy vector containing just the directory request address.
    std::vector<GCacheRequestStateMachine> directory_request;
    if(dir_req) {
      assert(ENABLE_EXPERIMENTAL_DIRECTORY && "Can only get here with coherence enabled.");
      if (IsValidLine(dir_req_addr)) {
        DEBUG_HEADER();
        fprintf(stderr, "[BUG] Must be a miss at this point! target_addr: %08x\n", dir_req_addr);
        assert(0);
      }
      directory_request.push_back(GCacheRequestStateMachine(dir_req_addr));
    }
    //std::vector<GCacheRequestStateMachine>::iterator dir_req_it_begin = directory_request.begin(),
    //                                                 dir_req_it_end   = directory_request.end();
    //std::vector<GCacheRequestStateMachine>::iterator &lookup_begin = dir_req ? dir_req_it_begin : rmsg_iter->get_sms_begin(),
    //                                                 &lookup_end = dir_req ? dir_req_it_end : rmsg_iter->get_sms_end();

    lookup_addr = dir_req ? dir_req_addr : rmsg_iter->get_addr();
    
    // FIRST: Is it valid?  Can we send it now? This is the authoritative check
    // that will return the message.  There is also the chance that this request
    // should not fill into the GCache and memory has gotten back to us with the
    // result in which case the check_memaccess_done() call will
    // return true.  In the event that an NGA memory operation is called on a
    // line valid in the GCache, the first predicate will be true (IsValid) so
    // no memory request is generated.  NOTE: Nothing leaves the RequestBuffer
    // except through this point.
    if (__DEBUG_ADDR == lookup_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      rmsg_iter->dump(__FILE__, __LINE__);
      if (IsValidLine(lookup_addr)) {
        fprintf(stderr, "[LINE IS VALID] addr: %08x\n", lookup_addr);
      } else {
        fprintf(stderr, "[LINE IS NOT VALID] addr: %08x\n ", lookup_addr);
      }
      if (rmsg_iter->check_memaccess_done()) {
        fprintf(stderr, "[MEMACCESS DONE] addr: %08x\n ", lookup_addr);
      }
    }

    //Short-circuit addresses that are already valid in the cache w/o having to pend.
    //FIXME This is too optimistic.  We should model a realistic number of ports and stick to it.
    if(!dir_req) {
      for(vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                      end = rmsg_iter->get_sms_end();
          it != end; ++it)
      {
        if(IsValidLine(it->getAddr()) && it->isWaitingToPend())
          it->wasValid();
      }
    }

    // Added the ability to remove write-allocate when doing W->W transfers.
    // It drastically cuts down on DRAM write traffic.
    bool is_wr2wr_transfer = ( CMDLINE_ENABLE_GCACHE_NOALLOCATE_WR2WR_TRANSFERS &&
                               ENABLE_EXPERIMENTAL_DIRECTORY &&
                               !dir_req &&
                               rmsg_iter->get_type() == IC_MSG_WRITE_REQ) ? true : false;
    //Need to short-circuit these so that we can send the reply.
    if(is_wr2wr_transfer)
    {
      for(std::vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(), end = rmsg_iter->get_sms_end();
          it != end; ++it)
      {
        if(it->isWaitingToPend())
          it->wasValid();
      }
    }

    bool some_lines_waiting_to_reply = false;
    for(vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                  end = rmsg_iter->get_sms_end();
        it != end; ++it)
    {
      if(it->isWaitingToReply()) {
        some_lines_waiting_to_reply = true;
        break;
      }
    }
    
    
    //FIXME We need some way to send back multiple down messages for one up message.
    if ((!dir_req && rmsg_iter->check_memaccess_done())
      || some_lines_waiting_to_reply
      || rigel::cache::PERFECT_GLOBAL_CACHE
      || is_wr2wr_transfer)
    {
      if (__DEBUG_ADDR == lookup_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "GCACHE REQUEST HIT: ");
        rmsg_iter->dump(__FILE__, __LINE__);
      }
      /*
      printf("HIT: ");
      if(!dir_req && rmsg_iter->check_memaccess_done())
        printf("MEMACCESSDONE ");
      if((rmsg_iter->get_type() != IC_MSG_PREFETCH_BLOCK_GC_REQ && some_lines_waiting_to_reply))
        printf("WAITINGTOREPLY ");
      if(rigel::cache::PERFECT_GLOBAL_CACHE)
        printf("PERFECTGC ");
      if(is_wr2wr_transfer)
        printf("WR2WR ");
      printf("\n");
      rmsg_iter->dump(__FILE__ , __LINE__);
      */
      helper_handle_gcache_request_hit(rmsg_iter);
      // If rmsg_iter is at end(), break.  Otherwise, ++ and continue.
      if (rmsg_iter == GlobalNetwork->RequestBuffers[myQueueNum][VCN_DEFAULT].end()) { break; }
      else { ++rmsg_iter; continue; }
    }
    

    // SECOND: Not valid. Is it pending?  If it is, wait until valid.  This
    // covers requests that are sitting in the memory controller as well since
    // any request at the memory controller must have an MSHR from the GC
    // associated with it.
    bool is_pending = true; //set to false if we find differently
    if(dir_req)
    {
      //Only one address to deal with here
      if(IsPending(lookup_addr) == NULL) { is_pending = false; }
    }
    else //for non-directory requests, continue on if *any* of its lines still need to be pended
    {
      //Early out if we have pended this request already.
      //FIXME Commented out the below early-out because we apparently cannot guarantee
      //that a message will be able to consume the data once it's back.  We do have access
      //bits that lock the line into the G$ until it is touched at least once, but my intuition
      //is that another request to the same line could come along and touch the data while we aren't
      //looking, allowing it to be evicted before the message that originally pend()ed the request
      //for the data can access the data from the G$.
      //In a real chip, you would want to fix this so that each message can have a small state machine
      //that doesn't have to keep rechecking to see if its addresses are pending, valid, etc.
      //For now, this works well enough.
      //NOTE: When this is fixed, remove the set_gcache_pended() call just below here.
      //if(rmsg_iter->check_gcache_pended()) { ++rmsg_iter; continue; }
      for (vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                          end = rmsg_iter->get_sms_end();
           it != end; ++it)
      {
        if(it->isWaitingToPend() && !IsPending(it->getAddr())) {
          is_pending = false;
          break;
        }
      }
    }
    //If is_pending is true here, another request is pending for these addresses.
    //We shouldn't set the bit in the message unless we pend them ourselves.
    if (is_pending) {
      //In flagrant violation of the advice given above (shouldn't set unless we pend them ourselves)
      //I am setting the gcache_pended bit here so that we have a more accurate picture of RequestBuffer
      //sizing.  NOTE: When we fix the FIFOcity problem outlined above, take the following line out:
      rmsg_iter->set_gcache_pended();
      //Don't take this line out :P
      ++rmsg_iter; continue;
    }
    // WRITE ALLOCATE: If it is a write, we do not need to go to memory so just
    // fill now.  The fill may fail so this request had to try again.  Adds an
    // extra cycle to the access so it is not zero-cycle turnaround.
    icmsg_type_t ic_msg = dir_req ? dir_req_msg_type : rmsg_iter->get_type();
    switch (ic_msg)
    {
      // Has no request message so it never will get the access bit set.
      case IC_MSG_DIRECTORY_WRITE_REQ:
      // BUG 189: I am removing this assert.  I think it is okay.  I am not sure
      // why this was never reached in the past.
        if (Fill(lookup_addr, false /* Not HW PF */, false /* Not BulkPF */, true /*dirty*/, false)) {
          for(std::vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                              end = rmsg_iter->get_sms_end();
              it != end; ++it)
          {
            if(it->getAddr() == lookup_addr)
              it->wasValid();
          }
        }
        ++rmsg_iter;
        continue;
      // All the ones below have a request message associated with them so they
      // will wrap around and call helper_handle_gcache_request_hit() above
      // which will then set the accessed bit.
      case IC_MSG_WRITE_REQ:
      case IC_MSG_EVICT_REQ:
      case IC_MSG_GLOBAL_WRITE_REQ:
      case IC_MSG_LINE_WRITEBACK_REQ:
      {
        bool foundIt = false;
        if (Fill(lookup_addr, false /*Not HW PF*/, false /*Not BulkPF*/, true /*dirty*/, true)) {
          for(std::vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                               end = rmsg_iter->get_sms_end();
              it != end; ++it)
          {
            if(it->getAddr() == lookup_addr)
            {
              foundIt = true;
              it->wasValid();
            }
          }
          if(!foundIt) {
            fprintf(stderr,"Error: Did not find corresponding state machine for 0x%08x\n", lookup_addr);
            rmsg_iter->dump(__FILE__, __LINE__);
          }
        }
        ++rmsg_iter;
        continue;
      }
      default:
        break;
    }

    // THIRD: Not valid, not pending.  Miss that we have not serviced yet.
    // However, there are no GCache MSHRs available, so we try again later.
    if (mshr_full(false)) { //(false) means not an eviction
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == lookup_addr && __DEBUG_CYCLE <= rigel::CURR_CYCLE) {
        DEBUG_HEADER();
        fprintf(stderr, "NO MSHRS!");
        rmsg_iter->dump(__FILE__, __LINE__);
      }
      #endif
      ++rmsg_iter;
      continue;
    }

    // Restrict the number of requests into the GCache bank that we will allow.
    if (requests_this_cycle <= 0) {
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == lookup_addr && __DEBUG_CYCLE <= rigel::CURR_CYCLE) {
        DEBUG_HEADER();
        fprintf(stderr, "WANT TO PEND ADDR %08X AT G$ BUT RAN OUT OF PORTS\n", lookup_addr);
        rmsg_iter->dump(__FILE__, __LINE__);
      }
      #endif
      break;
    }

    requests_this_cycle--;
    profiler::stats[STATNAME_GCACHE_PORT].inc_histogram();

    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == rmsg_iter->get_addr() && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "TRYING A PEND!");
      rmsg_iter->dump(__FILE__, __LINE__);
    }
    #endif
    // No write operations should make it to this point.
    assert(ic_msg != IC_MSG_LINE_WRITEBACK_REQ);
    assert(ic_msg != IC_MSG_EVICT_REQ);
    assert(ic_msg != IC_MSG_GLOBAL_WRITE_REQ);
    assert(ic_msg != IC_MSG_WRITE_REQ);
    // FINAL: If it pass through all the checks and the requested line is not in
    // the GCache/Memory system somewhere, then we pend it which will trigger a
    // memory access.
    if (dir_req) {
      // These assertions make sure that any memory accesses on behalf of directory
      // are to the same slice of memory as the original address requested by the
      // program (mapping to the same G$ bank)
      assert(AddressMapping::GetController(rmsg_iter->get_addr()) ==
        AddressMapping::GetController(lookup_addr));
      assert(AddressMapping::GetGCacheBank(rmsg_iter->get_addr()) ==
        AddressMapping::GetGCacheBank(lookup_addr));
      
      if(!is_being_evicted(lookup_addr)) {
        // Handle directory requests slightly differently.  Should work the same
        // as prefetches/evictions in that the cluster cache stuff and core_id
        // fields are useless for pend() to work properly.
        CacheAccess_t ca_out(
          lookup_addr,                  // address
          -1,                           // core number (faked)
          rigel::POISON_TID,          // thread id (FIXME, what should this be?!)
          dir_req_msg_type,             // interconnect message type
          NULL // rmsg_iter->spawner            // instruction correlated to this access
        );
        pend(ca_out,    // cache access packet
             100000000, // latency
             -1,        // ccache_pending_index
             false      // is_eviction
        );
      }
    } else {
      std::set<uint32_t> addrsWaitingToPend;
      for(vector<GCacheRequestStateMachine>::iterator it = rmsg_iter->get_sms_begin(),
                                                            end = rmsg_iter->get_sms_end();
          it != end; ++it)
      {
        const uint32_t addr = it->getAddr();
        if(!IsValidLine(addr) && !IsPending(addr) && !is_being_evicted(addr) && it->isWaitingToPend())
        {
          addrsWaitingToPend.insert(addr);
          it->pend();
        }
      }
      CacheAccess_t ca_out(
        addrsWaitingToPend,            // addresses
        rmsg_iter->GetCoreID()+rmsg_iter->get_cluster()*(rigel::CORES_PER_CLUSTER+1),
        rmsg_iter->GetThreadID(),      // thread id
        rmsg_iter->get_type(),         // interconnect message type
        NULL // rmsg_iter->spawner     // instruction correlated to this access
      );

      pend( ca_out,                      // cache access packet
            100000000,                   // latency
            rmsg_iter->GetPendingIndex(),// ccache_pending_index
            false                        // is_eviction
      );

      rmsg_iter->set_gcache_pended();
      //For debugging purposes, record the cycle pend is set.
      //FIXME there may be some redundancy here
      rmsg_iter->set_pend_cycle();
    }

    // Handle GCache prefetch.  Do not insert a prefetch if it is valid in the
    // GCache already or there is one pending for the line.
    helper_handle_prefetches(rmsg_iter);
    rmsg_iter++;
  }
}

//Gather statistics at the end of the simulation.
void
GlobalCache::gather_end_of_simulation_stats() const
{
  cache_directory->gather_end_of_simulation_stats();
}
