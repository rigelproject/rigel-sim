////////////////////////////////////////////////////////////////////////////////
/// src/caches/cache_model_write.cpp
////////////////////////////////////////////////////////////////////////////////
///
///  This file contains the includes write data cache interaction with core
///  model.
///
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, NULL, stderr
#include "memory/address_mapping.h"  // for AddressMapping
#include "caches_legacy/cache_model.h"    // for CacheModel
#include "caches_legacy/cache_base.h"  // for CacheAccess_t, etc
#include "caches_legacy/global_cache.h"  // for GlobalCache
#include "caches_legacy/l1d.h"     // for L1DCache
#include "caches_legacy/l2d.h"     // for L2Cache
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for DEBUG_HEADER, etc
#include "icmsg.h"          // for ICMsg
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrStats
#include "mshr.h"           // for MissHandlingEntry
#include "sim.h"
#include "memory/backing_store.h" // for GlobalBackingStoreType definition
#if 0
#define __DEBUG_ADDR 0x00000000
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#undef __DEBUG_CYCLE
#endif

#define DEBUG_WRITE_ACCESS_DUMP() \
    DEBUG_HEADER(); \
    fprintf(stderr, "[[ WRITE DATA ]] FIXME this info is BAD because it uses core not L1CachedID " \
                    "cluster: %d core: %4d addr: 0x%08x data: 0x%08x PC: 0x%08x, L2.set: %d L2.valid: %2x " \
                    "L2.IsPending() %1d L2.IsValidWord() %1d L2.IsValidLine() %1d L2.NotInterlocked() %1d " \
                    "L1D.IsPending() %1d L1D.IsValidWord() %1d L1D.IsValidLine() %1d " \
                    "L2.IsDirtyLine() %1d \n", \
                      cluster_num, core, addr, data, instr.get_currPC(), L2.get_set(addr), L2.getValidMask(addr),\
                      (L2.IsPending(addr) != NULL), L2.IsValidWord(addr), \
                      L2.IsValidLine(addr), L2.nonatomic_ccache_op_lock(tid, addr), (L1D[core].IsPending(addr) != NULL),\
                      L1D[core].IsValidWord(addr), L1D[core].IsValidLine(addr), \
                      L2.IsDirtyLine(addr));

#ifdef __DEBUG_ADDR
  #define DEBUG_WRITE_ACCESS(str) \
    if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) { \
      DEBUG_HEADER(); \
      fprintf(stderr, "[CACHEMODEL WRITE] %s  addr: 0x%08x core: %4d " \
                    "cluster: %d\n", str, addr, core, cluster_num);  \
    }
#else
  #define DEBUG_WRITE_ACCESS(str)
#endif

MemoryAccessStallReason
CacheModel::helper_return_write_success(bool &stall,
    MemoryAccessStallReason masr, uint32_t const &data, uint32_t addr,
    InstrLegacy &instr, int core, int tid)
{
  using namespace rigel;
  DEBUG_WRITE_ACCESS("Success.");
  int L1CacheID = GetL1DID(tid);

  // On success, we reset the arb complete bit for the thread so that the next
  // access must arbitrate.
  thread_arb_complete[tid] = false;

  // We can unlock the address for the core once we succeed.
  L2.nonatomic_ccache_op_unlock(tid, addr);

  // For store-conditional, we need to check the access against the pending
  // load-links.  If the load-linked is still valid, invalidate all of the
  // other pending load-linked's at the cluster and allow the
  // store-conditional to proceed.  Otherwise, store-condtional has failed.
  // The status register ($r1) will be set to one. 

  assert( instr.get_type() != I_NULL  &&
          "CacheModel::helper_return_write_success: NullInstr found when not expected!");

  if (I_STC == instr.get_type()) {
    bool success;
    // Update the STC checker for the cluster.
    store_conditional_check(instr, tid , addr, success);
    // Store-conditional failed!  In WB we will update the status register
    // for the failed STC ($r1)
    if (!success) {
      instr.set_bad_stc();
      // Save the number of attempts for profiling later.
      instr.stats.mem_polling_retry_attempts = memory_retry_watchdog_count[tid];
      // Reset WDT on successful read.
      memory_retry_watchdog_count[tid] = 0;
      // We succeeded, set the accessed bit.
      L2.set_accessed_bit_write(addr);
      L1D[L1CacheID].set_accessed_bit_write(addr);
      L1D[L1CacheID].set_temporary_access_complete(addr);
      return masr;
    }
  }

  // Update memory at this point making value visible to the world.
  backing_store.write_word(addr, data);

  // Save the number of attempts for profiling later.
  instr.stats.mem_polling_retry_attempts = memory_retry_watchdog_count[tid];
  // Reset WDT on successful read.
  memory_retry_watchdog_count[tid] = 0;
  // We succeeded, set the accessed bit.
  L2.set_accessed_bit_write(addr);
  L1D[L1CacheID].set_accessed_bit_write(addr);
  L1D[L1CacheID].set_temporary_access_complete(addr);
  L2.setDirtyWord(addr);
  return masr;
}

