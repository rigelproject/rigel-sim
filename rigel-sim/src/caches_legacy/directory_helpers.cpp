
#include <assert.h>                     // for assert
#include <stddef.h>                     // for size_t, NULL
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for fprintf, stderr, FILE
#include <bitset>                       // for bitset
#include <queue>                        // for queue
#include <set>                          // for set, etc
#include <string>                       // for string
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for DEBUG_HEADER, etc
#include "directory.h"      // for dir_entry_t, CacheDirectory, etc
#include "util/dynamic_bitset.h"  // for DynamicBitset
#include "icmsg.h"          // for ICMsg
#include "profile/profile.h"        // for ProfileStat
#include "profile/profile_names.h"  // for ::STATNAME_DIR_SHARER_COUNT, etc
#include "sim.h"            // for stats, LINESIZE, CURR_CYCLE, etc
#include "util/util.h"           // for SimTimer
#if 0
#define __DEBUG_ADDR 0x04a1c800
#ifdef __DEBUG_CYCLE
#undef __DEBUG_CYCLE
#endif
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#define __DEBUG_CYCLE 0x7FFFFFFF
#endif

using namespace rigel;

bool 
CacheDirectory::helper_request_read_permission( uint32_t req_addr, 
                                                int req_clusterid, 
                                                directory::dir_entry_t &entry)
{
  using namespace directory;

  // Pending eviction for line; Ignore request.
  if (DIR_STATE_EVICTION_LOCK_WRITE == entry.get_state()) { return false; }
  if (DIR_STATE_EVICTION_LOCK_READ  == entry.get_state()) { return false; }
  if (DIR_STATE_EVICTION_LOCK_READ_BCAST  == entry.get_state()) { return false; }
  // These should never occur methinks? 
  // if (DIR_STATE_PENDING_READ_BCAST == entry.get_state()) { return false; }
  // if (DIR_STATE_PENDING_WRITE_BCAST == entry.get_state()) { return false; }

  // If there is a pending request, i.e., there is a transient hardware
  // state between a request and a reply and the L3/GCache has not serviced
  // it yet, we block the transition.
  if (entry.check_fill_pending()) { return false; }

  // Set touch time so WDT does not fail.
  entry.set_last_touch();

  // Check to see if we already have permission.  
  if (entry.check_cluster_is_sharing(req_clusterid)) 
  {
    rigel::GLOBAL_debug_sim.dump_all();
    DEBUG_HEADER();
    fprintf(stderr, "A cluster with permission should not generate a read request! ");
    fprintf(stderr, "addr: 0x%08x cluster: %3d\n", req_addr, req_clusterid);
    dump();
    assert(0);
  }

  // Number of sharer invalidations sent.

  switch (entry.get_state())
  {
    // If the line is already held as readable by another cluster,
    // add this cluster to the list of sharers.  
    case DIR_STATE_READ_SHARED:
    {
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == (req_addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) { 
        DEBUG_HEADER();
        fprintf(stderr, "[GRANT READ PERM] addr: %08x new sharer: %d\n", 
          req_addr, req_clusterid);
      }
      #endif
      entry.inc_sharers(req_clusterid);
      entry.set_fill_pending(req_clusterid);
      entry.set_last_touch();
      return true;
    }
    // If the line is held as writeable by another cluster, send out
    // invalidate request to holder.
    case DIR_STATE_WRITE_PRIV:
    {
      int curr_owner = entry.get_owner();

      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == (req_addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) { 
        DEBUG_HEADER();
        fprintf(stderr, "[PEND WRITE PERM] addr: %08x owner: %d new sharer: %d\n", 
          req_addr, curr_owner, req_clusterid);
      }
      #endif

      // First we have to lock the entry, all other requests will have to wait.
      entry.set_state(DIR_STATE_PENDING_WB);
      entry.set_waiting_cluster(req_clusterid);

      // Send out invalidate to the writer holding the line.
      profiler::stats[STATNAME_DIR_INVALIDATE_WR].inc();
      ICMsg wb_msg(IC_MSG_CC_WR_RELEASE_PROBE, entry.get_addr(), 
        std::bitset<rigel::cache::LINESIZE>(), curr_owner, -1, -1, -1, NULL);
      // TODO: Throttle how fast we can insert these messages?
      insert_pending_icmsg(wb_msg);

      // We do not have read permission yet.
      profiler::stats[STATNAME_DIR_SHARER_COUNT].inc_multi_histogram(1);
      return false;
    }
    // We should not find pending reads/writes here.  The check is above.
    case DIR_STATE_PENDING_READ:
    case DIR_STATE_PENDING_WRITE:
    case DIR_STATE_PENDING_WB:
    case DIR_STATE_EVICTION_LOCK_WRITE:
    case DIR_STATE_EVICTION_LOCK_READ:
    case DIR_STATE_PENDING_READ_BCAST:
    case DIR_STATE_PENDING_WRITE_BCAST:
    case DIR_STATE_EVICTION_LOCK_READ_BCAST:
    // Invalid entries should be skipped already.
    case DIR_STATE_INVALID:
    default:
    {
      rigel::GLOBAL_debug_sim.dump_all();
      DEBUG_HEADER();
      fprintf(stderr, "BUG! Message type: %s\n", dir_state_string[entry.get_state()]);
      assert(0);
    }
  }

  return false;
}

