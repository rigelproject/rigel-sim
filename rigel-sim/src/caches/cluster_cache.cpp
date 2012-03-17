////////////////////////////////////////////////////////////////////////////////
// src/caches/cluster_cache.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the implementation of the L2D cache (cluster cache).
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for fprintf, stderr, printf, etc
#include <set>                          // for set, etc
#include "memory/address_mapping.h"  // for AddressMapping
#include "broadcast_manager.h"  // for BroadcastManager
#include "cache_model.h"    // for CacheModel
#include "cache_tags.h"     // for CacheTag
#include "caches/cache_base.h"  // for CacheAccess_t, etc
#include "caches/l1d.h"     // for L1DCache
#include "caches/l1i.h"     // for L1ICache
#include "caches/l2d.h"     // for L2Cache
#include "caches/l2i.h"     // for L2ICache
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for DEBUG_HEADER, etc
#include "util/dynamic_bitset.h"  // for DynamicBitset
#include "util/util.h"      // for ExitSim
#include "icmsg.h"          // for ICMsg
#include "interconnect.h"   // for TileInterconnectBase
#include "locality_tracker.h"  // for LocalityTracker
#include "mshr.h"           // for MissHandlingEntry
#include "profile/profile.h"        // for ProfileStat, Profile, etc
#include "profile/profile_names.h"
#include "seqnum.h"         // for seq_num_t
#include "sim.h"
#include "tlb.h"            // for TLBs
#include "memory/backing_store.h" // FIXME used for is_executable()

using namespace rigel;

#if 0
#define __DEBUG_ADDR 0x20455e80
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#define __DEBUG_CYCLE 0x7FFFFFFF
#endif

// Method Name: PerCycle()
//
// Called every cycle for each L2 cache.
void
L2Cache::PerCycle() {
  // Handle profiling.
  ccache_handle_profiling();
  // We need to deal with fills that are unable to allocate in the cluster
  // cache immediately.  This could lead to a deadlock whereby no eviction can
  // get out of the cache and a fill cannot proceed and thus no progress is
  // ever made.  We deal with this a little more elegantly in RTL/the
  // new cache model, but for now, we will leave this how it is. 
  ccache_handle_deferred_fills();
  // Check if there are messages on the network that we can return.  Do the fill
  // reply handling as needed
  ccache_handle_tile_net_replies();
  // And this boys and girls is where we puke the simulator because something is
  // royally screwed up. 
  ccache_wdt_expired();
  // Check for a pending memory barrier.  If there are no pending entries in
  // front of it for the core holding the memory barrier and the writeback count
  // is zero, the memory barrier can retire.
  ccache_handle_membar();
  // Iterate over all MSHRs starting at the head of the FIFO.  We need to check
  // all of them in simulation since some of them may have returned values
  // already and are being clocked.  We should possibly split this up into two
  // pieces to model HW better since we only need to look at the head entry to
  // determine what we can schedule while the clocking of other MSHRs can be
  // handled independently for all the other requests.
  ccache_handle_pending_misses();
}

// Method Name: ccache_handle_profiling() (HELPER)
//
// Deal with any profiling stuff that needs to be done at the top of the
// ccache::PerCycle() call.
void
L2Cache::ccache_handle_profiling()
{
  int i = validBits.findFirstSet();
  unsigned int outbound = 0, pending_outbound = 0;
  while(i != -1)
  {
    // Check invariants.
    //helper_check_pending_miss_invariants(i);
    // There is a valid entry in the queue.  It may or may not have been
    // serviced by the tile interconnect.
    outbound++;
    if (!pendingMiss[i].IsRequestAcked()) { pending_outbound++; }
    i = validBits.findNextSet(i);
  }
  profiler::stats[STATNAME_CCACHE_OUTBOUND_QUEUE].inc_mem_histogram(outbound, 1);
  profiler::stats[STATNAME_CCACHE_PENDING_OUTBOUND_QUEUE].inc_mem_histogram(pending_outbound, 1);
}

// Method Name: ccache_handle_deferred_fills() (HELPER)
//
// In PerCycle(), fill operations that were previously blocked get first
// priority.
void
L2Cache::ccache_handle_deferred_fills()
{
  int i = validBits.findFirstSet();
  while(i != -1)
  {
    MissHandlingEntry<rigel::cache::LINESIZE> &mshr = pendingMiss[i];
    if (mshr.GetFillBlocked())
    {
      //TODO Is this actually an adequate check?  There's an ICMsg::check_is_read(), but it doesn't
      //appear to be complete either.
      bool rnw = (IC_MSG_WRITE_REQ != mshr.GetICMsgType());
      uint32_t fill_addr = mshr.get_fill_addr();

      bool MSHRSquashed = false;
      
      if (Fill(fill_addr, mshr.was_ccache_hwprefetch(), rnw, mshr.get_incoherent(), MSHRSquashed)) {
        // Do ICMsg type-specific operations.  This should be the same as what
        // happens just before a ClusterRemove on a successful Fill().
        if(!MSHRSquashed)
        {
          mshr.notify_fill(fill_addr);
          switch (mshr.GetICMsgType())
          {
            case IC_MSG_WRITE_REPLY: { setDirtyLine(fill_addr); break; }
            // When we remove the message, reset the logical channel and continue.
            case IC_MSG_CCACHE_HWPREFETCH_REPLY:
              dec_hwprefetch_count(); /* Fall through */
            case IC_MSG_READ_REPLY:
            {
              int gcache_bank =  AddressMapping::GetGCacheBank(fill_addr);
              seq_num[gcache_bank].ccache_put(mshr.get_seq_channel());
              break;
            }
            default: break;
          }
          // On successful remove, clear WDT
          //FIXME should we reset this if we have filled one line in a bulk request
          //and there are others left?
          network_stall_WDT = 0;
          // The Fill has completed.
          mshr.ClearFillBlocked();
          // Remove the MSHR from the pending request list if there are no other lines outstanding.
          if(mshr.IsDone())
          {
            pendingMissFifoRemove(mshr.GetPendingIndex());
	    mshr.clear(); //NEW
          }
        }
      }
    }
    //Get next valid MSHR
    i = validBits.findNextSet(i);
  }
}