///////////////////////////////////////////////////////////////////////////////
/// write_access
///////////////////////////////////////////////////////////////////////////////
/// perform a write
MemoryAccessStallReason
CacheModel::write_access(InstrLegacy &instr, int core, uint32_t addr, const
  uint32_t &data, bool &stall, bool &l3d_hit, 
  bool &l2d_hit, bool &l1d_hit, int cluster, int tid)
{
  using namespace rigel::cache;
  using namespace rigel;

  int L1CacheID = GetL1DID(tid);

  // Idealized RTM D$ accesses.
  bool is_freelibpar =    (instr.get_currPC() > CODEPAGE_LIBPAR_BEGIN 
                        && instr.get_currPC() < CODEPAGE_LIBPAR_END 
                        && CMDLINE_ENABLE_FREE_LIBPAR) ? true : false;
  

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_WRITE_ACCESS_DUMP();
  }
  #endif

  MemoryAccessStallReason masr = MemoryAccessStallReasonBug;

  // Check the WDT first for the access.
  if (memory_retry_watchdog_count[tid]++ > CACHEBASE_MEMACCESS_ATTEMPTS_WDT ) {
    rigel::GLOBAL_debug_sim.dump_all();
    fprintf(stderr, "\n---------------------------------------------\n");
    DEBUG_HEADER();
    fprintf(stderr, "[[ CacheModel::write_access() WDT ]] Retry count exceeded (%d)\n",
                    memory_retry_watchdog_count[tid]);
    DEBUG_WRITE_ACCESS_DUMP();
    assert(0);
  }

  // For profiling.  Returned to the pipeline model.
  l1d_hit = L1D[L1CacheID].IsValidWord(addr) || PERFECT_L1D || is_freelibpar;
  l2d_hit = L2.IsValidWord(addr) || PERFECT_L2D;
  // Only used for approximating global cache perf.  It is slightly inaccurate.
  l3d_hit = (GLOBAL_CACHE_PTR[AddressMapping::GetGCacheBank(addr)]->IsValidLine(addr));

  // Set the start time for the operation.  The method is idempodent.  Note that
  // we wait until this request is not going to stall to start the timer that
  // way we do not accumulate the time for a long-latency op in front of us too.
  instr.set_first_memaccess_cycle();

  // Used for L2Cache pend() calls.
  icmsg_type_t icmsg_type = rigel::instr_to_icmsg(instr.get_type());

  // If the line is currently interlocked by another core, we have to stall.
  // TODO: We probably want to move the interlock below L1D hits and the
  // arbitrate cycle.  Right now contended writes to the cluster cache are not
  // pipelined on the bus.  The effect should be small, however.  Proper write
  // buffering would correct the problem. 
  if (!L2.nonatomic_ccache_op_lock(tid, addr)) {
    stall = true;
    DEBUG_WRITE_ACCESS("Interlock");
    return L2DAccessBitInterlock;
  }

  // Check if the address is tracked as incoherent.
  bool incoherent = L2.get_incoherent(addr);

  // In the case that we have read permission to a line but want to write it,
  // for safety's sake, we invalidate it locally then reissue a write miss.  It
  // slows performance marginally for a very uncommon case while accurately
  // modeling the write miss latency.  A conservative hardware implementation
  // would be similar.
  //
  // For better accuracy, this should probably be moved on the other side of the
  // arbitration. 
  if (ENABLE_EXPERIMENTAL_DIRECTORY && !incoherent)
  {
    if (L1D[L1CacheID].IsValidWord(addr)) {
      if (!L1D[L1CacheID].IsDirtyLine(addr)) {
        if (!L2.IsDirtyLine(addr) && L2.get_accessed_bit(addr)) {
          if (!L1D[L1CacheID].get_temporary_access_pending()) {
            // If the line is present, clean in the L1 and L2 and it is not a
            // pending temporary access, we invalidate the line in the L1D for
            // consistency.  This access is going to generate a write miss at
            // the L2 anyway.
            L1D[L1CacheID].invalidate_line(addr);
          }
        }
      }
    }

    if (L2.IsValidWord(addr)) {
      // If it is already pending, this request stalls.
      MissHandlingEntry<rigel::cache::LINESIZE> *mshr = L2.IsPending(addr);
      if (mshr != NULL) {
        mshr->AddCore(core);
        return helper_return_write_stall(stall, L2DPending);
      }

      // Overloading dirty bits to check if it has been written by this cluster.
      // Really we would probably want to keep separate L2 metadata.
      if (!L2.IsDirtyLine(addr) && L2.get_accessed_bit(addr)) {
        // If we run out of L2 MSHRs, stall. Bug fix #166. 
        if (L2.mshr_full(false)) { return helper_return_write_stall(stall, L2DMSHR); }

        CacheAccess_t ca(
          addr & ~0x1F,             // address
          core,                     // core number
          tid,                      // thread id
          IC_MSG_CC_RD_RELEASE_REQ, // interconnect message type
          rigel::NullInstr        // no instruction correlated to this access
        );

        // Writebacks are counted for a fake core.
        // On an invalidate, we need to sync back up with the directory and
        // stall the write for a cycle.   While the release is in flight, the
        // write cannot move forward since it will be blocked by the pending
        // release.
        L2.pend(ca, true /* is eviction! */);

        // Note that we are trying to write a line that is already in the shared
        // state.  To avoid thrashing by other reads, we invalidate the line,
        // set the read-to-write pending bit, and then when the reply returns
        // from the directory, we immediately send a WRITE request.
        L2.set_read_to_write_upgrade_pending(addr);
        L2.invalidate_line(addr);
        return helper_return_write_stall(stall, L2DCoherenceStall);
      }
    }
  }


  /////////////////////////////////////////////////////////////////////////////
  // ****************** L1 Write Hit **********************
  /////////////////////////////////////////////////////////////////////////////
  if( L1D_ENABLED ) {
    // If we have the value in our L1, we can just update it there and move
    // along.  If there are multiple lines in the L1D, this could be dicey
    // depending on what consistency model we end up supporting.
    if ( L1D[L1CacheID].IsValidWord(addr) || PERFECT_L1D || is_freelibpar) {
      if (ENABLE_EXPERIMENTAL_DIRECTORY && !incoherent) {
        // As a fail safe, if we find something in the L1D that is not in the L2D,
        // we invalidate it on a write upgrade and let it try again the next
        // cycle.  When something is evicted from the L2, EvictDown() should
        // clear this out, but for some reasons things can go awry.  maybe it is
        // a stale MSHR or something.  Nonetheless, this makes sure that you
        // have inclusion across L1/L2 and that proper coherence guarantees are
        // maintained.  
        if (!L2.IsValidWord(addr) || !L2.IsDirtyLine(addr)) {
          L1D[L1CacheID].invalidate_line(addr);
          return helper_return_write_stall(stall, L2DCoherenceStall);
        }

        DEBUG_WRITE_ACCESS("L1D Write hit.");
        if (!L2.IsValidLine(addr)
              || (L2.IsValidLine(addr) && !L2.IsDirtyLine(addr))) {
          DEBUG_HEADER();
          fprintf(stderr, "addr: %08x L2dirty: %1d L2Valid: %1d\n", addr,
            L2.IsDirtyLine(addr), L2.IsValidLine(addr));
          DEBUG_HEADER();
          fprintf(stderr, "Found a line that is going to hit in the L1D but is not"
                          "valid in L2D or is not Dirty!\n");
          assert(0);
        }
      }

      // FIXME: We incur no cost for setting written lines dirty in the L2.

      // This could be implemented with a separate address dirty bus.  The
      // overhead is pretty minimal to add that and the difference in
      // performance is probably negligable.

      // Note that if we are stuck serializing at the cluster cache anyway, the
      // utility of the second bus might be low.
      // The setDirty() call touch()'s line so no need to touch on write for LRU
      // status updates.  We dirty the lines and move along. 
      L1D[L1CacheID].setDirtyWord(addr);
      L2.setDirtyWord(addr);

      return helper_return_write_success(stall, NoStall, data, addr, instr, core, tid);
    }
  }

  // We have missed in the L1D (or one does not exist) so we must arbitrate for
  // the L2D.  
  if (!helper_arbitrate_L2D_access(addr, core, tid, L2_WRITE)) {
    DEBUG_WRITE_ACCESS("[STALL] Lost Arbitration for L2D");
    // Lost arbitration, stall.
    return helper_return_write_stall(stall, L2DArbitrate);
  }

  // We assume that for write accesses, the line is automatically dirty at the
  // fill.  Also note that it seems that we are write-no-allocate at the L1D for
  // stores. 

  // FIXME We should decide if we are write-allocate or write-no-allocate at both the L1D
  // and the L2D, or implement both, but do it consistently.
  //
  // fixed Fill() so that writes are filled at the cluster
  // cache and then their dirty bits are set.  This will make sure that proper
  // writeback traffic occurs and that coherence does not do a free
  // read-to-write upgrade.  A better way to solve this may be to add permission
  // bits to the tags or some other form of notification at the C$ tags allowing
  // disambiguation of lines that are valid for reading and lines that are valid
  // for writing.
  // This if condition says that the word needs to be valid, and if we are coherent
  // we must have the line in modified state (i.e. at least one of the dirty bits in the line
  // must be set).  If we are SWcc, we don't need the line to be dirty beforehand.
  if ( (L2.IsValidWord(addr) && !(rigel::ENABLE_EXPERIMENTAL_DIRECTORY && !L2.IsDirtyLine(addr)))  || PERFECT_L2D ) {
    /////////////////////////////////////////////////////////////////////////////
    // ********************* L2 Write Hit **********************
    /////////////////////////////////////////////////////////////////////////////

    // We have the cluster bus and the line is already resident in the L2D so we
    // just set it dirty and return.  
    L2.setDirtyWord(addr);
    L2.set_accessed_bit_write(addr);

    return helper_return_write_success(stall, NoStall, data, addr, instr, core, tid);
  } else {
    /////////////////////////////////////////////////////////////////////////////
    // ******************** L2 Write Miss ***********************
    /////////////////////////////////////////////////////////////////////////////

    // Write-Allocate the L2/Cluster Cache on a miss
    if (!ENABLE_L2D_WRITEALLOCATE)
    {
      // Without write allocate, we can either keep the dirty data at the core
      // in the L1D until we evict it at the core and writeback to the L2D, or
      // we can bypass the L1D and put it directly into the cluster cache to
      // tile network queue.  We would probably add a write buffer here if we
      // did the latter.  Doing it for "free" (or at least on the cheap since we
      // already did arbitrate for the L2 bus) here is not a bad approximation.
      
      DEBUG_WRITE_ACCESS("L2D Write No Allocate");
      // With coherence enabled, this will not work.  We can
      // still be write no-allocate, but that will require obtaining coherence
      // first and then doing the update and then flushing it back to the global
      // cache.  Icky.
      assert(0 && "Write no-allocate is busticated.");

      return helper_return_write_success(stall, NoStall, data, addr, instr, core, tid);
    } else {
      // Fill this address now to write-allocate.  It should not be valid at
      // this point and we have access to the L2 bus.  
      //FIXME We might need to make sure we are setting the appropriate valid and dirty bits in the L2.
      MissHandlingEntry<rigel::cache::LINESIZE> *mshr = L2.IsPending(addr);
      bool full = L2.mshr_full(false); //not an eviction
      if( (mshr == NULL) && (!full) && (!L2.is_being_evicted(addr)) ) {

        CacheAccess_t ca(
          addr,                    // address
          core,                    // core number
          tid,                     // thread id
          icmsg_type,              // interconnect message type
          &instr                   // no instruction correlated to this access
        );

        // Writebacks are assigned a fake corenum.

        DEBUG_WRITE_ACCESS("[PEND] L2D Write Allocate");
        // Try to fill, no stall on success.
        L2.pend(ca, false);

        if (ENABLE_EXPERIMENTAL_DIRECTORY && !incoherent) {
          DEBUG_WRITE_ACCESS("[STALL] Coherence write miss.");
          return helper_return_write_stall(stall, L2DPending);
        } else {
          return helper_return_write_success(stall, NoStall, data, addr, instr, core, tid);
        }

      } else {
        DEBUG_WRITE_ACCESS("[STALL] L2D Write Allocate");

        // Handle stall reason updates.
        if (full) {
          return helper_return_write_stall(stall, L2DMSHR);
        } else if (mshr != NULL) {
          mshr->AddCore(core);
          return helper_return_write_stall(stall, L2DPending);
        }
      }
    }
  }
  assert(0 && "We should never reach this point in the code.  It should return above.");
  return helper_return_write_stall(stall, masr);
}
// end write_access
///////////////////////////////////////////////////////////////////////////////