bool 
CacheDirectory::helper_request_write_permission( uint32_t req_addr, 
                                                 int req_clusterid, 
                                                 directory::dir_entry_t &entry)
{
  using namespace directory;

  // Pending eviction for line; Ignore request.
  if (DIR_STATE_EVICTION_LOCK_WRITE == entry.get_state()) { return false; }
  if (DIR_STATE_EVICTION_LOCK_READ  == entry.get_state()) { return false; }
  if (DIR_STATE_EVICTION_LOCK_READ_BCAST  == entry.get_state()) { return false; }
  // Missed case for limited directories.  May not be the right fix. 
  if (DIR_STATE_PENDING_WRITE_BCAST == entry.get_state()) { return false; }

  // If there is a pending request, i.e., there is a transient hardware
  // state between a request and a reply and the L3/GCache has not serviced
  // it yet, we block the transition.
  if (entry.check_fill_pending()) { return false; }

  // Set touch time so WDT does not fail.
  entry.set_last_touch();

  // Check to see if we already have permission.  Unlike the read shared case
  if (entry.check_cluster_is_sharing(req_clusterid) && DIR_STATE_WRITE_PRIV == entry.get_state()) 
  {
    rigel::GLOBAL_debug_sim.dump_all();
    DEBUG_HEADER();
    fprintf(stderr, "A cluster with write permission should not generate a write request!\n");
    dump();
    assert(0);
  }

  #ifdef DEBUG_DIRECTORY
  DEBUG_HEADER();      
  fprintf(stderr, "[WRITE] Found matching request\n");
  #endif


  switch (entry.get_state())
  {
    // If the line is already read shared, check to see if it is held by the
    // requester and only the requester and perform the upgrade.  Otherwise,
    // invalidate all other copies and take it as write-private.
    case DIR_STATE_READ_SHARED:
    {

      #ifdef DEBUG_DIRECTORY
      DEBUG_HEADER();
      fprintf(stderr, "[WRITE] Match is read shared by: %d.\n", entry.get_num_sharers());
      #endif
      
      // If the broadcast bit is set and the number of sharers is greater than
      // zero, we will have to broadcast.  One thing we might want to check for
      // is to see how many times this predicate fails because # sharer == 0,
      // which is an indication of how good software is about "checking in"
      // unused data.
      if (entry.check_bcast() && entry.get_num_sharers() > 0) 
      {
        // Add the entry to the list of outstanding bcasts
        add_outstanding_bcast(&entry);
        // Lock the entry
        entry.set_state(DIR_STATE_PENDING_WRITE_BCAST);
        // For probe filter directory, we need to setup the next PF state.  
        entry.set_pf_bcast_next(DIR_STATE_WRITE_PRIV, req_clusterid);
        // Requestor is not added to list, but all other sharers are.
        entry.set_sharers_bcast(req_clusterid);
        entry.set_waiting_cluster(req_clusterid);
        int num_sharer_invs = 0;
        // Send out broadcasts to all clusters.
        if (rigel::cache::ENABLE_BCAST_NETWORK)
        {
          // One message per broadcast.
          ICMsg wb_msg(IC_MSG_SPLIT_BCAST_INV_REQ, entry.get_addr(), 
            std::bitset<rigel::cache::LINESIZE>(), -358, -1, -1, -1, NULL);
          for (size_t cid = 0; cid < (size_t)rigel::NUM_CLUSTERS; cid++) {
            wb_msg.set_bcast_seq(cid, -1, seq_num[cid]);
            num_sharer_invs++;
          }
          // Do not send message to next sharer.
          wb_msg.add_bcast_skip_cluster(req_clusterid);
          insert_pending_icmsg(wb_msg);
        } else {
          for (size_t cid = 0; cid < (size_t)rigel::NUM_CLUSTERS; cid++) 
          {
            // Do not send message to requester.
            if ((size_t)req_clusterid == cid) { continue; }
            #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == (entry.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) { 
              DEBUG_HEADER();
              fprintf(stderr, "[PROBE] Sending BCAST_INV probe for addr: %08x to cluster: %d\n",
                entry.get_addr(), cid);
            }
            #endif
            // Handle splitting broadcasts separately.
            ICMsg wb_msg(IC_MSG_CC_BCAST_PROBE, entry.get_addr(), 
              std::bitset<rigel::cache::LINESIZE>(), cid, -1, -1, -1, NULL);
            // This has no concept of a logical channel, just the current time
            // steps.
            wb_msg.set_seq(-1, seq_num[cid]);
            num_sharer_invs++;
            // TODO: Throttle how fast we can insert these messages?
            insert_pending_icmsg(wb_msg);
          }
        }
        assert(num_sharer_invs <= rigel::NUM_CLUSTERS && "too many sharers!?");
        profiler::stats[STATNAME_DIR_SHARER_COUNT].inc_multi_histogram(num_sharer_invs);
        profiler::stats[STATNAME_DIR_INVALIDATE_BCAST].inc(num_sharer_invs);

        // We do not have read permission yet.
        return false;
      }
      // Non-broadcast.
      if (entry.check_cluster_is_sharing(req_clusterid) && 1 == entry.get_num_sharers())
      {
        entry.set_state(DIR_STATE_WRITE_PRIV);
        entry.set_fill_pending(req_clusterid);
        return true;
      }
      // For simplicity, remove cluster as a sharer so it does not get an
      // invalidate.  If it is not a read sharer, this is a NOP.
      entry.dec_sharers(req_clusterid);

      // First we have to lock the entry, all other requests will have to wait.
      entry.set_state(DIR_STATE_PENDING_WRITE);
      entry.set_waiting_cluster(req_clusterid);
      int num_sharer_invs = 0;
      // Send out invalidate to all readers holding the line.
      for (size_t cid = 0; cid < config.bitvec_size; cid++) 
      {
        // Do not generate message if the line is not held.
        if (!entry.check_cluster_is_sharing(cid)) { continue; }
        
        #ifdef __DEBUG_ADDR
        if (__DEBUG_ADDR == (entry.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) { 
          DEBUG_HEADER();
          fprintf(stderr, "[PROBE] Sending INV probe for addr: %08x to cluster: %d\n",
            entry.get_addr(), cid);
        }
        #endif

        profiler::stats[STATNAME_DIR_INVALIDATE_REG].inc();
        ICMsg wb_msg(IC_MSG_CC_INVALIDATE_PROBE, entry.get_addr(), 
          std::bitset<rigel::cache::LINESIZE>(), cid, -1, -1, -1, NULL);

        num_sharer_invs++;
        // TODO: Throttle how fast we can insert these messages?
        insert_pending_icmsg(wb_msg);
      }
      assert(num_sharer_invs <= rigel::NUM_CLUSTERS && "too many sharers!?");
      profiler::stats[STATNAME_DIR_SHARER_COUNT].inc_multi_histogram(num_sharer_invs);
      // We do not have read permission yet.
      return false;
    }
    // TODO BELOW!
    // If the line is held as writeable by another cluster, send out
    // invalidate request to holder.
    case DIR_STATE_WRITE_PRIV:
    {
      #ifdef DEBUG_DIRECTORY
      DEBUG_HEADER();
      fprintf(stderr, "[WRITE] Match is write private.\n");
      #endif

      int current_owner = entry.get_owner();
      // First we have to lock the entry, all other requests will have to wait.
      entry.set_state(DIR_STATE_PENDING_WRITE);
      // Cluster to give permission to when owner responds.
      entry.set_waiting_cluster(req_clusterid);
      // Send out invalidate to the writer holding the line.
      profiler::stats[STATNAME_DIR_INVALIDATE_WR].inc();
      // Update count of write-to-write transfers.
      profiler::stats[STATNAME_DIR_WR2WR_TXFR].inc();
      ICMsg wb_msg(IC_MSG_CC_WR_RELEASE_PROBE, entry.get_addr(), 
        std::bitset<rigel::cache::LINESIZE>(), current_owner, -1, -1, -1, NULL);
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[WRITE -> WRITE] owner: %3d req_clusterid: %3d addr: %08x "
                        " state: %s\n", 
          current_owner, req_clusterid, req_addr, dir_state_string[entry.get_state()]);
      }
      #endif
      // TODO: Throttle how fast we can insert these messages?
      insert_pending_icmsg(wb_msg);

      profiler::stats[STATNAME_DIR_SHARER_COUNT].inc_multi_histogram(1);
      // We do not have read permission yet.
      return false;
    }
    // We should not find pending reads/writes here.  The check is above.
    case DIR_STATE_PENDING_READ:
    case DIR_STATE_PENDING_WRITE:
    case DIR_STATE_PENDING_WB:
    case DIR_STATE_EVICTION_LOCK_WRITE:
    case DIR_STATE_EVICTION_LOCK_READ:
    case DIR_STATE_EVICTION_LOCK_READ_BCAST:
    // Invalid entries should be skipped already.
    case DIR_STATE_INVALID:
    default:
    {
      rigel::GLOBAL_debug_sim.dump_all();
      DEBUG_HEADER();
      fprintf(stderr, "[BUG] Write Release: addr: %08x cluster: %4d\n", req_addr, req_clusterid);
      fprintf(stderr, "[DIRECTORY BUG]\n"); entry.dump();
      assert(0);
    }
  }
}

