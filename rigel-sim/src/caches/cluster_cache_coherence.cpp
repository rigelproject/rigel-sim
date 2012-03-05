////////////////////////////////////////////////////////////////////////////////
/// src/caches/cluster_cache_coherence.cpp
////////////////////////////////////////////////////////////////////////////////
///
///  This file contains the implementation of the L2D coherence-related methods.
///
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, stderr, NULL
#include <set>                          // for _Rb_tree_const_iterator, etc
#include "memory/address_mapping.h"  // for AddressMapping
#include "cache_model.h"    // for CacheModel
#include "cache_tags.h"     // for CacheTag
#include "caches/cache_base.h"  // for CacheAccess_t, etc
#include "caches/l2d.h"     // for L2Cache
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for DEBUG_HEADER, icmsg_type_t, etc
#include "util/dynamic_bitset.h"  // for DynamicBitset
#include "icmsg.h"          // for ICMsg
#include "mshr.h"           // for MissHandlingEntry
#include "seqnum.h"         // for seq_num_t
#include "sim.h"            // for CORES_PER_CLUSTER, etc

using namespace rigel;

// Move to rigel in sim.h later if anyone cares...
static const int WDT_PROBE_FAILURES = 1000;

#if 0
#define __DEBUG_ADDR 0x20400020
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#define __DEBUG_CYCLE 0x7FFFFFFF
#endif

