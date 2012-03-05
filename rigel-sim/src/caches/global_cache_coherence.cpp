#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdlib.h>                     // for abort
#include <cstdio>                       // for fprintf, NULL, stderr
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <list>                         // for _List_iterator, list, etc
#include <new>                          // for bad_alloc
#include <set>                          // for set
#include <string>                       // for char_traits, string
#include <vector>                       // for vector
#include "memory/address_mapping.h"  // for AddressMapping
#include "caches/cache_base.h"  // for CacheAccess_t
#include "caches/global_cache.h"  // for GlobalCache, etc
#include "caches/global_cache_debug.h"  // for DEBUG_CC, etc
#include "caches/hybrid_directory.h"  // for HybridDirectory, etc
#include "define.h"         // for ::IC_MSG_CC_RD_RELEASE_REQ, etc
#include "directory.h"      // for CacheDirectory
#include "global_network.h"  // for GlobalNetworkBase
#include "icmsg.h"          // for ICMsg
#include "interconnect.h"
#include "overflow_directory.h"  // for dir_of_timing_t, etc
#include "profile/profile.h"        // for ProfileStat
#include "profile/profile_names.h"  // for ::STATNAME_GCACHE_PORT
#include "sim.h"

using namespace rigel;

// helper_handle_directory_access()
//
// Checks whether a message has permission to access the directory this cycle
// and if not, it pends a new request.
//
// RETURNS: true if the request can proceed to cache access, false otherwise.
bool
GlobalCache::helper_handle_directory_access(ICMsg &rmsg)
{
  using namespace directory;

  if (ENABLE_EXPERIMENTAL_DIRECTORY)
  {
    uint32_t addr = rmsg.get_addr();
    int cluster = rmsg.get_cluster();
    DEBUG_CC(addr, cluster, "[GCACHE] helper_handle_directory_access.");

    if (rmsg.get_type() == IC_MSG_BCAST_UPDATE_REQ ||
        rmsg.get_type() == IC_MSG_BCAST_INV_REQ)
    {
      // Skip over broadcasts, not part of the coherence protocol.
      // At this point, we must have coherence permission to the line.
      rmsg.gcache_dir_proxy_accesses_complete = true;
      return true;
    } else if (ICMsg::check_is_global_operation(rmsg.get_type()))
    {
      // Skip over global operations for coherence actions for now.
      // At this point, we must have coherence permission to the line.
      rmsg.gcache_dir_proxy_accesses_complete = true;
      return true;
    } else {

      // Allocate the timer struct if needed.
      if (NULL == rmsg.get_dir_timer()) {
        rmsg.gcache_dir_proxy_accesses_pending = true;
        try { rmsg.set_dir_timer(new dir_of_timing_t); }
        catch (std::bad_alloc & b) {
          DEBUG_HEADER()
          fprintf(stderr, "Caught bad allocation.\n");
					std::cerr << "Error from exception: " << b.what() << "\n";
          assert(0);
        }
      }

      // Hybrid coherence check.  Note that if we already hit in the directory,
      // we ignore the hybrid cohernece controller.
      if (hybrid_directory.hybrid_coherence_enabled(addr, *rmsg.get_dir_timer()))
      {
        rmsg.gcache_dir_proxy_accesses_complete = false;
        rmsg.set_incoherent();
        return true;
      }

      if (ICMsg::check_is_read(rmsg.get_type()))
      {
        DEBUG_CC_MSG(addr, cluster, "[READ] Message at GCache.", rmsg.get_type());

        // Check directory permission, if line is shared, allow request to
        // proceed to accessing the GCache.
        if (!cache_directory->check_read_permission(addr, cluster, *(rmsg.get_dir_timer())))
        {
          DEBUG_CC(addr, cluster, "[READ] Do not have read permission");
          // If we already have the request as a write permission, we can skip
          // getting read permission.
          if (!cache_directory->check_write_permission(addr, cluster, *(rmsg.get_dir_timer()))) {
            DEBUG_CC(addr, cluster, "[READ] Do not have write permission");
            // Request is pending for us, we can try again later.
            if (cache_directory->check_request_pending(addr)) {
              DEBUG_CC(addr, cluster, "[READ] Request is pending.");
              return false;
            }
            if (replies_this_cycle <= 0 || requests_this_cycle <= 0) {
              DEBUG_CC(addr, cluster, "[READ] Ports blocked: NOT Requesting read permission.");
              return false;
            }
            DEBUG_CC(addr, cluster, "[READ] Requesting read permission.");
            // When we request, we check to see if it will hit to update the counter.
            rmsg.set_directory_cache_hit(cache_directory->is_present(addr));
            // Request read permission from the directory.
            if (!(cache_directory->request_read_permission(addr, cluster, *(rmsg.get_dir_timer()))))
            {
            // In the case where you need to access WayPoint, you may have a
            // pending request going that will take multiple cycles.  Here we
            // block any new requests from proceeding while the WayPoint
            // controller services the current request.  This may be useful for
            // non-WP code too.
            if (ENABLE_OVERFLOW_DIRECTORY) { cache_directory->set_atomic_lock(addr, cluster); }
              DEBUG_CC(addr, cluster, "[READ] read now pending.");
              return false;
            }
          } else {
            // If we can do a WR->RD transition, must be a hit.
            rmsg.set_directory_cache_hit(true);
            // If we are getting access because we have write permission, we
            // downgrade to read permission to accelerate the senario: C0 writes
            // X, X gets evicted, directory still thinks C0 owns X in M state,
            // others now want it shared even though C0 does not plan to write
            // it again.
            cache_directory->write_to_read_perm_downgrade(addr, cluster, *(rmsg.get_dir_timer()));
          }
        }

        DEBUG_CC(addr, cluster, "[READ] Obtained read permission!");
      } else if (ICMsg::check_is_write(rmsg.get_type())) {
        DEBUG_CC(addr, cluster, "[WRITE] Message at GCache.");

        // Check if we already have the write permission
        if (!cache_directory->check_write_permission(addr, cluster, *(rmsg.get_dir_timer()))) {
          DEBUG_CC(addr, cluster, "[WRITE] Do not have write permission.");
          // Request is pending for us, we can try again later.
          if (cache_directory->check_request_pending(addr)) {
            DEBUG_CC(addr, cluster, "[WRITE] Request is pending.");
            return false;
          }
          // Request write permission from directory.
          if (replies_this_cycle <= 0 || requests_this_cycle <= 0) {
            DEBUG_CC(addr, cluster, "[WRITE] Ports blocked: NOT Requesting write permission.");
            return false;
          }
          DEBUG_CC(addr, cluster, "[WRITE] Requesting write.");
          // When we request, we check to see if it will hit to update the counter.
          rmsg.set_directory_cache_hit(cache_directory->is_present(addr));
          if (!cache_directory->request_write_permission(addr, cluster, *(rmsg.get_dir_timer()))) {
            // In the case where you need to access WayPoint, you may have a
            // pending request going that will take multiple cycles.  Here we
            // block any new requests from proceeding while the WayPoint
            // controller services the current request.  This may be useful for
            // non-WP code too.
            if (ENABLE_OVERFLOW_DIRECTORY) { cache_directory->set_atomic_lock(addr, cluster); }
            DEBUG_CC(addr, cluster, "[WRITE] Write now pending.");
            return false;
          }
        }
        DEBUG_CC(addr, cluster, "[WRITE] Obtained write permission!");
      } else {
        // We should never reach this point.  All requests should be reads,
        // writes, or handled separately above.
        DEBUG_HEADER();
        rmsg.dump(__FILE__, __LINE__);
        fprintf(stderr, "Unknown request type for directory.\n");
        abort();
      }
      // We were granted permission, must be a hit.
      rmsg.set_directory_cache_hit(true);
      // We have permission, lock line state until we return data to the requestor
      cache_directory->set_atomic_lock(addr, cluster);
      // At this point, we must have coherence permission to the line.
      return true;
    }
  } else  {
    // Set the proxy access bit to complete for non-directory cases too.
    rmsg.gcache_dir_proxy_accesses_complete = true;
  }
  return true;
}