void 
CacheDirectory::helper_release_read_ownership( uint32_t rel_addr, 
                                               int rel_clusterid, 
                                               icmsg_type_t ic_type, 
                                               directory::dir_entry_t &entry,
                                               bool bcast_found_valid_line)
{
  using namespace directory;
  using namespace rigel::cache;

  size_t set = get_set(rel_addr);

  if (ENABLE_PROBE_FILTER_DIRECTORY) 
  {
    if (entry.get_state() == DIR_STATE_EVICTION_LOCK_READ_BCAST ||
        entry.get_state() == DIR_STATE_PENDING_WRITE_BCAST)
    {
      // Handle BCAST ACK the same way as other release requests.
      switch (ic_type)
      {
        case IC_MSG_SPLIT_BCAST_SHR_REPLY:
        case IC_MSG_SPLIT_BCAST_OWNED_REPLY:
          // Track the bcast replies that found the line valid in the L2.
          if (bcast_found_valid_line) { entry.bcast_valid_sharers.insert(rel_clusterid); }
        case IC_MSG_CC_BCAST_ACK:
        case IC_MSG_SPLIT_BCAST_INV_REPLY:
        {
          entry.dec_sharers(rel_clusterid);
          if (0 == entry.get_num_sharers()) 
          {
            // Broadcast is done, remove it from the set
            remove_outstanding_bcast(&entry);
            
            dir_state_t state = entry.get_pf_bcast_next_state();
            int sharer = entry.get_pf_bcast_next_sharer();
            std::set<int> bcast_valid_sharers = entry.bcast_valid_sharers;

            // Added to make sure we clean up broadcasts before installing new entry
            entry.reset();
            entry.set_state(state);
            entry.inc_sharers(sharer);
            entry.set_addr(rel_addr);
            entry.set_fill_pending(sharer);
            entry.set_last_touch();
            // Only used when we want to allow rebuilding of sharer vector.
            if (ENABLE_BCAST_NETWORK && ENABLE_PROBE_FILTER_DIRECTORY && state == DIR_STATE_READ_SHARED)
            {
              // Add broadcast ack's that are sharing line to the list.
              for ( std::set<int>::iterator it = bcast_valid_sharers.begin(); 
                    it != bcast_valid_sharers.end(); it++) 
              { 
                assert(entry.get_state() == DIR_STATE_READ_SHARED);
                assert (sharer != *it && "The requester should not be currently sharing the line.");
                entry.inc_sharers(*it); 
              }
            }
          }
        }
        default: break;
      }
      return;
    } 
  }

  // Check to make sure the line is in a proper state for a read_release.
  switch (entry.get_state()) 
  {
    case DIR_STATE_PENDING_WRITE_BCAST:
    case DIR_STATE_EVICTION_LOCK_READ_BCAST:
    // FIXME?  We may need to reset this differently if this is an eviction
    // BCAST.
    {
      switch (ic_type) {
        // Handle BCAST ACK the same way as other release requests.
        case IC_MSG_CC_BCAST_ACK:
        case IC_MSG_SPLIT_BCAST_INV_REPLY:
          entry.dec_sharers(rel_clusterid);
          if (0 == entry.get_num_sharers()) {
            //Remove the bcast from the outstanding list
            remove_outstanding_bcast(&entry);
            entry.reset(); 
          }
        // We still ack the read release for broad casts, but we wait on the
        // BCAST ACK before decrementing the counter.
        case IC_MSG_CC_RD_RELEASE_REQ:
          break;
        default:
          rigel::GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER();
          fprintf(stderr, "Unhandled request: %d\n", ic_type);
          assert(0 && "Invalid request type!");
      }
      break;
    }
    // Simple release.  Nothing is waiting on the line.
    case DIR_STATE_READ_SHARED:
    {
      switch (ic_type) {
        case IC_MSG_CC_RD_RELEASE_ACK:
        case IC_MSG_CC_INVALIDATE_ACK:
        case IC_MSG_CC_RD_RELEASE_REQ:
          entry.dec_sharers(rel_clusterid);
          if (0 == entry.get_num_sharers()) {
            entry.reset();
          }
          break;
          #if 0
          // We need to wait for the NAK/REL pair before we decrement the
          // count.  This is only an issue in the evict state.
          if (entry.nak_waitlist.count(rel_clusterid)) {
            entry.dec_sharers(rel_clusterid);
            if (0 == entry.get_num_sharers()) { entry.reset(); }
          } else {
            entry.nak_waitlist.insert(rel_clusterid);
          }
          break;
          #endif
        default:
          rigel::GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER();
          fprintf(stderr, "Unhandled request: %d\n", ic_type);
          assert(0 && "Invalid request type!");
      }
      break;
    }
    // If this is locked for an eviction and the count drops to zero, it
    // becomes free and the waiter should now be able to take the entry back.
    case DIR_STATE_EVICTION_LOCK_READ:
    case DIR_STATE_EVICTION_LOCK_WRITE:
    // For a pending write, we check for all read requests and set it owned by
    // the waiter when the count drops to zero.
    case DIR_STATE_PENDING_WRITE:
    {
      // Waiting cluster is meaningless for EVICTION_LOCK
      int waiting_cluster = (entry.get_state() == DIR_STATE_PENDING_WRITE) ?
        entry.get_waiting_cluster() : -42;
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == rel_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[RELEASE] READ PENDING_WRITE waiting_cluster: %d "
                        "num_sharers: %d addr: %08x "
                        "msg: %2d nak_waitlist.count(): %2d state: %s\n", 
          waiting_cluster, entry.get_num_sharers(), rel_addr, ic_type, 
          entry.nak_waitlist.count(rel_clusterid), dir_state_string[entry.get_state()]);
      }
      #endif

      switch (ic_type) {
        case IC_MSG_CC_RD_RELEASE_ACK:
        case IC_MSG_CC_INVALIDATE_ACK:
          entry.dec_sharers(rel_clusterid);
          if (0 == entry.get_num_sharers()) { 
            entry.reset(); 
            entry.set_last_touch();
            if (entry.get_state() == DIR_STATE_PENDING_WRITE) {
              entry.set_state(DIR_STATE_WRITE_PRIV);
              entry.set_addr(rel_addr);
              entry.inc_sharers(waiting_cluster);
              entry.set_fill_pending(waiting_cluster);
            } else {
              // Remove pending eviction.
              dir_pending_replacement_way[set] = -1;
              dir_pending_replacement_bitvec[set] = false;
            }
          }
          break;
        case IC_MSG_CC_RD_RELEASE_NAK:
        case IC_MSG_CC_INVALIDATE_NAK:
        case IC_MSG_CC_RD_RELEASE_REQ:
          // We need to wait for the NAK/REL pair before we decrement the
          // count.  This is only an issue in the evict state.
          if (1 == entry.nak_waitlist.count(rel_clusterid)) 
          {
            #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == rel_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr, "[RELEASE] READ NAK WAITLIST NOT ZERO!!!: %d num_sharers: %d "
                              "addr: %08x "
                              "msg: %2d nak_waitlist.count(): %2d state: %s\n", 
                waiting_cluster, entry.get_num_sharers(), rel_addr, ic_type, 
                entry.nak_waitlist.count(rel_clusterid), dir_state_string[entry.get_state()]);
            }
            #endif
            entry.dec_sharers(rel_clusterid);
            if (0 == entry.get_num_sharers()) { 
              entry.reset(); 
              entry.set_last_touch();
              // Evictions do not have a wating cluster.
              if (entry.get_state() == DIR_STATE_PENDING_WRITE) {
                entry.set_state(DIR_STATE_WRITE_PRIV);
                entry.set_addr(rel_addr);
                entry.inc_sharers(waiting_cluster);
                entry.set_fill_pending(waiting_cluster);
              } else {
                // Remove pending eviction.
                dir_pending_replacement_way[set] = -1;
                dir_pending_replacement_bitvec[set] = false;
              }
            }
            return;
          } else {
            #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == rel_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr, "[RELEASE] READ NAK WAITLIST IS ZERO!!!: wait_cluster %d "
                              " num_sharers: %d addr: %08x "
                              " msg: %2d nak_waitlist.count(): %2d state: %s rel_clusterid: %d\n", 
                waiting_cluster, entry.get_num_sharers(), rel_addr, ic_type, 
                entry.nak_waitlist.count(rel_clusterid),
                  dir_state_string[entry.get_state()], rel_clusterid);
            }
            #endif
            entry.nak_waitlist.insert(rel_clusterid);
            return;
          }

        default:
          rigel::GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER();
          fprintf(stderr, "Unhandled request: %d\n", ic_type);
          assert(0 && "Invalid request type!");
      }
      break;
    }
    case DIR_STATE_WRITE_PRIV:
    {
      // This case can only happen when the directory cache serves as a probe
      // filter.
      if (rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY) { break; }
      /* Fall through */
    }
    // In the case where a line was WR_PRIVATE then evicted from the cluster
    // cache then pulled back in as read shared, we may need to dal with a
    // read release on a write-owned line.
    //case DIR_STATE_WRITE_PRIV:
    default:
      rigel::GLOBAL_debug_sim.dump_all();
      DEBUG_HEADER();
      fprintf(stderr, "BUG!  Invalid state transition for line. ");
      fprintf(stderr, "rel_addr: 0x%08x rel_clusterid: %d\n", rel_addr, rel_clusterid);
      DEBUG_HEADER(); 
      fprintf(stderr, "Errant entry: ");
      entry.dump(); assert(0);
  }
}