////////////////////////////////////////////////////////////////////////////////
/// helper_handle_ccbcast_probe()
////////////////////////////////////////////////////////////////////////////////
bool
L2Cache::helper_handle_ccbcast_probe(MissHandlingEntry<cache::LINESIZE> &mshr)
{
  const icmsg_type_t bcast_type = mshr.GetICMsgType();

  assert(rigel::ENABLE_EXPERIMENTAL_DIRECTORY
    && "This helper should not be called without coherence enabled!");
  assert((IC_MSG_CC_BCAST_PROBE == bcast_type || IC_MSG_SPLIT_BCAST_INV_REQ  == bcast_type
          || IC_MSG_SPLIT_BCAST_SHR_REQ == bcast_type) 
          && "Can only call with a BCAST PROBE!");
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[BCAST PROBE] ATTEMPT: cluster: %d addr: 0x%08x IsValid: %1d\n",
        cluster_num, mshr.get_addr(), IsValidLine(mshr.get_addr()));
      mshr.dump();
      fprintf(stderr, "\n");
  }
  #endif

  // Hack to fix timeout in bug 148 for --sleepy-cores. 
  for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
    if (pendingMiss[i].check_valid()) {
      uint32_t addr = pendingMiss[i].get_addr();
      // Wake up any cores waiting on code to avoid deadlock.
      if (addr < rigel::CODEPAGE_HIGHWATER_MARK) {
        pendingMiss[i].wake_cores_all();
      }
    }
  }

  // Return if there are no MSHR entries available since we need to produce a
  // reply this cycle.  Other coherence messages can use an eviction slot.
  // Broadcasts must not block evictions. 
  if (mshr_full(false, true)) { 
    return false; 
  }
  // Also check if we have to also insert a writeback message due to a BCAST
  // taking over the dir entry of a WRITE_PRIVATE line.
  if (ENABLE_PROBE_FILTER_DIRECTORY && mshr.get_bcast_write_shootdown_required()) {
    uint32_t shootdown_addr = mshr.get_bcast_write_shootdown_addr();
    if (IsDirtyLine(shootdown_addr)) {
      if (mshr_full(true)) { return false; }
      // Send the reply back only if it is cached.  Not sure if this should be
      // an eviction or a CC message.
      CacheAccess_t ca(shootdown_addr, CORES_PER_CLUSTER+1, 
        POISON_TID, IC_MSG_CC_WR_RELEASE_ACK, NullInstr);
      // Make line clean if it is cached.
      clearDirtyLine(mshr.get_bcast_write_shootdown_addr());
      pend( ca, true );
    }
  }

  if ( IsPending(mshr.get_addr()) != NULL ) {
    uint32_t addr = mshr.get_addr();
    for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
      if (pendingMiss[i].check_valid(addr) &&
          (IC_MSG_READ_REQ == pendingMiss[i].GetICMsgType() ||
           IC_MSG_CCACHE_HWPREFETCH_REQ == pendingMiss[i].GetICMsgType()))
      {
        // If it has left the cluster, we need to see if the directory has seen
        // the request or not.  If the directory has seen the request, then we
        // poison the request and ack the directory.
        if (pendingMiss[i].IsRequestAcked()) {
          int gcache_bank =  AddressMapping::GetGCacheBank(mshr.get_addr());
          seq_num_t dir_seq = mshr.get_seq_number();
          int seq_channel = pendingMiss[i].get_seq_channel();
          if (0 > seq_channel) {
              rigel::GLOBAL_debug_sim.dump_all();
              DEBUG_HEADER(); fprintf(stderr, "INPUT MSHR: ");
              mshr.dump(); fprintf(stderr, "\n");
              DEBUG_HEADER(); fprintf(stderr, "PENDING MSHR: ");
              pendingMiss[i].dump(); fprintf(stderr, "\n");
              assert(0 && "Invalid seq_channel!");
          }
          if (seq_num[gcache_bank].cluster_ts[seq_channel] >
              dir_seq.directory_ts[seq_channel])
          {
            // Directory has not seen the read request. We can just send the
            // BCAST ACK and we will eventually get the read reply too.
          } else {
            // We only poison if the directory is going to send us a reply.
            pendingMiss[i].set_probe_poison_bit();
          }
        } else {
          // I think we could speed things up here by squashing this request in
          // the case that the network has not taken it yet.  Now we end up
          // poisoning it and waiting to re-request.
          pendingMissFifoRemove(pendingMiss[i].GetPendingIndex());
          pendingMiss[i].clear(false);
          pendingMiss[i]._core_id = -1;
          pendingMiss[i].clear_probe_poison_bit();
        }
      }
      // We may need to put this line back in so that we hold back a BCAST ACK
      // until after the read release reply has come back from the directory.
      // This will ensure that we do not have the ACK get reordered with the REL
      // in the network which could lead to the directory getting a REL for a
      // line it does not think this cluster caches any longer (since we sent it
      // an ACK already).
      else if (pendingMiss[i].check_valid(addr)
        && (IC_MSG_CC_RD_RELEASE_REQ == pendingMiss[i].GetICMsgType()))
      {
        if (pendingMiss[i].IsRequestAcked()) {
          // If there is already a read release pending, we need to poison this
          // and hold back the BCAST ACK until the reply comes back.  We need to
          // remove the BCAST PROBE from the network, otherwise we will end up
          // blocking the read release reply that is coming back.
          pendingMiss[i].set_probe_poison_bit();
          return true;
        } else {
          // If the read release has not been taken by the network, we just
          // squash it and let the BCAST ack serve as our release operation.
          pendingMissFifoRemove(pendingMiss[i].GetPendingIndex());
          pendingMiss[i].clear(false);
          pendingMiss[i]._core_id = -1;
          pendingMiss[i].clear_probe_poison_bit();
        }
      }
    }
  }

  // Check to see if the line that was brought in has been accessed yet.
  if (IsValidLine(mshr.get_addr())) {
    uint32_t addr = mshr.get_addr();
    int set = get_set(addr);
    int way = get_way_line(addr);

    if (!TagArray[set][way].get_accessed_bit()) {

      #ifdef __DEBUG_ADDR
        if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
          DEBUG_HEADER();
          fprintf(stderr, "[BCAST PROBE] FAIL: "
                          "Trying to probe a line that has not been touched yet.\n");
          mshr.dump();
          fprintf(stderr, "\n");
        }
      #endif
      return false;
    }
  }
  #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[BCAST PROBE] Inserting ACK: ");
      mshr.dump();
      fprintf(stderr, "\n");
    }
  #endif

  // Only used when we want to allow rebuilding of sharer vector.
  if (IC_MSG_SPLIT_BCAST_SHR_REQ != bcast_type) {
    // Find the line and invalidate it in the cache.
    if (IsValidLine(mshr.get_addr())) {
      // State is now INVALID.
      invalidate_line(mshr.get_addr());
    }
    // Regardless of state in L2, invalidate in all L1's if found.
    Caches->EvictDown(mshr.get_addr());
  }

  // If the broadcast network is enabled, then we may need to source the data
  // (OWNED) or we notify the directory that we are still holding the line
  // (SHARED) or we do not have it so we invalidate (INVALID).  Otherwise, it is
  // a normal broadcast that we ACK.
  icmsg_type_t msg_type = ENABLE_BCAST_NETWORK ? 
    (bcast_type == IC_MSG_SPLIT_BCAST_SHR_REQ && IsValidLine(mshr.get_addr()) 
          ? ( IsDirtyLine(mshr.get_addr()) 
              ? IC_MSG_SPLIT_BCAST_SHR_REPLY : IC_MSG_SPLIT_BCAST_OWNED_REPLY ) 
          : IC_MSG_SPLIT_BCAST_INV_REPLY) 
      : IC_MSG_CC_BCAST_ACK;

  // Send the reply back whether it was cached or not.
  CacheAccess_t ca(
    mshr.get_addr(),    // address
    CORES_PER_CLUSTER+1,// core number (faked here)
    rigel::POISON_TID,// thread id  (is it safe to get TID from mshr here?)
    msg_type,
    rigel::NullInstr  // no instruction correlated to this access
  );

  // If we are returning an OWNED reply, we need to clean the data since it is
  // going into the shared state at the directory.  For simplicity, I am going
  // to avoid the dirty L1 problem by evicting down. 
  if (IC_MSG_SPLIT_BCAST_SHR_REQ == bcast_type) {
    if (IsDirtyLine(mshr.get_addr())) { 
      clearDirtyLine(mshr.get_addr()); 
      Caches->EvictDown(mshr.get_addr());
    }
  } else {
    IsValidLine(mshr.get_addr()) ? 
      ca.set_bcast_resp_valid() : ca.clear_bcast_resp_valid();
  }
  pend( ca, true );
  // Another bcast is outstanding.  Decremented at removal.
  outstanding_bcast_ack++;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// helper_handle_coherence_probe()