// helper_handle_probe_responses()
//
// Handle probe responses coming back from the cores.  This function is
// responsible for removing the messages from the ProbeResponse VC buffer.
//
// RETURNS: true if a message is handled this cycle, false otherwise.
bool
GlobalCache::helper_handle_probe_responses( dir_of_timing_t * dir_timer)
{
  using namespace directory;

  using namespace rigel::cache;
  if (ENABLE_EXPERIMENTAL_DIRECTORY)
  {
    int queue_num = bankID/GCACHE_BANKS_PER_MC;

		std::list<ICMsg>::iterator rmsg_iter;
    for ( rmsg_iter = GlobalNetwork->RequestBuffers[queue_num][VCN_PROBE].begin();
          rmsg_iter != GlobalNetwork->RequestBuffers[queue_num][VCN_PROBE].end();
          rmsg_iter++)
    {
      using namespace rigel::cache;

      // Only worry about those requests which map to this bank
      if (AddressMapping::GetGCacheBank((*rmsg_iter).get_addr()) != bankID) continue;

      // First we need to handle probe replies and releases.
      uint32_t addr = rmsg_iter->get_addr();
      int cluster = rmsg_iter->get_cluster();

      DEBUG_CC(addr, cluster, "[ProbeResponse Filtering]");
      // TODO: Build in hybrid coherence checking.
      if (hybrid_directory.hybrid_coherence_enabled(addr, *dir_timer))
      {
         ICMsg cc_msg = *rmsg_iter;
         cc_msg.set_incoherent();
         cc_msg.set_ready_cycle(0);
         // Need to generate a return message (TODO FIXME!!! We should get rid of
         // read releases for incoherent accesses at the cluster cache)
         GlobalNetwork->ReplyBufferInject(queue_num, VCN_PROBE, cc_msg);
         rmsg_iter = GlobalNetwork->RequestBufferRemove(queue_num, VCN_PROBE, rmsg_iter);
         return true;
      }

      switch (rmsg_iter->get_type())
      {
        // For NAKs, we just ignore and let the WRITEBACK or EVICT that is
        // waiting deal with it.
        case IC_MSG_CC_WR_RELEASE_ACK:
        case IC_MSG_CC_WR2RD_DOWNGRADE_ACK:
          // For write releases, we need to fill the line into the global cache.
          if (!IsValidLine(addr)) { Fill(addr, false /* Not PREF */, false /*Not BulkPF*/, true /*dirty*/, false); }
          else { setDirtyLine(addr); }
        case IC_MSG_CC_WR2RD_DOWNGRADE_NAK:
        case IC_MSG_CC_WR_RELEASE_NAK:
        {
          DEBUG_CC_MSG(addr, cluster, "[WR PROBE ACK/NAK] Message at GCache.", rmsg_iter->get_type());
          cache_directory->release_write_ownership(addr, cluster, rmsg_iter->get_type(), *dir_timer);
          // Message serviced, no reply need be generated.
          rmsg_iter->set_ready_cycle(0);
          rmsg_iter = GlobalNetwork->RequestBufferRemove(queue_num, VCN_PROBE, rmsg_iter);
          return true;
        }
        // Since invalidates are currently silent, every request will get one
        // message from each cluster it sent a probe out to.
        case IC_MSG_CC_INVALIDATE_NAK:
        case IC_MSG_CC_RD_RELEASE_NAK:
        case IC_MSG_CC_RD_RELEASE_ACK:
        case IC_MSG_CC_INVALIDATE_ACK:
        {
          DEBUG_CC_MSG(addr, cluster, "[RD PROBE ACK/NAK] Message at GCache.", rmsg_iter->get_type());
          cache_directory->release_read_ownership(addr, cluster, rmsg_iter->get_type(), *dir_timer);
          // Message serviced, no reply need be generated.
          rmsg_iter = GlobalNetwork->RequestBufferRemove(queue_num, VCN_PROBE, rmsg_iter);
          return true;
        }
        // For shared responses, we want to fill into the G$ if we can to avoid
        // a DRAM accesses.
        case IC_MSG_SPLIT_BCAST_SHR_REPLY:
        case IC_MSG_SPLIT_BCAST_OWNED_REPLY:
          if (!IsValidLine(addr)) { 
            Fill(addr, false /* Not PREF */, false /* Not BulkPF */,
              (rmsg_iter->get_type() == IC_MSG_SPLIT_BCAST_OWNED_REPLY) /*dirty*/, false); 
          }
        case IC_MSG_SPLIT_BCAST_INV_REPLY:
        {
          // Iterate through skip list and release lines not skipped.
          for (int i = 0; i < CLUSTERS_PER_TILE; i++) {
            int cid = cluster+i; 
            if (0 == rmsg_iter->get_bcast_skip_list().count(cid)) {
              DEBUG_CC_MSG(addr, cluster, "[BCAST SPLIT ACK] Message at GCache.", 
                rmsg_iter->get_type());
              cache_directory->release_read_ownership(addr, cid, rmsg_iter->get_type(), 
                *dir_timer, rmsg_iter->get_bcast_resp_valid_list(cid));
            }
          }
          // Message serviced, no reply need be generated.
          rmsg_iter = GlobalNetwork->RequestBufferRemove(queue_num, VCN_PROBE, rmsg_iter);
          return true;
        }
        case IC_MSG_CC_BCAST_ACK:
        {
          // For shared responses, we want to fill into the G$ if we can to avoid
          // a DRAM accesses.
          if (!IsValidLine(addr)) { Fill(addr, false /* Not PREF */, false /*Not BulkPF*/, false /*dirty*/, false); }
          DEBUG_CC_MSG(addr, cluster, "[RD PROBE ACK/NAK] Message at GCache.", rmsg_iter->get_type());
          cache_directory->release_read_ownership(addr, cluster, rmsg_iter->get_type(), *dir_timer,
            rmsg_iter->get_bcast_resp_valid());
          // Message serviced, no reply need be generated.
          rmsg_iter = GlobalNetwork->RequestBufferRemove(queue_num, VCN_PROBE, rmsg_iter);
          return true;
        }
        // Read releases must be handled like probe responses since they must
        // move ahead of regular requests that might be stalled.
        case IC_MSG_CC_RD_RELEASE_REQ:
        {
          // Cluster is checking in a shared request
          DEBUG_CC(addr, cluster, "[READ RELEASE] Message at GCache.");
          cache_directory->release_read_ownership(addr, cluster, IC_MSG_CC_RD_RELEASE_REQ, *dir_timer);

          ICMsg cc_msg = *rmsg_iter;
          cc_msg.set_ready_cycle(0);
          GlobalNetwork->ReplyBufferInject(queue_num, VCN_PROBE, cc_msg);
          rmsg_iter = GlobalNetwork->RequestBufferRemove(queue_num, VCN_PROBE, rmsg_iter);
          return true;
        }
        // First check to see if the line is owned, if not we can take it now.
        case IC_MSG_EVICT_REQ:
        case IC_MSG_LINE_WRITEBACK_REQ:
        {
          // For write releases, we need to fill the line into the global cache.
          if (!IsValidLine(addr)) { Fill(addr, false /* Not PREF */, false /*Not BulkPF*/, true /*dirty*/, false); }
          // We have already handled these messages so it can just drop through.
          DEBUG_CC_MSG(addr, cluster, "[WRITEBACK] Releasing write ownership.", rmsg_iter->get_type());
          // For writebacks, we may be releasing write ownership of an owned line
          // or of one that is going to transition to someone else (preempting an
          // invalidate).  In the case where the probe happens before the
          // writeback is received by the G$, but after it leaves the C$, the G$
          // will get a NAK.
          cache_directory->release_write_ownership(addr, cluster, rmsg_iter->get_type(), *dir_timer);

          ICMsg cc_msg = *rmsg_iter;
          cc_msg.set_ready_cycle(0);
          GlobalNetwork->ReplyBufferInject(queue_num, VCN_PROBE, cc_msg);
          rmsg_iter = GlobalNetwork->RequestBufferRemove(queue_num, VCN_PROBE, rmsg_iter);
          return true;
        }
        // Done.
        default:
          break; // Do nothing, drop through.
      }
    }
  }
  // No messages handled.
  return false;
}