void 
CacheDirectory::helper_release_write_ownership( uint32_t rel_addr, 
                                                int rel_clusterid, icmsg_type_t ic_type,
                                                directory::dir_entry_t &entry)
{
  using namespace directory;

  size_t set = get_set(rel_addr);
  
  if (rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY) 
  {
    if (entry.get_state() == DIR_STATE_EVICTION_LOCK_READ_BCAST ||
        entry.get_state() == DIR_STATE_PENDING_WRITE_BCAST)
    {
      // Handle BCAST ACK the same way as other release requests.
      switch (ic_type) 
      {
        case IC_MSG_CC_BCAST_ACK:
        case IC_MSG_SPLIT_BCAST_INV_REPLY:
        {
          entry.dec_sharers(rel_clusterid);
          if (0 == entry.get_num_sharers()) 
          {
            // Added to make sure we clean up broadcasts before installing new entry
            //Remove bcast from outstanding list
            remove_outstanding_bcast(&entry);
            entry.reset();
            entry.set_state(entry.get_pf_bcast_next_state());
            entry.inc_sharers(entry.get_pf_bcast_next_sharer());
            entry.set_addr(rel_addr);
            entry.set_fill_pending(entry.get_pf_bcast_next_sharer());
            entry.set_last_touch();
          }
          break;
        }
        case IC_MSG_SPLIT_BCAST_SHR_REPLY:
        case IC_MSG_SPLIT_BCAST_OWNED_REPLY:
        {
          rigel::GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER();
          fprintf(stderr, "Invalid message: %d\n", ic_type);
          assert(0);
        }
        default: break; 
      }
      return;
    } 
  }

  // Check to make sure the line is in a proper state for a read_release.
  switch (entry.get_state()) 
  {
    case DIR_STATE_WRITE_PRIV:
    {
      switch (ic_type) 
      {
        case IC_MSG_EVICT_REQ:
        case IC_MSG_LINE_WRITEBACK_REQ:
          // All is good in the hood, release the ownership for realz.    
          entry.reset();
          break;
        default:
          rigel::GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER();
          fprintf(stderr, "Invalid message: %d\n", ic_type);
          assert(0);
      }
      break;
    }
    case DIR_STATE_PENDING_WB:
    case DIR_STATE_PENDING_READ:
    case DIR_STATE_PENDING_WRITE:
    case DIR_STATE_EVICTION_LOCK_WRITE:
    case DIR_STATE_EVICTION_LOCK_READ:
    {
      int waiting_cluster = entry.get_waiting_cluster();

      if ((entry.get_state() == DIR_STATE_EVICTION_LOCK_READ) || 
          (entry.get_state() == DIR_STATE_EVICTION_LOCK_WRITE)) {
        waiting_cluster = -43;
      }
        
      dir_state_t new_state;
      
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == rel_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[RELEASE] WRITE waiting_cluster: %d num_sharers: %d addr: %08x "
                        " msg: %2d nak_waitlist.count(): %2d state: %s\n", 
          waiting_cluster, entry.get_num_sharers(), rel_addr, ic_type, 
          entry.nak_waitlist.count(rel_clusterid), dir_state_string[entry.get_state()]);
      }
      #endif

      switch (ic_type) 
      {
        case IC_MSG_EVICT_REQ:
        case IC_MSG_LINE_WRITEBACK_REQ:
        case IC_MSG_CC_WR_RELEASE_ACK:
        case IC_MSG_CC_WR_RELEASE_NAK:
        {
          if ( (1 == entry.nak_waitlist.count(rel_clusterid)) ||
               (IC_MSG_CC_WR_RELEASE_ACK == ic_type)
            ) 
          {
            if (DIR_STATE_PENDING_WRITE == entry.get_state()) {
               new_state = DIR_STATE_WRITE_PRIV; 
            } else 
            if (DIR_STATE_PENDING_WB == entry.get_state()) { 
              // Even though a write may have triggered the WB, we set it to shared
              // with a single sharer so a waiting writer will upgrade immediately.
              new_state = DIR_STATE_READ_SHARED; 
            } else 
            if ((DIR_STATE_EVICTION_LOCK_WRITE == entry.get_state()) ||
                (DIR_STATE_EVICTION_LOCK_READ  == entry.get_state())) 
            {
              // Remove pending eviction.
              dir_pending_replacement_way[set] = -1;
              dir_pending_replacement_bitvec[set] = false;

              // Eviction is complete!  We can just reset the line and move
              // forward.
              entry.reset();
              entry.set_last_touch();
              break;
            } else { assert(0); }

            entry.reset(); 
            entry.set_addr(rel_addr);
            entry.inc_sharers(waiting_cluster);
            entry.set_state(new_state);
            entry.set_fill_pending(waiting_cluster);
            break;
          } else if (0 == entry.nak_waitlist.count(rel_clusterid)) {
            // Still waiting on the NAK/actual writeback from the cluster.
            entry.nak_waitlist.insert(rel_clusterid);
            break;
          } else {
            rigel::GLOBAL_debug_sim.dump_all();
            DEBUG_HEADER();
            fprintf(stderr, "Unhandled message type: %d\n", ic_type);
            assert(0);
          }
        }
        default:
        {
          rigel::GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER();
          fprintf(stderr, "Unhandled message type: %d\n", ic_type);
          assert(0);
        }
      }
      break;
    }
    default:
      rigel::GLOBAL_debug_sim.dump_all();
      DEBUG_HEADER();
      fprintf(stderr, "BUG!  Invalid state transition for line. ");
      fprintf(stderr, "rel_addr: 0x%08x rel_clusterid: %d\n", rel_addr, rel_clusterid);
      DEBUG_HEADER(); 
      fprintf(stderr, "Errant entry: ");
      entry.dump(); assert(0);
  }
}