// Method Name: ccache_handle_tile_net_replies() (HELPER)
//
// Remove replies coming from the global cache.
void
L2Cache::ccache_handle_tile_net_replies()
{
  using namespace rigel;
  bool rnw;

  MissHandlingEntry<rigel::cache::LINESIZE> mshr;
  while (TileInterconnect->ClusterRemovePeek(cluster_num, mshr))
  {
    // DEBUG: Check to see if the message has not been consumed in a long
    // while.  If so, puke the simulator and dump the pending information.
    if (!network_wdt_check(mshr)) assert(0 && "CCache network WDT failure.");
    // Check for a valid MSHR coming out of the network.  Note that CoreID may
    // be one past the legal number since that is how WRITEBACKS are checked.
    // Let RD Release fail coreid check.
    if ( !(IC_MSG_CC_RD_RELEASE_REPLY == mshr.GetICMsgType()) &&
        !ICMsg::check_is_coherence(mshr.GetICMsgType()) &&
        !( (mshr.GetCoreID() >= 0) &&
           (mshr.GetCoreID() <= CORES_PER_CLUSTER)  &&
           (mshr.GetThreadID() >=0 || mshr.GetThreadID()==rigel::POISON_TID) &&
           (mshr.GetThreadID() <= THREADS_PER_CLUSTER  || mshr.GetThreadID() == POISON_TID) &&
           (mshr.GetPendingIndex() < MSHR_COUNT) &&
           (mshr.GetPendingIndex() >= 0)
         )
       )
    {
      DEBUG_HEADER();
      mshr.dump();
      assert((mshr.GetCoreID() >= 0) && "Invalid CoreID");
      assert((mshr.GetThreadID() >= 0 || mshr.GetThreadID() == POISON_TID) && "Invalid ThreadID");
      assert((mshr.GetCoreID() <= CORES_PER_CLUSTER) && "Core number out of range!");
      assert((mshr.GetThreadID() <= THREADS_PER_CLUSTER || mshr.GetThreadID() == POISON_TID ) 
        && "Thread number out of range!");
      assert((mshr.GetPendingIndex() < MSHR_COUNT) && "Invalid Pending Index!");
      assert((mshr.GetPendingIndex() >= 0) && "Invalid Pending Index");
      assert(0);
    }

    switch (mshr.GetICMsgType())
    {
      // TODO: We want to finish the splitting bcast stuff 
      case (IC_MSG_SPLIT_BCAST_INV_REQ):
      case (IC_MSG_SPLIT_BCAST_SHR_REQ):
      case (IC_MSG_CC_BCAST_PROBE):
      {
        assert(ENABLE_EXPERIMENTAL_DIRECTORY
          && "IC_MSG_CC should only occur when the directory is enabled!");
        if (helper_handle_ccbcast_probe(mshr)) { goto cluster_remove_success; }
        #ifdef DEBUG_CACHES
        DEBUG_HEADER();
        fprintf(stderr, "[L2D BCAST PROBE REQUEST IGNORED] ");
        mshr.dump();
        fprintf(stderr, "\n");
        #endif
        // The coherence probe is being ignored.  Try again later.
        return;
      }
      case (IC_MSG_CC_INVALIDATE_PROBE):
      case (IC_MSG_CC_WR_RELEASE_PROBE):
      case (IC_MSG_CC_RD_RELEASE_PROBE):
      case (IC_MSG_CC_WR2RD_DOWNGRADE_PROBE):
      {
        assert(ENABLE_EXPERIMENTAL_DIRECTORY
          && "IC_MSG_CC should only occur when the directory is enabled!");
        if (helper_handle_coherence_probe(mshr)) { goto cluster_remove_success; }
        #ifdef DEBUG_CACHES
        DEBUG_HEADER(); fprintf(stderr, "[L2D PROBE REQUEST IGNORED] ");
        mshr.dump(); fprintf(stderr, "\n");
        #endif
        // The coherence probe is being ignored.  Try again later.
        return;
      }
      // FIXME: Broadcasts are generated by one core and propagate out to the
      // GCache.  The GCache turns them around and the network generates
      // NOTIFY's at each step.  Every cluster gets a NOTIFY (BCAST is a full
      // multicast) even if it is the sender.  The sender also gets a REPLY so
      // that the core injecting the BCAST knowns when it completes (it gets
      // *two* replies, in essence).  We still need to deal with UPDATES
      // properly. 
      case (IC_MSG_BCAST_INV_NOTIFY):
      {
        invalidate_line(mshr.get_addr());
        Caches->broadcast_manager->ack(mshr.get_addr(),cluster_num);
        goto cluster_remove_success;
      }
      case (IC_MSG_BCAST_UPDATE_NOTIFY):
      {
        // Tell the broadcast manager about it
        Caches->broadcast_manager->ack(mshr.get_addr(),cluster_num);
        // Invalidate the L1 caches
        CacheAccess_t ca(
          mshr.get_addrs(),    // address
          -1,                 // core number (reset below)
          rigel::POISON_TID,// reset below
          mshr.GetICMsgType(),// should not apply here
          rigel::NullInstr  // no instruction correlated to this access
        );
        // assuming intent is to invalidate all of a cluster's L1D caches
        for (int i = 0; i < rigel::L1DS_PER_CLUSTER; i++) 
        {
          bool inv_stall = false;
          // invalidate() must internally choose tid or core based on cache mode
          // (per core or per thread L1D caches)
          // just pass iterator in as both and it needs to choose which to use,
          // both are not correct...
          ca.set_tid( i ); // ...iterate across all tids?...or...
          ca.set_core_id( i ); // ...iterate across all cores?
          //Caches->invalidate_line(i, mshr.get_addr(), inv_stall);
          Caches->invalidate_line(ca, inv_stall);
        }
        goto cluster_remove_success;
      }
      case (IC_MSG_GLOBAL_READ_REPLY):
      case (IC_MSG_GLOBAL_WRITE_REPLY):
      case (IC_MSG_ATOMINC_REPLY):
      case (IC_MSG_ATOMDEC_REPLY):
      case (IC_MSG_ATOMXCHG_REPLY):
      case (IC_MSG_ATOMCAS_REPLY):
      case (IC_MSG_ATOMADDU_REPLY):
      case (IC_MSG_ATOMMAX_REPLY):
      case (IC_MSG_ATOMMIN_REPLY):
      case (IC_MSG_ATOMOR_REPLY):
      case (IC_MSG_ATOMAND_REPLY):
      case (IC_MSG_ATOMXOR_REPLY):
      case (IC_MSG_BCAST_UPDATE_REPLY):
      case (IC_MSG_BCAST_INV_REPLY):
      {
        assert(mshr.GetCoreID() >= 0 && "Invalid core number in message!");
        assert(mshr.GetThreadID() >= 0 && "Invalid tid number in message!");

        // For not blocking atomics, we have no GlobalMemOp state to screw with.
        int idx = ConvPendingToReal(mshr.GetPendingIndex());
        if (!pendingMiss[idx].get_was_nonblocking_atomic()) {
          // ATOMIC operation completed, notifying core
          GlobalMemOpOutstanding[ mshr.GetThreadID() ] = false;
          GlobalMemOpCompleted[ mshr.GetThreadID() ] = true;
        }

        assert(pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].check_valid() &&
          "Requested index into list of pending address is invalid!");
        //FIXME Not sure if I can actually just clear this address out.
        pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].set_line_ready(mshr.get_addr(), rigel::CURR_CYCLE); //No delay
        pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].notify_fill(mshr.get_addr()); //FIXME Handle MSHRs with multiple lines correctly
        pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].clear();
        pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())]._core_id = -1;
        pendingMissFifoRemove(mshr.GetPendingIndex());
        goto cluster_remove_success;
      }
      // Instruction fetches that are issued when a non-unified L2 is used
      // never fill into the cluster cache and propogate directly to the L2I.
      // With a unified L2, the fetches are sent to the G$ as standard
      // IC_MSG_READ_REQ's anyway, so this should never be reached.
      // FIXME we should leave some info so we know where to find the MSHR that
      // is waiting for this.  We shouldn't have to search through every MSHR
      // in the cluster.
      // FIXME How many cycles should the second arg to set_line_ready() be?
      // This is the number of cycles it will take to transport the line from here to the L2,
      // NOT the L2 access latency as it was before.
      // FIXME We need a unified discipline for setting the line ready time, when the Fill()
      // call occurs, and when we can return the data to MSHRs pending on lower-level caches.
      // This depends on whether we can bypass the cache and forward the data directly to the
      // lower-level cache or whether we have to wait for the fill write, then do a read.
      case (IC_MSG_INSTR_READ_REPLY):
      {
        MissHandlingEntry<rigel::cache::LINESIZE> &ccMSHR = pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())];
        
        if (!rigel::cache::USE_L2I_CACHE_SEPARATE && !rigel::cache::USE_L2I_CACHE_UNIFIED)
        {
          for (int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
            int j = Caches->L1I[i].validBits.findFirstSet();
            while(j != -1)
            {
              for (std::set<uint32_t>::const_iterator it = mshr.get_pending_addrs_begin(),
                                                      end = mshr.get_pending_addrs_end();
                   it != end; ++it)
              {
                const uint32_t &addr = *it;
                if (Caches->L1I[i].pendingMiss[j].check_valid(addr)) {
                  Caches->L1I[i].pendingMiss[j].SetRequestAcked();
                  Caches->L1I[i].pendingMiss[j].set_line_ready(addr, rigel::CURR_CYCLE+1); //FIXME see above
                  //Take it out of the C$ MSHR
                  ccMSHR.remove_addr(addr);
                }
              }
              j = Caches->L1I[i].validBits.findNextSet(j);
            }
          }
        } else if(rigel::cache::USE_L2I_CACHE_SEPARATE) {
          int i = Caches->L2I.validBits.findFirstSet();
          while(i != -1)
          {
            for (std::set<uint32_t>::const_iterator it = mshr.get_pending_addrs_begin(),
              end = mshr.get_pending_addrs_end();
            it != end; ++it)
            {
              const uint32_t &addr = *it;
	      if (addr==0x3000) fprintf(stderr,"INSTR REPLY MSG RCVD\n");
              if (Caches->L2I.pendingMiss[i].check_valid(addr)) {
                Caches->L2I.pendingMiss[i].SetRequestAcked();
                Caches->L2I.pendingMiss[i].set_line_ready(addr, rigel::CURR_CYCLE+1); //FIXME see above
                //Take it out of the C$ MSHR
                ccMSHR.remove_addr(addr);
              }
            }
            i = Caches->L2I.validBits.findNextSet(i);
          }
        }

        assert(mshr.GetCoreID() >= 0 && "Invalid core number in message!");
        assert(mshr.GetThreadID() >= 0 && "Invalid thread number in message!");
        assert(pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].check_valid() &&
          "Requested index into list of pending address is invalid!");
        //Get rid of the C$ MSHR if all addresses were returned in this message.
        if(ccMSHR.IsDone())
        {
          pendingMissFifoRemove(mshr.GetPendingIndex());
          //OLD pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].clear();
          //OLD pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())]._core_id = -1;
          ccMSHR.clear();
          ccMSHR._core_id = -1;
        }
        goto cluster_remove_success;
      }
      case (IC_MSG_LINE_WRITEBACK_REPLY):
      {
        // There is no pending MSHR to handle here.  We can just remove the
        // message and be done with it
        assert(mshr.GetThreadID() >= 0 && "Invalid thread number in message!");
        WritebacksPending[mshr.GetThreadID()]--;
        // If we are counting writebacks, the MSHR that was allocated to it
        // was cleared up after the message was taken by the network.
        assert(WritebacksPending[mshr.GetThreadID()] >= 0 && "Removed one too many WBs!");
        if (rigel::cache::ENABLE_WRITEBACK_COUNTING) goto cluster_remove_success;
        // Otherwise, we treat it like a global operation.
      }
      case IC_MSG_EVICT_REPLY:
      case IC_MSG_CC_RD_RELEASE_REPLY:
      {
        int idx = ConvPendingToReal(mshr.GetPendingIndex());
        // We just deallocate the MSHR on an eviction and move along.
         if (IC_MSG_CC_RD_RELEASE_REPLY == mshr.GetICMsgType() &&
            pendingMiss[idx].get_probe_poison_bit() )
        {
          // We had a release request in the network when a BCAST probe came
          // in.  We need to send the BCAST ACK now so that the directory
          // will decrement the count of sharers.
          pendingMiss[idx].set_line_ready(mshr.get_addr(), rigel::CURR_CYCLE);
          pendingMiss[idx].notify_fill(mshr.get_addr());
          // This will error out if we ever have an MSHR with >1 address
          // pending. I think that is a good thing.
          pendingMiss[idx].clear(); 
          pendingMiss[idx].set(mshr.get_addr(), 0, true, 
            ENABLE_BCAST_NETWORK ? IC_MSG_SPLIT_BCAST_INV_REPLY : IC_MSG_CC_BCAST_ACK, 
            -2, mshr.GetPendingIndex(), POISON_TID);
          pendingMiss[idx].clear_probe_poison_bit();
        } else if (ENABLE_EXPERIMENTAL_DIRECTORY && pendingMiss[idx].read_to_write_upgrade) {
          // Before we can issue the write, we have to make sure that there is
          // not a pending coherence message.  To test for it, we clear
          // ourselves, check for IsPending, then either allow the write to go
          // through or drop it on the floor.  Dropping the Wr side of a Rd->Wr
          // upgrade could lead to live lock though :-/ 

          // We need to remove the MSHR that we are now invalidating.  A new pend
          // may occur in the next cycle.  We are servicing a write request that
          // had to be sent back to directory before we could move from the S
          // state to the M state.
          int core_id = pendingMiss[idx]._core_id;
          pendingMiss[idx].clear(false); //Don't make sure it's empty
          if (NULL != IsPending(mshr.get_addr())) {
            // There is another message pending now beside the Rd->Wr upgrade.
            // Drop the upgrade request and wait for it to reissue.
            pendingMiss[idx]._core_id = -1;
            pendingMissFifoRemove(mshr.GetPendingIndex());
          } else {
            // Re-enable the request and issue the write.
            pendingMiss[idx].set( mshr.get_addr(), 0, true, IC_MSG_WRITE_REQ,
              core_id, mshr.GetPendingIndex(), POISON_TID );
          }
        } else { //Not a poisoned read-release, not a R->W upgrade
          //FIXME Can't do bulk prefetches this way, we need to know when the MSHR is actually
          //done (out of addresses)
          pendingMiss[idx].set_line_ready(mshr.get_addr(), rigel::CURR_CYCLE);
          pendingMiss[idx].notify_fill(mshr.get_addr());
          pendingMiss[idx].clear();
          pendingMiss[idx]._core_id = -1;
          pendingMissFifoRemove(mshr.GetPendingIndex());
        }
        goto cluster_remove_success;
      }
      default: 
      {
        break;
      }
    }
    // If we cannot fill this cycle, stop removing messages and try again next time.
    if (!ICMsg::check_can_ccache_fill(mshr.GetICMsgType())) {
      rigel::GLOBAL_debug_sim.dump_all();
      DEBUG_HEADER();
      assert(0 && "Should not be trying to fill a no-fill message!");
    }
    // Check if this should dirty the line on a fill.
    rnw = (IC_MSG_WRITE_REPLY != mshr.GetICMsgType());
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[DEFAULT FILL] ATTEMPT: cluster: %d addr: %08x IsValid: %1d\n",
          cluster_num, mshr.get_addr(), IsValidLine(mshr.get_addr()));
    }
    #endif

    //Scope this block to prevent compile errors screaming about jumps to labels crossing the
    //declaration of MSHRSquashed.
    {
      bool MSHRSquashed = false;
      if (false == Fill(mshr.get_addr(), mshr.was_ccache_hwprefetch(), rnw, mshr.get_incoherent(), MSHRSquashed))
      {
        if(!MSHRSquashed)
        {
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr, "[FILL FAILED] Set Fill Blocked: cluster: %d addr: %08x IsValid: %1d\n",
                cluster_num, mshr.get_addr(), IsValidLine(mshr.get_addr()));
          }
          #endif
          // FIXME: This one is BIG.  The issue here is that eventually you will
          // need to evict something from the CCache to satisfy this fill.  If the
          // network is backed up, that fill will never happen.  We just cycle
          // around here forever.  We may want to come up with a snazzier way to
          // deal with this, but for now we just skip out if we cannot fill.
          MissHandlingEntry<LINESIZE> &pendingMSHR = pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())];
          pendingMSHR.SetFillBlocked();
          pendingMSHR.set_line_ready(mshr.get_addr(), rigel::CURR_CYCLE + 0); //Set it ready so we catch it in handle_deferred_fills()
          
        }
        //return; //Depends on how we want to do it.  We can assume the data is held in a buffer with the MSHR and remove the message,
        //or keep the message in the network until we can actually do the Fill().  For now, we do the former, since we already
        //assume such a buffer in other places, such as evictions.
      }
      else
      {
        //FIXME handle this case more cleanly
        //If the MSHR was NAKed, Fill() turns it into an eviction or read release.
        //In that case, we don't want to set_line_ready()
        //To make things worse, Fill() can actually kill the MSHR completely and
        //remove it from the pending FIFO out from under us.  We should have a better way of dealing with this.
        if(!MSHRSquashed)
        {
          MissHandlingEntry<LINESIZE> &pendingMSHR = pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())];
          pendingMSHR.set_line_ready(mshr.get_addr(), rigel::CURR_CYCLE + 0); //FIXME see above
          pendingMSHR.notify_fill(mshr.get_addr());
          if(pendingMSHR.IsDone())
          {
            pendingMissFifoRemove(pendingMSHR.GetPendingIndex());
            pendingMSHR.clear();
          }
        }
      }
    }
    // FIXME not sure if below two lines (existing comments) are true.
    // There is no else clause.  We let ccache_handle_pending_misses() clean up
    // the MSHR later in this cycle. 
  cluster_remove_success:
    // Do ICMsg type-specific operations.
    switch (mshr.GetICMsgType())
    { 
      case IC_MSG_WRITE_REPLY: { setDirtyLine(mshr.get_addr()); break; }
      // When we remove the message, reset the logical channel and continue.
      case IC_MSG_CCACHE_HWPREFETCH_REPLY:  
        dec_hwprefetch_count(); /* Fall through */
      case IC_MSG_READ_REPLY:
      case IC_MSG_PREFETCH_BLOCK_CC_REPLY:
      {
        int gcache_bank =  AddressMapping::GetGCacheBank(mshr.get_addr());
        seq_num[gcache_bank].ccache_put(mshr.get_seq_channel());
        break;
      }
      default: break;
    }
    // On successful remove, clear WDT
    network_stall_WDT = 0;
    
    // Do accounting of received message:
    Profile::handle_network_stats(mshr.get_addr(), mshr.GetICMsgType());
    // Actual removal of a message from the network.  The line is now in the
    // cache. NOTE: This is the only place that message will leave the
    // network.
    //FIXME Pretty inefficient to have to find the message back instead of passing a
    //pointer to it or something like that.  We already know where it is from the
    //ClusterRemovePeek() call.
    TileInterconnect->ClusterRemove(cluster_num, mshr);
    // We can only allow one request per cycle to enter the CCache otherwise we
    // lose a point of serialization that screws up coherence amongst other
    // things.  Bug 196 discusses this issue. 
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: ccache_handle_membar() (HELPER)
//
// Handle a pending memory barrier if necessary.
////////////////////////////////////////////////////////////////////////////////
void
L2Cache::ccache_handle_membar()
{

  //for (int core_idx = 0; core_idx < rigel::CORES_PER_CLUSTER; core_idx++)
  for (int tidx = 0; tidx < rigel::THREADS_PER_CLUSTER; tidx++)
  {
    // There is no memory barrier pending (common case)
    if ( !MemoryBarrierPending[tidx] ) {
      continue;
    }

    // Check if we still have pending writebacks
    if (rigel::cache::ENABLE_WRITEBACK_COUNTING) {
      if (WritebacksPending[tidx] > 0) {
        continue;
      }
    }

    int pend_idx;
    bool pending_req = false;
    int count = 0;

    while (-1 != (pend_idx = pendingMissFifoGetEntry(count++))) {
      MissHandlingEntry<LINESIZE> &mshr = pendingMiss[ConvPendingToReal(pend_idx)];

      // Check if the MSHR is valid for this core
      //if (mshr.GetCoreID() == core_idx && mshr.check_valid()) {
      if (mshr.GetThreadID() == tidx && mshr.check_valid()) {
        pending_req = true;
        break;
      }
    }

    // Found a valid, pending MSHR for the core...try back later
    if (pending_req) {
      continue;
    }

    // If we get to this point, the core has a pending memory barrier while
    // there are no pending writebacks for the core and there are no outstanding
    // memory requests for the core.
    assert(GetMemoryBarrierPending(tidx)
      && "There must be a memory barrier pending at this point!");
    assert(!GetMemoryBarrierCompleted(tidx)
      && "Cannot have two overlapping completions!");

    ClearMemoryBarrierPending(tidx);
    // This must be reset by the core!
    SetMemoryBarrierCompleted(tidx);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ccache_handle_pending_misses()
////////////////////////////////////////////////////////////////////////////////
void
L2Cache::ccache_handle_pending_misses()
{
  int pend_idx = 0;
  int count = 0;
  while (-1 != (pend_idx = pendingMissFifoGetEntry(count++)))
  {
    using namespace rigel::cache;
    int real_idx = ConvPendingToReal(pend_idx);
    MissHandlingEntry<LINESIZE> &mshr = pendingMiss[real_idx];

    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[L2D ClusterInject ATTEMPT] (): addr: 0x%08x cluster: %d "
                      "msg_type: %d core: %d tid: %d \n",
                      mshr.get_addr(), cluster_num, mshr.GetICMsgType(),
                      mshr.GetCoreID() , mshr.GetThreadID() );
      mshr.dump();
    }
    #endif

    // If invalid, nothing to do
    if (!mshr.check_valid()) { continue; }

    // Check if this is out of the network, but was unable to fill for some
    // reason.  Fill() is called again next cycle for the request.
    if (mshr.GetFillBlocked()) { continue; }
    // If not scheduled, skip.  Network messages that fill in this cycle are
    // return in the next cycle since Schedule() from the lower level will find
    // the address valid in the cache.
    if (mshr.get_addrs().size() > 0)
    {
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[MSHR PENDING] cluster: %d addr: %08x IsValid: %1d\n",
                cluster_num, mshr.get_addr(), IsValidLine(mshr.get_addr()));
      }
      #endif
    }
    if (!mshr.has_ready_line())
    {
      if(!mshr.IsRequestAcked())
      {
        int seq_channel;
        int gcache_bank = AddressMapping::GetGCacheBank(mshr.get_addr());
        if (!helper_acquire_seq_channel(gcache_bank, seq_channel)) { continue; }

        // FIXME: Right now memory barriers may not be working properly.  I
        // think I set it up so that they are handled elsewhere so the two
        // commented-out lines below may be deprecated.  The message is: Do
        // not attempt to inject if there is a memory barrier pending. 
        if (TileInterconnect->ClusterInject(
              cluster_num, mshr, mshr.GetICMsgType(),
              seq_num[gcache_bank], seq_channel) )
        {
          // Do accounting of recieved message:
          Profile::handle_network_stats(mshr.get_addr(), mshr.GetICMsgType());
          // We succeeded!  Time to set the ssequence number for the channel.
          mshr.set_seq(seq_channel, seq_num[gcache_bank]);
          rigel::profiler::stats[STATNAME_TILE_INTERCONNECT_INJECTS].inc();
          // Message was consumed by the network
          mshr.SetRequestAcked();

          //Update last address for locality measurement
          if(rigel::ENABLE_LOCALITY_TRACKING)
          {
            if(!ICMsg::check_is_coherence(mshr.GetICMsgType()))
            {
              int core = mshr.GetCoreID();
              if(core < 0 || core > rigel::CORES_PER_CLUSTER)
              {
                printf("Cluster %d: Setting out of bounds core core number %d to %d for perCoreLocalityTracker, addr 0x%08x\n",
                       cluster_num, core, rigel::CORES_PER_CLUSTER, mshr.get_addr());
                       core = rigel::CORES_PER_CLUSTER;
              }
              for(std::set<uint32_t>::const_iterator it = mshr.get_pending_addrs_begin(),
                                                end = mshr.get_pending_addrs_end();
                  it != end; ++it)
              {
                const uint32_t &addr = *it;
                perCoreLocalityTracker[core]->addAccess(addr);
                aggregateLocalityTracker->addAccess(addr);
              }
            }
          }

          if(rigel::ENABLE_TLB_TRACKING)
          {
            if(!ICMsg::check_is_coherence(mshr.GetICMsgType()))
            {
              //Track global TLB behavior
              if (rigel::ENABLE_TLB_TRACKING)
              {
                for(size_t i = 0; i < num_tlbs; i++) {
                  for(std::set<uint32_t>::const_iterator it = mshr.get_pending_addrs_begin(),
                                                    end = mshr.get_pending_addrs_end();
                      it != end; ++it)
                  {
                    tlbs[i]->access(*it);
                  }
                }
              }
            }
          }

          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
            DEBUG_HEADER();
            fprintf(stderr, "L2 - ClusterInject(): addr: 0x%08x cluster: %d "
                            "msg_type: %d core: %d tid: %d [[SUCCESS]]\n",
                            mshr.get_addr(), cluster_num, mshr.GetICMsgType(),
                            mshr.GetCoreID(), mshr.GetThreadID() );
          }
          #endif

          switch(mshr.GetICMsgType())
          {
            case IC_MSG_LINE_WRITEBACK_REQ:
            {
              WritebacksPending[mshr.GetThreadID()]++;
              // Since the request is now in the network, we can remove the MSHR
              // allocated to it immediately.  When the ACK returns from the
              // GCache, we just decrement the count and do nothing with the MSHR
              if (rigel::cache::ENABLE_WRITEBACK_COUNTING) {
                assert(pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].check_valid() &&
                  "Requested index into list of pending address is invalid!");
                //OLD pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].clear();
                //OLD pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())]._core_id = -1;
                pendingMissFifoRemove(pend_idx);
                mshr.clear(); //NEW
                mshr._core_id = -1; //NEW
              }
              break;
            }
            // Coherence probe responses are not replied to by the global cache,
            // we simply release them once they are in the network.
            case IC_MSG_CC_BCAST_ACK:
            case IC_MSG_SPLIT_BCAST_INV_REPLY:
            case IC_MSG_SPLIT_BCAST_SHR_REPLY:
            case IC_MSG_SPLIT_BCAST_OWNED_REPLY:
              // We reserve some capacity in the MSHR list for bcasts.  We track
              // them here.
              outstanding_bcast_ack--;
            case IC_MSG_CC_WR2RD_DOWNGRADE_ACK:
            case IC_MSG_CC_WR2RD_DOWNGRADE_NAK:
            case IC_MSG_CC_WR_RELEASE_ACK:
            case IC_MSG_CC_WR_RELEASE_NAK:
            case IC_MSG_CC_RD_RELEASE_ACK:
            case IC_MSG_CC_RD_RELEASE_NAK:
            case IC_MSG_CC_INVALIDATE_ACK:
            case IC_MSG_CC_INVALIDATE_NAK:
            // We also release once G$ block prefetches (pref.b.gc) have been issued
            // into the network.
            case IC_MSG_PREFETCH_BLOCK_GC_REQ:
            {
              //OLD pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())].clear();
              //OLD pendingMiss[ConvPendingToReal(mshr.GetPendingIndex())]._core_id = -1;
              pendingMissFifoRemove(pend_idx);
              mshr.clear(false); //NEW, we don't need to make sure this one is empty
              mshr._core_id = -1; //NEW
              break;
            }
            // For limited directory, increment the sequence number on a read.
            case IC_MSG_READ_REQ:
            case IC_MSG_CCACHE_HWPREFETCH_REQ:
            {
              seq_num[gcache_bank].ccache_get(seq_channel);
              break;
            }
            default: { break; }
          }
        } // ELSE --> Inject Failure.
      }
      continue;
    }

    //FIXME not sure if this is right (used to be if !IsScheduled())
    if (!mshr.has_ready_line()) continue;

    icmsg_type_t msg_type = mshr.GetICMsgType();

    // Global operations do not fill into the CCache so we want to skip the
    // follow few blocks.  The global memory operations are removed from the
    // MSHRs at NetworkRemove() above.
    // Same goes for IC_MSG_INSTR_READ_REQ

    //FIXME not sure if this is required now that the readycycles timers are gone
    switch(msg_type) {
      case (IC_MSG_BCAST_UPDATE_REQ):
      case (IC_MSG_LINE_WRITEBACK_REQ):
        if (rigel::cache::ENABLE_WRITEBACK_COUNTING) {
          assert(0 && "Should never find a scheduled MSHR for"
                      " a writeback if WB counting is enabled.");
        }
      case (IC_MSG_ATOMXCHG_REQ):
      case (IC_MSG_ATOMCAS_REQ):
      case (IC_MSG_ATOMADDU_REQ):
      case (IC_MSG_ATOMMAX_REQ):
      case (IC_MSG_ATOMMIN_REQ):
      case (IC_MSG_ATOMOR_REQ):
      case (IC_MSG_ATOMAND_REQ):
      case (IC_MSG_ATOMXOR_REQ):
      case (IC_MSG_ATOMINC_REQ):
      case (IC_MSG_ATOMDEC_REQ):
      case (IC_MSG_GLOBAL_READ_REQ):
      case (IC_MSG_GLOBAL_WRITE_REQ):
      case (IC_MSG_BCAST_INV_REQ):
      case (IC_MSG_INSTR_READ_REQ):
        continue;
      default:
        /* Do nothing */
        break;
    }

    bool rnw = (IC_MSG_WRITE_REPLY != mshr.GetICMsgType());
    /* Check if fill can complete */
    uint32_t fill_addr = mshr.get_fill_addr();
    bool MSHRSquashed = false;
    if (false == Fill(fill_addr, mshr.was_ccache_hwprefetch(), rnw, mshr.get_incoherent(), MSHRSquashed))
    {
      if(!MSHRSquashed)
      {
        // Puts the MSHR back on the end of the queue
        pendingMissFifoRemove(pend_idx);
        int new_pend_idx = pendingMissFifoInsert(real_idx);
        mshr.pending_index = new_pend_idx;
        mshr.SetFillBlocked(); //FIXME this wasn't here before, but I think we want to handle this in
                               //handle_deferred_fills() next cycle, rather than keep putting it on
                               //the back of the FIFO.
        continue;
      }
    } else {
      if(!MSHRSquashed) {
        mshr.notify_fill(fill_addr);
        // Added for CC sanity checking 
        if (IC_MSG_WRITE_REPLY == mshr.GetICMsgType()) { setDirtyLine(fill_addr); }
        if(mshr.IsDone())
        {
          pendingMissFifoRemove(pend_idx);
          mshr.clear();
        }
      }
    }
  } /* end for() iterating over MSHRs */
}