////////////////////////////////////////////////////////////////////////////////
bool
L2Cache::helper_handle_coherence_probe(MissHandlingEntry<cache::LINESIZE> &mshr)
{
  assert(rigel::ENABLE_EXPERIMENTAL_DIRECTORY
    && "This helper should not be called without coherence enabled!");

  //HACK FIXME this could be prohibitively slow if we call this function often
  // Hack to fix timeout in bug 148 for --sleepy-cores. 
  for(int i = validBits.findFirstSet(); i != -1; i = validBits.findNextSet(i)) {
    for(std::set<uint32_t>::const_iterator it = pendingMiss[i].get_pending_addrs_begin(),
                                     end = pendingMiss[i].get_pending_addrs_end();
        it != end; ++it)
    {
      const uint32_t &addr = *it;
      // Wake up any cores waiting on code to avoid deadlock.
      if (addr < rigel::CODEPAGE_HIGHWATER_MARK) {
        pendingMiss[i].wake_cores_all();
      }
    }
  }

  // If we run out of MSHRs, we cannot handle the probe this cycle.  Any probe
  // handling will need an MSHR, so we short circuit it up here instead of
  // waiting until just before the pend() call.  We also want to avoid modifying
  // any of the existing MSHRs or the cache state if we have to redo the probe
  // later anyway.
  if (mshr_full(true)) { return false; }

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[PROBE] ATTEMPT: cluster: %d addr: 0x%08x IsValid: %1d\n",
        cluster_num, mshr.get_addr(), IsValidLine(mshr.get_addr()));
      mshr.dump();
      fprintf(stderr, "\n");
  }
  #endif

  uint32_t addr = mshr.get_addr();

  // The only thing that should be pending here is a RD->WR upgrade.  We poison
  // the MSHR and do not let it Fill() later.  The probe might beat the fill
  // back from the directory so this handles that case.
  //
  if ( IsPending(addr) != NULL )
  {
    for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++)
    {
      // We want to ignore global operations from the perspective of the
      // coherence protocol.
      if (pendingMiss[i].check_valid(addr)
           && !ICMsg::check_is_global_operation(pendingMiss[i].GetICMsgType()))
      {
        // If it has left the cluster, we invalidate.  If it is not present yet,
        // we set the NAK bit and defer the writeback action.
        if (pendingMiss[i].IsRequestAcked())
        {
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr, "[POISONING MSHR (REQ ACKED)]: cluster: %d\n", cluster_num);
              fprintf(stderr, "\n");
          }
          #endif
          if (IsValidLine(addr))
          {
            //FIXME We can reach this code in a correct case because of the semantics of Fill().
            //Fill() makes the line valid in the cache, and schedules the MSHR which was pending
            //on it to be ready L2D_ACCESS_LATENCY cycles in the future.  If we service a probe
            //during that window, the MSHR will have the line in its ready queue but it won't
            //be clear()ed.
            
            rigel::GLOBAL_debug_sim.dump_all();
            DEBUG_HEADER(); fprintf(stderr, "Found a valid line that was acked by the cluster "
                                            "cache.  This should not happen (I think?)\n");
            DEBUG_HEADER(); fprintf(stderr, "pendingMiss[%3d]: ", i); pendingMiss[i].dump();
            DEBUG_HEADER(); fprintf(stderr, "probe MSHR: "); mshr.dump();
            assert(0 && "We should never get here.");
            // I am not sure if this ever happens with coherence turned on
            // since any pending request may not have a valid entry in the
            // cluster cache.
          } else {
            switch (pendingMiss[i].GetICMsgType())
            {
              case IC_MSG_READ_REQ:
              case IC_MSG_CCACHE_HWPREFETCH_REQ:
              case IC_MSG_INSTR_READ_REQ:
              case IC_MSG_WRITE_REQ:
                // The NAK bit is set so that when the reply returns, a response is
                // sent frome the cluster.
                pendingMiss[i].set_probe_nak_bit();
                #ifdef __DEBUG_ADDR
                if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F)
                      && (__DEBUG_CYCLE < rigel::CURR_CYCLE))
                {
                    DEBUG_HEADER();
                    fprintf(stderr, "[SETTING NAK BIT] Demand Access: cluster: %d\n", cluster_num);
                    fprintf(stderr, "\n");
                }
                #endif
              default: break;
            }
          }
        } else {
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr, "[POISONING MSHR (REQ NOT ACKED)]: cluster: %d\n", cluster_num);
              fprintf(stderr, "\n");
              fprintf(stderr, "PENDING MISS: ");
              pendingMiss[i].dump();
          }
          #endif
          switch (pendingMiss[i].GetICMsgType())
          {
            case IC_MSG_WRITE_REQ:
              // If the message is a write and was triggered by a write that
              // found the line in shared state in the cluster cache, we may
              // have to worry about starving that write if we continually clear
              // the MSHR and allow the probe to go forward.  The warning will
              // let us know if it is really an issue.
              if (pendingMiss[i].read_to_write_upgrade) {
                DEBUG_HEADER();
                fprintf(stderr, "[COHERENCE WARNING] Squashing a read-to-write upgrade to allow "
                                "a probe to make forward progress. cluster: %3d", cluster_num);
                mshr.dump();
              }
            case IC_MSG_CCACHE_HWPREFETCH_REQ:
            case IC_MSG_READ_REQ:
            case IC_MSG_INSTR_READ_REQ:
              if (pendingMiss[i].GetICMsgType() == IC_MSG_CCACHE_HWPREFETCH_REQ)
                { dec_hwprefetch_count(); }
              // If the request has not been taken by the network yet, we can
              // simply remove it and then reissue it once the invalidation
              // probe is handled.  The one problem that we do run into is that
              // we may end up starving a writer core now that the
              // read-to-write upgrade atomicity may be broken. Hrm.
              pendingMissFifoRemove(pendingMiss[i].GetPendingIndex());
              pendingMiss[i].clear(false);
              pendingMiss[i]._core_id = -1;
              pendingMiss[i].clear_probe_poison_bit();

              goto done_check;
            default: break;
          }
          // We found a request that was just inserted but has not yet be
          // injected into the network.  For now, we will just stall the probe.
          // When this request is taken by the network, we will service the
          // probe.  I found this bug in a one cycle WoV.  Beware!
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER();
              fprintf(stderr, "[POISONING MSHR (FAILED PENDING L2D REQ ACK)]: "
                              "cluster: %d\n", cluster_num);
              fprintf(stderr, "\n");
          }
          #endif
          return false;
        }
        // If we find one, we can poison it and move on.  We may need to
        // consider the accessed bit too....
        goto done_check;
      }
    }
  }

  // Check to see if the line that was brought in has been accessed yet.
  if (IsValidLine(mshr.get_addr()))
  {
    uint32_t addr = mshr.get_addr();
    int set = get_set(addr);
    int way = get_way_line(addr);

    if (!TagArray[set][way].get_accessed_bit())
    {
      #ifdef __DEBUG_ADDR
        if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
          DEBUG_HEADER();
          fprintf(stderr, "[PROBE] FAIL: Trying to probe a line that has not been touched yet.\n");
          mshr.dump();
          fprintf(stderr, "\n");
        }
      #endif

      if (wdt_count_access_bit_probe_failures++ > WDT_PROBE_FAILURES) {
        DEBUG_HEADER();
        fprintf(stderr, "[PROBE] FAIL: Trying to probe a line that has not been touched yet.\n");
        mshr.dump();
        fprintf(stderr, "\n");
        fprintf(stderr, "[CCACHE TAG]: ");
        TagArray[set][way].dump();
        fprintf(stderr, "\n");
        fprintf(stderr, "Cluster: %3d Failed attempts: %8d\n",
          cluster_num, wdt_count_access_bit_probe_failures);
        // I am taking away the assertion to see if we can make forward
        // progress.  Somehow, a request might be getting lost and allowing a
        // line to get stuck for an arbitrarily long time.
        //
        // If a line has been sitting unaccessed for long enough, we release it
        // to avoid protocol deadlock. 
        goto done_check;
      }

      return false;
    }
  }