void
CacheDirectory::dump()
{
  fprintf(stderr, "DUMPING DIRECTORY:\n");
  for (size_t set = 0; set < config.num_sets; set++) {
    for (size_t way = 0; way < config.num_ways; way++) {
      if (get_entry(set, way).check_valid()) {
        fprintf(stderr, "[%5d][%5d] ", (int)set,(int)way);
        get_entry(set, way).dump();
      }
    }
  }
  fprintf(stderr, "Pending message queue: \n");
  int pq_idx = 0;
  while (!pending_msg_q.empty()) 
  {
    fprintf(stderr, "pending_msg_q[%4d] ", pq_idx++); 
    pending_msg_q.front().dump(__FILE__, __LINE__);
    pending_msg_q.pop();
  }
}

size_t 
CacheDirectory::get_way(uint32_t addr) const
{
  size_t set = get_set(addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    directory::dir_entry_t &entry = dir_array[i];
    if (entry.check_addr_match(addr)) {
      return (i-startOfSet);
    }
  }

  rigel::GLOBAL_debug_sim.dump_all();
  assert(0 && "Address not found in directory!  Only call get_way on known-resident addresses.");

  return 0xFFFFFFFF;
}

// Return a reference to the entry in the set with the fewest touches per cycle since allocation.
directory::dir_entry_t &
CacheDirectory::get_least_frequently_touched_entry(size_t set)
{
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  int i = valid_bits.findNextClearInclusive(startOfSet);
  if((i != -1) && (i < endOfSet))
    return dir_array[i];
  //All are valid.  Let LFU for realz.
  directory::dir_entry_t &way0 = get_entry(set, 0);
  //Need to add 1 to the time in case we call same cycle.
  float lowest_touch_freq = ((float)way0.get_num_touches() /
                             (float)(rigel::CURR_CYCLE - way0.allocation_cycle + 1));
  int least_frequently_touched_way = 0;
  for(size_t way = 0; way < config.num_ways; way++) {
    directory::dir_entry_t &tmp = get_entry(set, way);
    float touch_freq = ((float)tmp.get_num_touches() /
                        (float)(rigel::CURR_CYCLE - tmp.allocation_cycle + 1));
    if(touch_freq < lowest_touch_freq) {
      lowest_touch_freq = touch_freq;
      least_frequently_touched_way = way;
    }
  }
  return get_entry(set, least_frequently_touched_way);
}

  
// Return a reference to the entry in the set with the fewest touches since allocation.
directory::dir_entry_t &
CacheDirectory::get_least_touched_entry(size_t set)
{
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  int i = valid_bits.findNextClearInclusive(startOfSet);
  if((i != -1) && (i < endOfSet))
    return dir_array[i];
  //All are valid.  Let LFU for realz.
  directory::dir_entry_t &way0 = get_entry(set, 0);
  int lowest_touches = way0.get_num_touches();
  int least_touched_way = 0;
  for(size_t way = 0; way < config.num_ways; way++) {
    directory::dir_entry_t &tmp = get_entry(set, way);
    int touches = tmp.get_num_touches();
    if(touches < lowest_touches) {
      lowest_touches = touches;
      least_touched_way = way;
    }
  }
  return get_entry(set, least_touched_way);
}