////////////////////////////////////////////////////////////////////////////////
/// L2Cache::evict()
////////////////////////////////////////////////////////////////////////////////
/// Attempt to find a suitible way in the set matching addr to insert the line
/// trying to fill in to.  If there is an invalid entry, that one is used first.
/// If there are no invalid entries, the LRU line is simply replaced in the case
/// of a clean line, or in the case of a dirty line, the line is first written
/// back.
///
/// The return value is the way within the set that was evicted.  It is
/// only valid if false == stall.
///
/// PARAMETERS
///  uint32_t addr: Address of the line that will be inserted into the cache.
///  bool &stall: Set if the eviction did not occur.
int
L2Cache::evict(uint32_t addr, bool &stall) 
{
  int victim = 0;
  bool dirty_victim = false;
  int set = get_set(addr);
  uint64_t lru_cycle = TagArray[set][0].last_touch();
  uint64_t lru_time = 0;
  uint32_t victim_addr;
  int victim_start;
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
      dirty_victim = TagArray[set][i].get_victim_dirty();

      goto try_pend;
    }
  }
  // Always evict an invalid line first, return unstalled if one is found.
  for (int i = 0; i < numWays; i++) {
    if (!TagArray[set][i].valid()) {
      stall = false;
      return i;
    }
  }
  // Get the new LRU entry.
  for (int i = 1; i < numWays; i++) {
    if ((lru_time = TagArray[set][i].last_touch()) < lru_cycle) {
      lru_cycle = lru_time;
      victim = i;
    }
  }
  // Need to make sure we pick one that is not being filled currently otherwise
  // deadlock *will* happen, I know...
  victim_start = victim;
  while (TagArray[set][victim].locked()) {
    // I believe this is unreachable, code above tests for this and jumps past here
    victim = (victim + 1) % numWays;
    assert(victim != victim_start && "All entries are locked!");
  }

  // Handle Writeback if needed.  Right now we assume a write-through L1D, so no
  // writeback is necessary.
  victim_addr = TagArray[set][victim].get_tag_addr();
  dirty_victim = TagArray[set][victim].dirty(victim_addr, CACHE_ALL_WORD_MASK);

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (victim_addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER();
    fprintf(stderr, "[L2D evict()] (): victim_addr: 2x%08x cluster: %d ",
      victim_addr, cluster_num);
  }
  #endif

  // Invalidate in the lower level (Assumes write-through on L1D)
  Caches->EvictDown(victim_addr);

  // Invalidate the victim for the time being so that a subsequent access to the
  // line will be queued but this may be wrong, I dunno...
  TagArray[set][victim].invalidate();