done_check:
  // Reset the watchdog for probe failures.
  wdt_count_access_bit_probe_failures = 0;

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[PROBE] ATTEMPT: Doing PROBE reply: %d\n", cluster_num);
      mshr.dump();
      fprintf(stderr, "\n");
  }
  #endif

  // FIXME For now, the replies back up to the GCache are going to overload the
  // is_eviction flag on pend().  In the future we may need to reserve a spot in
  // the request buffer for coherence messages too.
  switch(mshr.GetICMsgType())
  {
    case (IC_MSG_CC_WR_RELEASE_PROBE):
    {
      icmsg_type_t return_msg_type;

      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == (mshr.get_addr() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[CC PROBE] WR_REL probe at CCache.  addr: %08x cluster: %d\n",
          mshr.get_addr(), cluster_num);
      }
      #endif

      // Find the line and invalidate it in the cache.
      if (IsValidLine(mshr.get_addr())) {
        // State is now INVALID.
        invalidate_line(mshr.get_addr());
        return_msg_type = IC_MSG_CC_WR_RELEASE_ACK;
      } else {
        // Message not found, send a NAK.  A writeback or write request is
        // pending.  If it is a writeback, the NAK + the WB will release the
        // line.  If it is a write request, the code block above has already set
        // the nak bit for the pending write request which will send a WB when
        // the response from the directory arrives.  Either way, the NAK+WB will
        // get to the directory.
        return_msg_type = IC_MSG_CC_WR_RELEASE_NAK;
      }

      // Regardless of state in L2, invalidate in all L1's if found.
      Caches->EvictDown(mshr.get_addr());

      // Send the reply back whether it was cached or not.
      // TODO: The pends should be fire-and-forget.  FIXME FIXME The way it
      // is now, an MSHR will expect a reply that will never come :'(
      //pend(mshr.get_addr(), return_msg_type,
      //  CORES_PER_CLUSTER+1, -1, true, rigel::NullInstr, 0, FIXME_TID);
      CacheAccess_t ca(
        mshr.get_addr(),    // address
        CORES_PER_CLUSTER+1,// core number (faked here?)
        rigel::POISON_TID,//mshr.GetThreadID(), // thread id
        return_msg_type,    // interconnect message type
        rigel::NullInstr  // no instruction correlated to this access
      );
      //FIXME what happens if this fails (out of MSHRs?)
      pend(ca, true);

      break;
    }
    case (IC_MSG_CC_INVALIDATE_PROBE):
    {
      #ifdef DEBUG_CACHES
      DEBUG_HEADER();
      fprintf(stderr, "[CC PROBE] INV probe at CCache.  addr: %08x cluster: %d\n",
        mshr.get_addr(), cluster_num);
      #endif

      icmsg_type_t return_msg_type;

      // Find the line and invalidate it in the cache.
      if (IsValidLine(mshr.get_addr())) {
        // State is now INVALID.
        invalidate_line(mshr.get_addr());
        return_msg_type = IC_MSG_CC_INVALIDATE_ACK;
      } else {
        // Message not found, send a NAK.
        return_msg_type = IC_MSG_CC_INVALIDATE_NAK;
      }

      // Regardless of state in L2, invalidate in all L1's if found.
      Caches->EvictDown(mshr.get_addr());
      // Send the reply back whether it was cached or not.
      // TODO: The pends should be fire-and-forget.  FIXME FIXME The way it
      // is now, an MSHR will expect a reply that will never come :'(
      //pend(mshr.get_addr(), return_msg_type,
      //  CORES_PER_CLUSTER+1, -1, true, rigel::NullInstr, 0, FIXME_TID);
      CacheAccess_t ca(
        mshr.get_addr(),    // address
        CORES_PER_CLUSTER+1,// core number (faked here?)
        rigel::POISON_TID,//mshr.GetThreadID(), // thread id
        return_msg_type,    // interconnect message type
        rigel::NullInstr  // no instruction correlated to this access
      );
      pend(ca, true);

      break;
    }
    case (IC_MSG_CC_RD_RELEASE_PROBE):
    {
      icmsg_type_t return_msg_type;

      #ifdef DEBUG_CACHES
      DEBUG_HEADER();
      fprintf(stderr, "[CC PROBE] RD_REL probe at CCache.  addr: %08x cluster: %d\n",
        mshr.get_addr(), cluster_num);
      #endif

      // Find the line and invalidate it in the cache.
      if (IsValidLine(mshr.get_addr())) {
        // State is now INVALID.
        invalidate_line(mshr.get_addr());
        return_msg_type = IC_MSG_CC_RD_RELEASE_ACK;
      } else {
        // Message not found, send a NAK.
        return_msg_type = IC_MSG_CC_RD_RELEASE_NAK;
      }

      // Regardless of state in L2, invalidate in all L1's if found.
      Caches->EvictDown(mshr.get_addr());
      // Send the reply back whether it was cached or not.
      // TODO: The pends should be fire-and-forget.  FIXME FIXME The way it
      // is now, an MSHR will expect a reply that will never come :'(
      //pend(mshr.get_addr(), return_msg_type,
      //  CORES_PER_CLUSTER+1, -1, true, rigel::NullInstr, 0, FIXME_TID);
      CacheAccess_t ca(
        mshr.get_addr(),    // address
        CORES_PER_CLUSTER+1,// core number (faked here?)
        rigel::POISON_TID,//mshr.GetThreadID(),          // thread id
        return_msg_type,    // interconnect message type
        rigel::NullInstr  // no instruction correlated to this access
      );
      pend( ca, true );

      break;
    }
    case (IC_MSG_CC_WR2RD_DOWNGRADE_PROBE):
    {
      icmsg_type_t return_msg_type;

      #ifdef DEBUG_CACHES
      DEBUG_HEADER();
      fprintf(stderr, "[CC PROBE] WR2WR_DOWNGRADE probe at CCache.  addr: %08x cluster: %d\n",
        mshr.get_addr(), cluster_num);
      #endif

      // Find the line and invalidate it in the cache.
      if (IsValidLine(mshr.get_addr()))
      {
        // State is now SHARED.
        set_coherence_state(mshr.get_addr(), CC_STATE_S);
        clearDirtyLine(mshr.get_addr());

        return_msg_type = IC_MSG_CC_RD_RELEASE_ACK;
      } else {
        // Message not found, send a NAK.
        return_msg_type = IC_MSG_CC_RD_RELEASE_NAK;
      }

      // Regardless of state in L2, invalidate in all L1's if found.
      Caches->EvictDown(mshr.get_addr());
      // Send the reply back whether it was cached or not.
      // TODO: The pends should be fire-and-forget.  FIXME FIXME The way it
      // is now, an MSHR will expect a reply that will never come :'(
      //pend(mshr.get_addr(), return_msg_type,
      //  CORES_PER_CLUSTER+1, -1, true, rigel::NullInstr, 0, FIXME_TID);
      CacheAccess_t ca(
        mshr.get_addr(),    // address
        CORES_PER_CLUSTER+1,// core number (faked here?)
        rigel::POISON_TID,//mshr.GetThreadID(),          // thread id
        return_msg_type,    // interconnect message type
        rigel::NullInstr  // no instruction correlated to this access
      );
      pend(ca, true);

      break;
    }
    default:
      rigel::GLOBAL_debug_sim.dump_all();
      mshr.dump();
      assert(0 && "Unknown PROBE type!\n");
  }
  return true;
}

void
L2Cache::set_read_to_write_upgrade_pending(uint32_t addr) 
{
  for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
    if (pendingMiss[i].check_valid(addr)) {
      if (pendingMiss[i].GetICMsgType() != IC_MSG_CC_RD_RELEASE_REQ) {
        fprintf(stderr, "CLUSTER: %d\n", cluster_num);
        fprintf(stderr, "RD->WR Upgrade to invalid state. addr: %08x ic_msg_type: %2d\n",
          addr, pendingMiss[i].GetICMsgType());
        // A read-to-write upgrade can only occur while the release is pending.
        rigel::GLOBAL_debug_sim.dump_all();
        assert(0 && "Read to write upgrade to an invalid state.");
      }
      // We should never try to upgrade an incoherent access at this stage.
      if (pendingMiss[i].get_incoherent()) {
        assert(0 && "Found invalid read to write upgrade: Coherence bit is set.\n");
      }
      pendingMiss[i].read_to_write_upgrade = true;
      return;
    }
  }
  rigel::GLOBAL_debug_sim.dump_all();
  fprintf(stderr, "Read to write upgrade failed to find request for addr: 0x%08x.", addr);
  assert(0);
}