directory::dir_entry_t &
CacheDirectory::get_lru_entry(size_t set)
{
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  int i = valid_bits.findNextClearInclusive(startOfSet);
  if((i != -1) && (i < endOfSet))
    return dir_array[i];

  // All are valid.  Get LRU for realz.
  uint64_t oldest_access = get_entry(set, 0).get_last_touch();
  int oldest_way = 0;
  for (size_t way = 0; way < config.num_ways; way++) {
    if (get_entry(set, way).get_last_touch() < oldest_access) {
      oldest_access = get_entry(set, way).get_last_touch(); 
      oldest_way = way;
    }
  }
  return get_entry(set, oldest_way);  
}

directory::dir_entry_t &
CacheDirectory::get_lra_entry(size_t set)
{
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  int i = valid_bits.findNextClearInclusive(startOfSet);
  if((i != -1) && (i < endOfSet))
    return dir_array[i];
  
  // All are valid.  Get LRA for realz.
  uint64_t oldest_access = get_entry(set, 0).allocation_cycle;
  int oldest_way = 0;
  for (size_t way = 0; way < config.num_ways; way++) {
    if (get_entry(set, way).allocation_cycle < oldest_access) {
      oldest_access = get_entry(set, way).allocation_cycle;
      oldest_way = way;
    }
  }
  return get_entry(set, oldest_way);
}