try_pend:
  // If the line is incoherent, we need not do read releases.
  bool incoherent = TagArray[set][victim].get_incoherent();
  // If we are non-dirty and not using the directory, return.
  if (!dirty_victim && !ENABLE_EXPERIMENTAL_DIRECTORY) { stall = false; return victim; }
  // If we are non dirty and this is an incoherent access, return.
  if (!dirty_victim && incoherent)
  {
    // Line is no longer dirty but it is locked until fill happens from higher
    // level of the caceh
    TagArray[set][victim].clear_dirty(victim_addr, CACHE_ALL_WORD_MASK);
    TagArray[set][victim].invalidate();
    stall = false;
    return victim;
  }

  // Non-coherent RigelSim can drop invalidates on the floor.  Coherent
  // RigelSim must check in with the directory.
  // Since this is an evict, it should not have to stall on an MSHR (we keep entries
  // marked specifically for evictions).
  // If it does, we make use of is_being_evicted() to make sure subsequent accesses
  // to the evicted line don't grab an MSHR until after the eviction grabs one.
  // We used to check "mshr_full(true) || IsPending(victim_addr)", but IsPending()
  // was meant to prevent grabbing multiple MSHRs to evict the same line, but that should never
  // happen.
  if (mshr_full(true))
  {
      fprintf(stderr, "%08"PRIX64": mshr_full(): %01d IsPending(%08"PRIX32"): %01d, is_being_evicted(): %01d\n", rigel::CURR_CYCLE, mshr_full(true), victim_addr, (IsPending(victim_addr) != NULL), is_being_evicted(victim_addr));
      if(IsPending(victim_addr) != NULL) {
        fprintf(stderr, "DUMPING PENDING MSHR:\n");
        IsPending(victim_addr)->dump();
      }
      // Allocate the slot, but we have to wait until the upper level of the cache
      // takes it before we can allow the fill to proceed.
      stall = true;
      TagArray[set][victim].set_victim_addr(victim_addr);
      // Handle dirty bits.
      if (dirty_victim) TagArray[set][victim].set_victim_dirty();
      else              TagArray[set][victim].set_victim_clean();

      TagArray[set][victim].lock(addr);

      return 0;
  }
  
  TagArray[set][victim].unlock(); //Need to unlock the line once we get an MSHR so that 
                                  //subsequent accesses to this line can grab MSHRs.

  // used for both pends below
  CacheAccess_t ca(
    victim_addr,         // Address.
    CORES_PER_CLUSTER,   // There is no core number associated with the request.
    THREADS_PER_CLUSTER, // There is no thread ID associated with the request.
    IC_MSG_ERROR,        // * Message type set later.
    rigel::NullInstr   // No instruction correlated to this access.
  );
  // We need to make sure that we writeback the value, coherence or not, and
  // in the case of coherence being enabled, we need to do a read release.
  if (dirty_victim) {
    profiler::stats[STATNAME_DIR_L2D_DIRTY_EVICTS].inc();
    ca.set_icmsg_type( IC_MSG_EVICT_REQ );
    pend(ca, true /* is eviction! */ ) ;
  } else if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
    assert(!incoherent && "We should never try to pend a clean line with incoherence set\n");
    ca.set_icmsg_type( IC_MSG_CC_RD_RELEASE_REQ );
    pend(ca, true /* is eviction! */);
    profiler::stats[STATNAME_DIR_L2D_READ_RELEASES].inc();
  }
  // Line is no longer dirty but it is locked until fill happens from higher
  // level of the caceh
  TagArray[set][victim].clear_dirty(victim_addr, CACHE_ALL_WORD_MASK);
  TagArray[set][victim].invalidate();
  // Generate trace information for the cluster cache read misses
  if (rigel::GENERATE_CCTRACE) {
    fprintf(rigel::memory::cctrace_out, "%12lld, %4d, %8x, %c\n",
      (unsigned long long)rigel::CURR_CYCLE, cluster_num, addr, 'E');
  }

  // We found a victim and there is no stall.
  stall = false;
  return victim;
}