void
GlobalCache::helper_handle_directory_responses(int myQueueNum)
{
  if (ENABLE_EXPERIMENTAL_DIRECTORY)
  {
    // Check for a pending probe from the directory.  Give it priority over other
    // requests.
    ICMsg cc_probe_msg;
    while (cache_directory->get_next_pending_icmsg(cc_probe_msg))
    {
      // TODO: We should fix it so that ReplyBufferInject() is allowed to stall.
      // This is a dirty hack for now.
      if (GlobalNetwork->ReplyBuffers[myQueueNum][VCN_PROBE].size() >=
            rigel::interconnect::MAX_ENTRIES_GCACHE_PROBE_REQUEST_BUFFER) { 
        //rigel::GLOBAL_debug_sim.dump_all();
        //DEBUG_HEADER();
        //fprintf(stderr, "PROBE STUCK! addr: 0x%08x\n", cc_probe_msg.get_addr());
        //assert(0);
        // FIXME!
        break; 
      }
      DEBUG_CC(cc_probe_msg.get_addr(), cc_probe_msg.get_cluster(),
        "[PROBE] Inserting CC probe into network.");
      replies_this_cycle--;
      cc_probe_msg.set_ready_cycle(0);
      // Check to see if we can inject this cycle, otherwise break.
      if (IC_MSG_SPLIT_BCAST_INV_REQ == cc_probe_msg.get_type() || 
          IC_MSG_SPLIT_BCAST_SHR_REQ == cc_probe_msg.get_type()) 
      {
        if (!GlobalNetwork->BroadcastReplyInject(myQueueNum, VCN_PROBE, cc_probe_msg)) { break; }
      } else {
        if (!GlobalNetwork->ReplyBufferInject(myQueueNum, VCN_PROBE, cc_probe_msg)) { break; }
      }
      // Once we have safely injected, we will remove the request.
      cache_directory->remove_next_pending_icmsg();
      // If we are out of reply ports, stop servicing probes to be sent by the
      // directory.
      if (replies_this_cycle <= 0) break;
    }
  }

  // Deal with probe responses from the cores.  May use up a request port.
  if ((ENABLE_OVERFLOW_DIRECTORY) && probe_dir_timer->misses_pending() > 0)
  {
    // Grab the next address in the trace to sent to the cache model/memory
    // model.  Only delete it once it is a hit.
    dir_of_timing_entry_t ent = probe_dir_timer->next();
    uint32_t dir_req_addr = ent.addr;
    icmsg_type_t dir_req_msg_type = ent.type;

    // 1. [SKIP HIT]
    if (IsValidLine(dir_req_addr))  {
      probe_dir_timer->remove();
    } else {
      // 2. [CHECK PENDING]
      if (IsPending(dir_req_addr) == NULL && !is_being_evicted(dir_req_addr)) {
        // 3. [WRITE FILL]
        // FIXME Is this giving us free writes even if something can't be immediately evicted?
        // Why aren't we decrementing requests_this_cycle?
        if (IC_MSG_DIRECTORY_WRITE_REQ == dir_req_msg_type) {
          Fill(dir_req_addr, false /* Not PREF */, false /*Not BulkPF*/, true /* don't dirty */, false);
          probe_dir_timer->remove();
        } else if (!mshr_full(false) && requests_this_cycle > 0) {
          // 4. [READ PEND]
          requests_this_cycle--;
          profiler::stats[STATNAME_GCACHE_PORT].inc_histogram();

          CacheAccess_t ca_out(
            dir_req_addr,       // address
            -1,                 // core number (faked)
            rigel::POISON_TID,// thread id (FIXME, what should this be?!)
            dir_req_msg_type,   // interconnect message type
            rigel::NullInstr  // no instruction correlated to this access
          );

          pend(ca_out,   // cache_access packet
              100000000, // latency
              -1,        // ccache_pending_index
              false      // is_eviction
          );
        }
      }
    }
  } else {
    // If there are no pending miss requests from OF directory accesses, just do
    // probe response handler.
    if (helper_handle_probe_responses(probe_dir_timer)) { requests_this_cycle--; }
  }
}