directory::dir_entry_t &
CacheDirectory::get_least_shared_entry(size_t set)
{
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  int i = valid_bits.findNextClearInclusive(startOfSet);
  if((i != -1) && (i < endOfSet))
    return dir_array[i];
  
  //All are valid.  Let LS for realz.
  directory::dir_entry_t &way0 = get_entry(set, 0);
  //Need to add 1 to the time in case we call same cycle.
  float lowest_sharer_freq = ((float)way0.get_num_sharers() /
  (float)(rigel::CURR_CYCLE - way0.allocation_cycle + 1));
  int least_frequently_shared_way = 0;
  for(size_t way = 0; way < config.num_ways; way++) {
    directory::dir_entry_t &tmp = get_entry(set, way);
    float sharer_freq = ((float)tmp.get_num_sharers() /
    (float)(rigel::CURR_CYCLE - tmp.allocation_cycle + 1));
    if(sharer_freq < lowest_sharer_freq) {
      lowest_sharer_freq = sharer_freq;
      least_frequently_shared_way = way;
    }
  }
  return get_entry(set, least_frequently_shared_way);
}

// Return a reference to the entry selected for replacement, based on our replacement policy.
directory::dir_entry_t &
CacheDirectory::get_replacement_entry(size_t set)
{
  switch(rigel::DIRECTORY_REPLACEMENT_POLICY)
  {
    case dir_rep_lru: return get_lru_entry(set); break;
    case dir_rep_lfu: return get_least_frequently_touched_entry(set); break;
    case dir_rep_lu: return get_least_touched_entry(set); break;
    case dir_rep_lra: return get_lra_entry(set); break;
    case dir_rep_ls: return get_least_shared_entry(set); break;
    default: assert(0 && "Unrecognized directory replacement policy"); break;
  }
}