////////////////////////////////////////////////////////////////////////////////
// L2Cache::Fill()
////////////////////////////////////////////////////////////////////////////////
// Used to allocate an entry in the L2 cache
// INPUT : addr of data that needs to be added to cache
//         was_prefetch - Set if the Fill was triggered by a HW prefetch.
//         rnw - Set on read, cleared on write.
//         incoherent - Set on line that should not be treated like it is
//         coherent.  That is, no read releases and free read-to-write upgrades.
// OUTPUT: True if able to evict and add addr or addr already in cache
//        False if unable to evict and make room
//        squashed denotes whether or not the MSHR that corresponded to this address
//        was squashed during the fill (probably because of coherence actions).  If it
//        was, don't perform updates to the now-invalid MSHR like you might otherwise.
////////////////////////////////////////////////////////////////////////////////
bool
L2Cache::Fill(uint32_t addr, bool was_prefetch, bool rnw, bool incoherent, bool &squashed)
{
  squashed = false; //False by default, set to true further down if necessary.

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[FILL] ATTEMPT: cluster: %d addr: %08x IsValid: %1d\n",
        cluster_num, addr, IsValidLine(addr));
  }
  #endif

  if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY)
  {
    // If the poison bit is set from an earlier probe, the Fill simply gets
    // dropped on the floor.  This is pessimistic, but it avoids the race between
    // a probe and a Fill that was issued earlier by the directory but arrives
    // after the probe we already serviced.
    // FIXME it seems inefficient to have to find which MSHR caused this fill.
    //might want to change the Fill() interface or implement a smarter data structure.
    int i = validBits.findFirstSet();
    while(i != -1)
    {
      if (pendingMiss[i].check_valid(addr)) {
        // Check for NAKs since they do not fill.  The fill must be in response
        // to a different message.
        switch ( pendingMiss[i].GetICMsgType() ) {
          case IC_MSG_CC_RD_RELEASE_NAK:
          case IC_MSG_CC_WR_RELEASE_NAK:
          case IC_MSG_CC_INVALIDATE_NAK:
          case IC_MSG_CC_WR2RD_DOWNGRADE_NAK:
            i = validBits.findNextSet(i);
            continue;
          default: {}/* NADA */
        }

        if( pendingMiss[i].GetThreadID() < 0 ) {
          rigel_dump_backtrace();
          rigel::GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER(); pendingMiss[i].dump();
          fprintf(stderr,"Illegal tid %d in pendingMiss[%d] in L2Cache::Fill()\n", 
            pendingMiss[i].GetThreadID(), i);
          assert( 0 && "Illegal tid exists in mshr!" );
        }

        // We must allow it to be valid in the L1D of the requesting core for at
        // least one cycle to ensure forward progress.
        //int core_id = pendingMiss[i].GetCoreID();
        int L1CacheID = Caches->GetL1DID(pendingMiss[i].GetThreadID());
        //if (core_id >= 0 && core_id < rigel::CORES_PER_CLUSTER) {
        if (L1CacheID >= 0 && L1CacheID < rigel::L1DS_PER_CLUSTER) {
          //if (false == Caches->L1D[core_id].Fill(addr)) {
          if (false == Caches->L1D[ L1CacheID ].Fill(addr)) {
            // Could not fill this cycle.  Try again next cycle.
            return false;
          }
          Caches->L1D[ L1CacheID ].set_temporary_pending(addr);
        } else {
          // FIXME should we be allowed to reach here ever? we can! that means an
          // invalid thread ID got passed in...
          // Apparently many requests have their GetThreadID() set to some really large number (or -1 possibly)
        }
        // We should never have a coherence message in-flight for an
        // incoherent line.
        if (incoherent) {
          if (pendingMiss[i].get_probe_poison_bit() || pendingMiss[i].get_probe_nak_bit())
          {
            rigel::GLOBAL_debug_sim.dump_all();
            DEBUG_HEADER();
            fprintf(stderr, "[BUG]  We should not have a coherence message/poison bit set "
                            "for an incoherent line.\n");
            DEBUG_HEADER();
            pendingMiss[i].dump();
            assert(0);
          }
        }

        // Drop the request.
        if (pendingMiss[i].get_probe_poison_bit()) {
          switch(pendingMiss[i].GetICMsgType())
          {
            case IC_MSG_CC_RD_RELEASE_REQ:
            {
              // We had a release request in the network when a BCAST probe came
              // in.  We need to send the BCAST ACK now so that the directory
              // will decrement the count of sharers.
              int pendingIndex = pendingMiss[i].GetPendingIndex();
              pendingMiss[i].clear(false);
              pendingMiss[i].set(addr, 0, true, 
                ENABLE_BCAST_NETWORK ? IC_MSG_SPLIT_BCAST_INV_REPLY : IC_MSG_CC_BCAST_ACK, -2, 
                pendingIndex, rigel::POISON_TID  );
              pendingMiss[i].clear_probe_poison_bit();
              squashed = true; //TODO see note below on this
              return true;
            }
            case IC_MSG_CCACHE_HWPREFETCH_REQ:
              dec_hwprefetch_count();
            case IC_MSG_READ_REQ:
            {
              // We need to remove the MSHR that we are now invalidating.  A new pend
              // may occur in the next cycle.
              pendingMiss[i].clear(false);
              pendingMiss[i]._core_id = -1;
              pendingMissFifoRemove(pendingMiss[i].GetPendingIndex());
              // We have to invalidate the line to ensure RD->WR upgrades are
              // reissued.
              invalidate_line(addr);
              pendingMiss[i].clear_probe_poison_bit();
              squashed = true; //We squashed this one good.
              return true;
            }
            default:
            {
              // Infinite badness.  Only BCASTs from the limited directory scheme
              // should generate probe poisons and then only when the message type
              // is a read release or a read request.
              rigel::GLOBAL_debug_sim.dump_all();
              DEBUG_HEADER();
              pendingMiss[i].dump();
              assert(0 && "Unexpected reply type in Fill() with poison bit set.");
            }
          }
        } else if (pendingMiss[i].get_probe_nak_bit()) {
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr,
                "[FILL] NAK BIT SET: cluster: %d addr: %08x IsValid: %1d core: %4d tid: %d\n",
                cluster_num, addr, IsValidLine(addr), pendingMiss[i].GetCoreID(),
                pendingMiss[i].GetThreadID()
              );
          }
          #endif
          icmsg_type_t icmsg_return = IC_MSG_NULL;
          // NAK was already sent, we just got the actual response from the
          // directory, we need to now reply so that the directory can hand off
          // ownership (it is waiting for the NAK + the response)
          switch (pendingMiss[i].GetICMsgType())
          {
            case IC_MSG_CCACHE_HWPREFETCH_REQ:
            // No decrement here since the ClusterRemove() call deals with the
            // removal not Fill().
            case IC_MSG_READ_REQ:
            case IC_MSG_INSTR_READ_REQ:
              icmsg_return = IC_MSG_CC_RD_RELEASE_REQ;
              break;
            case IC_MSG_WRITE_REQ:
              icmsg_return = IC_MSG_EVICT_REQ;
              break;
            default:
              DEBUG_HEADER();
              fprintf(stderr, "Unhandled case.  Message type: %d\n", pendingMiss[i].GetICMsgType());
              assert(0);
          }

          // We need to remove the MSHR that we are now invalidating.  A new pend
          // may occur in the next cycle.
          int pendingIndex = pendingMiss[i].GetPendingIndex();
          pendingMiss[i].clear(false); //need to clear() before we can reset, MSHR need not be done.
          pendingMiss[i].set(addr, 0, true, icmsg_return, -2, pendingIndex, rigel::POISON_TID );
          squashed = true; //TODO Maybe we need a more nuanced version of 'squashed'ness.
                           //Here we didn't squash it, we just repurposed it, but you still probably
                           //Don't want to updates in the callee as though you're touching the old
                           //MSHR.

          // This is the only place the NAK bit is cleared (other than the MSHR
          // constructor)  This ensures that one and only one action is taken per NAK.
          pendingMiss[i].clear_probe_nak_bit();

          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr, "[FILL] FAILED on NAK: cluster: %d addr: %08x IsValid: %1d\n",
                cluster_num, addr, IsValidLine(addr));
              fprintf(stderr, "\n");
          }
          #endif

          return true;
        }
      }
      //Find next valid MSHR
      i = validBits.findNextSet(i);
    }
    
  }

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[FILL] PROVISIONAL SUCCESS cluster: %d addr: %08x IsValid: %1d rnw: %1d\n",
        cluster_num, addr, IsValidLine(addr), rnw);
      fprintf(stderr, "\n");
  }
  #endif

  // Already in the cache, the fill just cleans the rest of the line.
  if (IsValidLine(addr))
  {
    int set = get_set(addr);
    int way = get_way_line(addr);

    // We should not see an incoherent to coherent silent upgrade.
    if (incoherent != TagArray[set][way].get_incoherent()) {
      rigel::GLOBAL_debug_sim.dump_all();
      DEBUG_HEADER();
      fprintf(stderr, "[BUG] Mismatch between incoherence states: input: %2d\n", incoherent);
      assert(0);
    }

    TagArray[set][way].make_all_valid(addr);
    // We need to dirty the line if it is a write request so that the cache
    // model can disambiguate read-to-write upgrades from writes to already
    // dirty lines.  I am guarding it and only turning it on for coherent
    // RigelSim, but it can be generally applied methinks.
    if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
      if (!rnw) { TagArray[set][way].set_dirty(addr, 0xFFFF); }
    }

    return true;
  }

  // Find an empty line.  Eviction occurs if none are available
  bool stall;
  int set = get_set(addr);
  int way = evict(addr, stall);

  // Eviction failed this cycle, try back later.
  if (stall) {
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[FILL] Failed eviction: %d addr: %08x IsValid: %1d\n",
          cluster_num, addr, IsValidLine(addr));
        fprintf(stderr, "\n");
    }
    #endif

    return false;
  }

  // On a fill, all words are assumed to be valid from the next higher level of
  // the cache.
  uint32_t dirty_mask = 0;
  uint32_t valid_mask = CACHE_ALL_WORD_MASK;

  TagArray[set][way].insert(addr, dirty_mask, valid_mask);

  // Set the CacheTag to be an incoherent access.
  if (incoherent) 
  {
    TagArray[set][way].set_accessed_bit_read();
    TagArray[set][way].set_accessed_bit_write();
    TagArray[set][way].set_incoherent();
    if (was_prefetch) { TagArray[set][way].set_hwprefetch(); }
    // On writes, we dirty the cache.
    if (!rnw && ENABLE_EXPERIMENTAL_DIRECTORY) { TagArray[set][way].set_dirty(addr, 0xFFFF); }

    return true;
  }

  if (was_prefetch) {
    profiler::stats[STATNAME_CCACHE_HWPREFETCH_TOTAL].inc();
    TagArray[set][way].set_hwprefetch();
    // Prefetches can get pulled out before a core gets to access them since
    // they are speculative and there is no guarantee that a request will come
    // in.
    TagArray[set][way].set_accessed_bit_read();
    TagArray[set][way].set_accessed_bit_write();
  } else {
    // FIXME DIRTY HACK!!!  We need to make sure that speculative instructions
    // get their accessed bits set so they can be evicted.
		// We should fix this later so that the MSHR
    // knows that a fetch triggered the access and it can do the right thing on
    // a fill, i.e., set the bit if needed.
		// FIXME Needing to poke through to the cache model and backing store suggests
		// that maybe target memory metadata should live in a separate, globally
		// accessible object.
    if (Caches->backing_store.is_executable(addr)) { //if *addr is code
      TagArray[set][way].set_accessed_bit_read();
      TagArray[set][way].set_accessed_bit_write();
    } else {
      // Force an access before the line can be invalidated by a probe.
      if (rnw) {
        TagArray[set][way].clear_accessed_bit_read();
        TagArray[set][way].set_accessed_bit_write();
      } else {
        TagArray[set][way].clear_accessed_bit_write();
        TagArray[set][way].set_accessed_bit_read();
        // We need to dirty the line if it is a write request so that the cache
        // model can disambiguate read-to-write upgrades from writes to already
        // dirty lines.  I am guarding it and only turning it on for coherent
        // RigelSim, but it can be generally applied methinks. 
        if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY) { TagArray[set][way].set_dirty(addr, 0xFFFF); }
      }
    }
  }
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[FILL] SUCCESS: %d addr: %08x IsValid: %1d\n",
        cluster_num, addr, IsValidLine(addr));
      fprintf(stderr, "\n");
  }
  #endif
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// pend()
////////////////////////////////////////////////////////////////////////////////
// Attempts to allocate an MSHR for an outgoing request.  May also be called
// from evict() if it is punting dirty data.  Later on PerCycle() will try to
// shove the request into the network and eventually it will come back from the
// GCache and be removed.
void
L2Cache::pend(CacheAccess_t ca, bool is_eviction)
{
  if (IC_MSG_NULL == ca.get_icmsg_type() )
  {
    DEBUG_HEADER();
    fprintf(stderr, "Found a NULL message at L2 pend!\n");
    fprintf(stderr, "addr: %08x coreid: %d\n",
      ca.get_addr(), ca.get_core_id());
    assert(0);
  }

  MissHandlingEntry<rigel::cache::LINESIZE> *mshr;
  // If we do not mask, there will be multiple entries per cacheline...this is bad
  ca.set_addr( ca.get_addr() & rigel::cache::LINE_MASK );

  if (validBits.allSet()) {
    DEBUG_HEADER();
    fprintf(stderr, "Attempting to pend a request but there are no MSHRs available\n");
    fprintf(stderr, "addr: 0x%08x icmsg_type: %d cluster: %d core: %d is_eviction: %1d\n",
      ca.get_addr(), ca.get_icmsg_type(), cluster_num, ca.get_core_id(), is_eviction);
    assert(0);
  }

  if (ca.get_icmsg_type() == IC_MSG_CCACHE_HWPREFETCH_REQ || ca.get_icmsg_type() == IC_MSG_READ_REQ)
  {
    // Test to see if we ever try to pend something while a dirty victim is waiting:
    //TODO is the ca.get_addr() safe?
    int set = get_set( ca.get_addr() );
    for (int i = 0; i < numWays; i++) {
      if (TagArray[set][i].locked()) {
        if ( (rigel::cache::LINE_MASK & TagArray[set][i].victim_addr())
              == ca.get_addr() )
        {
          if (TagArray[set][i].get_victim_dirty()) {
            rigel::GLOBAL_debug_sim.dump_all();
            DEBUG_HEADER();
            fprintf(stderr,
              "FOUND DIRTY Victim while trying to do access. addr: 0x%08x\n",
              ca.get_addr() );
            assert(0);
          } else {
            DEBUG_HEADER();
            fprintf(stderr,
              "FOUND CLEAN victim while trying to do access. addr: 0x%08x\n",
              ca.get_addr() );
          }
        }
      }
    }
  }

  // Throttle hardware prefetches.
  if (ca.get_icmsg_type() == IC_MSG_CCACHE_HWPREFETCH_REQ) {
    // This is where a prefetch might get dropped on the floor.
    if (!hwprefetch_enabled()) {
      profiler::stats[STATNAME_DROPPED_HW_CCACHE_PREFETCHES].inc();
      return;
    }
    inc_hwprefetch_count();
  }

  // Generate trace information for the cluster cache read misses
  if (rigel::GENERATE_CCTRACE) {
    char miss_type = (ca.get_icmsg_type() == IC_MSG_WRITE_REQ) ? 'W' : 'R';
    // Should be more complete for types
    fprintf(rigel::memory::cctrace_out, "CCTRACE: %12"PRIu64", %4d, %8x, %c\n",
      rigel::CURR_CYCLE, cluster_num, ca.get_addr(), miss_type);
  }

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (ca.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr,
        "[L2D PEND] cluster: %d core: %d addr: %08x cPC: 0x%08x nPC 0x%08x\n",
        cluster_num, ca.get_core_id(), ca.get_addr(),
        ca.get_instr()->get_currPC(), ca.get_instr()->get_nextPC());
      fprintf(stderr, "\n");
  }
  #endif

  // Set this arbitrarily as 'high enough for the stack'.  Should really
  // do this at the core at write back to see whether the access was off $sp or
  // $fp regs.  FIXME:  Clean this up and use ProfStat
  // FIXME HACK We have >16MB of heap, so this will not work for most configs.
  if (ca.get_addr() > 0x00FFFFFF) { profiler->cache_stats.L2_stack_misses++; }

  // Find a free entry in pendingMiss[].  This should never fail.
  int free_real_idx;
  if(!PM_find_unused_mshr(free_real_idx))
  {
    DEBUG_HEADER();
    fprintf(stderr, "No MSHRs available in L2 pend()!!!\n");
    assert(0);
  }

  // Force in-order allocation
  mshr = &(pendingMiss[free_real_idx]);
  // When we insert, the entry is properly ordered but may start to slip over
  // time such that RealIndex != PendingIndex
  int pending_idx = pendingMissFifoInsert(free_real_idx);
  mshr->instr = ca.get_instr();
  if (ca.get_nonblocking_atomic()) {
    mshr->set_nonblocking_atomic();
  }
  
  // Auto-fill on writes disabled for coherence.  You suffer a write-miss when
  // coherence is disabled and have to wait.
  //FIXME It would be better if we didn't have to capture this case
  //both in this function and in Schedule() which calls it.
  //In this case, the L1D MSHR can be cleared because everything is taken care
  //of once it arbitrates for the L2 and performs the pend().
  if (!ENABLE_EXPERIMENTAL_DIRECTORY && ca.get_icmsg_type() == IC_MSG_WRITE_REQ) {
    mshr->set(
      ca.get_addrs(),
      L2D_ACCESS_LATENCY, false /* Var. Lat. */,
      ca.get_icmsg_type(),
      ca.get_core_id(),
      pending_idx,
      ca.get_tid()
    );
    // Fill immediately on writes.
    mshr->SetRequestAcked();
    mshr->clear_probe_poison_bit();
  } else {
    mshr->set(
      ca.get_addrs(),
      -1, true /* Var. Lat. */,
      ca.get_icmsg_type(),
      ca.get_core_id(),
      pending_idx,
      ca.get_tid()
    );
    mshr->clear_probe_poison_bit();
    // Handle broadcast ACK for lines valid in the L2 on BCAST request.
    ca.get_bcast_resp_valid() ? mshr->set_bcast_resp_valid() : mshr->clear_bcast_resp_valid();
 }
} // end L2Cache::pend()