void
CacheDirectory::DEBUG_print_sharing_vector(uint32_t req_addr, FILE *stream)
{
  using namespace directory;
  size_t set = get_set(req_addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    directory::dir_entry_t &entry = dir_array[i];
    // Skip over entries which do not match the address.
    if (!entry.check_addr_match(req_addr)) { continue; }
    entry.sharer_vec.print(stream);
  }
}

bool
CacheDirectory::insert_pending_icmsg(ICMsg &msg)
{
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == msg.get_addr() && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER();
    fprintf(stderr, "[PENDING MSG INSERT] ");
    msg.dump(__FILE__, __LINE__);
  }
  #endif
  pending_msg_q.push(msg);
  // TODO: Block when full.
  return true;
}

void
CacheDirectory::remove_next_pending_icmsg()
{
  assert(!pending_msg_q.empty() 
    && "Only call if a get_next_pending_icmsg() returned true in this cycle.");
  pending_msg_q.pop();
}

bool 
CacheDirectory::get_next_pending_icmsg(ICMsg &msg)
{
  if (pending_msg_q.empty()) { return false; }
  // Found a message, return it to the cache model.
  msg = pending_msg_q.front();
  // Note that timer is only counting one way.
  msg.timer_memory_wait.StartTimer();
  
  #ifdef DEBUG_DIRECTORY
  DEBUG_HEADER();
  fprintf(stderr, "MSG: ");
  msg.dump(__FILE__, __LINE__);
  fprintf(stderr, "\n");
  #endif

  return true;
}

void
CacheDirectory::clear_fill_pending(uint32_t req_addr, int req_clusterid)
{
  using namespace directory;

  size_t set = get_set(req_addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (entry.check_addr_match(req_addr)) 
    {
      #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr, "[FILL PENDING] CLEAR req_clusterid: %3d addr: %08x "
                              " state: %s\n", 
                req_clusterid, req_addr, dir_state_string[entry.get_state()]);
            }
      #endif
      entry.clear_fill_pending(req_clusterid);
      return;
    }
  }
  // If everything is tracked by the coherence protocol, this should never fail.
  // however, since we are not using coherence for global operations yet, you
  // may get GCache hits that do not have a directory entry associated with them
  
#if 0
  dump();
  DEBUG_HEADER();
  fprintf(stderr, "Calling clear_fill_pending() without a valid entry for the address.\n");
  DEBUG_HEADER();
  fprintf(stderr, "addr: 0x%08x cluster id: %4d\n", req_addr, req_clusterid);
  assert(0);
#endif
}
