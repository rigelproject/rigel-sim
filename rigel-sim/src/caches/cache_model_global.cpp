////////////////////////////////////////////////////////////////////////////////
/// src/caches/cache_model_global.cpp
////////////////////////////////////////////////////////////////////////////////
///
///  This file contains the includes global memory operation interaction with
///  core model.
///
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include "cache_model.h"    // for CacheModel
#include "caches/cache_base.h"  // for CacheAccess_t
#include "caches/hybrid_directory.h"  // for HybridDirectory, etc
#include "caches/l2d.h"     // for L2Cache
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy, etc
#include "define.h"         // for MemoryAccessStallReason, etc
#include "icmsg.h"          // for ICMsg
#include "instr.h"          // for InstrLegacy
#include "profile/profile.h"        // for CacheStat, Profile, etc
#include "core/regfile_legacy.h"        // for RegisterBase, etc
#include "sim.h"            // for MemoryModelType, etc
#include "memory/backing_store.h" //for GlobalBackingStoreType definition

///////////////////////////////////////////////////////////////////////////////
///   global_memory_access
///////////////////////////////////////////////////////////////////////////////
MemoryAccessStallReason
CacheModel::global_memory_access(
  const CacheAccess_t ca_in, uint32_t &data, bool &stall, int cluster)
{
  using namespace rigel;
  // Idealized RTM D$ accesses.
  bool is_freelibpar =    (ca_in.get_instr()->get_currPC() > CODEPAGE_LIBPAR_BEGIN 
                        && ca_in.get_instr()->get_currPC() < CODEPAGE_LIBPAR_END 
                        && CMDLINE_ENABLE_FREE_LIBPAR) ? true : false;
  
  // If the line is currently interlocked by another core, we have to stall.
  if (!L2.nonatomic_ccache_op_lock(ca_in.get_tid(), ca_in.get_addr())) {
    stall = true;
    return L2DAccessBitInterlock;
  }

  // Global memory accesses bypass the cluster cache and complete at the global
  // cache in RigelSim.  The property that only one global operations may be
  // outstanding from any core at any time is used to implement global
  // operations.  Each core has a bit in a bit vector signalling if it has a
  // completed global memory operations pending.  If a core finds that it has a
  // global operation completed this cycle, it returns the value to the
  // requesting core.  If it does not have one pending, a new request is created
  // and then the core pipeline model begins polling until the bit is set in the
  // completed bit vector by the CCache controller
  icmsg_type_t icmsg_type = rigel::instr_to_icmsg( ca_in.get_instr()->get_type());

  // For profiling, we track the stall reason and return it to the core model.
  MemoryAccessStallReason masr = MemoryAccessStallReasonBug;

  // The cluster cache has signalled that the global operation pending has been
  // completed.
  if (L2.GlobalMemOpCompleted[ca_in.get_tid()] == true || rigel::cache::PERFECT_GLOBAL_OPS 
        || is_freelibpar)
  {
    // Clear the completed flag and return the value to the core.
    L2.GlobalMemOpCompleted[ ca_in.get_tid() ] = false;
    assert(L2.GlobalMemOpOutstanding[ ca_in.get_tid() ] == false &&
            "The CCache controller should have already cleared the bit.");
    // Since the global operation may not complete this cycle, stall.
    stall = false;

    // If the global operation has completed, service it.  We piggyback perfect
    // global ops/atomics here too.
    if ((L2.GlobalLoadForCoreAtomicOutstanding[ca_in.get_tid()] == false)
        || rigel::cache::PERFECT_GLOBAL_OPS || is_freelibpar)
    {
      // Do RMW here to remain atomic.  This should be done at GCache
      // and sent back with the message, but this is much easier to implement
      // and is close to correct in terms of timing.  It is not cycle-equivalent
      // per se, but it is roughly cycle-accurate.
      switch (ca_in.get_instr()->get_type())
      {
        // Global atomic increment.  Take value, add one. (atom.inc)
        case I_ATOMINC:
        {
          data = backing_store.read_word(ca_in.get_addr());
          data++;
          backing_store.write_word(ca_in.get_addr(), data);
          break;
        }
        // Global atomic decrement.  Take value, subtract one. (atom.dec)
        case I_ATOMDEC:
        {
          data = backing_store.read_word(ca_in.get_addr());
          data--;
          backing_store.write_word(ca_in.get_addr(), data);
          break;
        }
        // Global atomic addition, unsigned. Return the new value.  (atom.addu)
        case I_ATOMADDU:
        {
          uint32_t mem_data;
          //  We could change this back to rB+imm16, rA where EA = (rB)+imm16
          //  and the value to increment is (rA)
          uint32_t addu_data = ca_in.get_instr()->get_sreg1().data.i32;
          // uint32_t addu_dest = instr.get_sreg2().data.i32;
          uint32_t addu_addr = ca_in.get_instr()->get_sreg0().data.i32;

          // Grab old value out of memory.
          data = backing_store.read_word(addu_addr);
          // Do the atomic addition here.
          mem_data = data + addu_data;
          backing_store.write_word(addu_addr, mem_data);
          // Return the old value back to the destination register
          // NOTE: This used to return the new value. but I changed
          // the semantics to match the original fetch-and-add semantics
          // as defined by Herlihy.  This also matches the xadd instruction on
          // x86.
          // No code here because the old value is stored in 'data'.
          break;
        }
        // Global atomic compare-and-swap operation. (atom.cas)
        case I_ATOMCAS:
        {
          // mem_data is the word we read out of memory.  If the value has not
          // changed, we swap in the new value (swap_data).  The value to return
          // is what was in memory when the CAS got there. 
          uint32_t mem_data;
          uint32_t compare_data = ca_in.get_instr()->get_sreg1().data.i32;
          uint32_t swap_data    = ca_in.get_instr()->get_sreg2().data.i32;
          uint32_t cas_addr     = ca_in.get_instr()->get_sreg0().data.i32;

          mem_data = backing_store.read_word(cas_addr);
          if (mem_data == compare_data) {
            backing_store.write_word(cas_addr, swap_data);
          }
          // Always return the "old" value in memory
          data = mem_data;
          break;
        }
        // Global atomic exchange.  (atom.xchg)
        case I_ATOMXCHG:
        {
          // Grab value from memory and replace it with value in the source
          // register. 
          uint32_t mem_data = backing_store.read_word( ca_in.get_addr());
          backing_store.write_word( ca_in.get_addr(), data);
          data = mem_data;
          break;
        }
        // Global load word.  (g.ldw)
        case I_GLDW:
        {
          // At this point timing has already been handled by the CCache
          // controller so we just pluck the value out.
          // FIXME If we wanted to be a
          // bit more accurate, we would freeze the value when we hit the
          // GCache and then return that to the core.
          data = backing_store.read_word(ca_in.get_addr());
          break;
        }
        // Global store word. (g.stw)
        case I_GSTW:
        {
          // FIXME The same is true for global stores as is true for global loads, but
          // we would update early instead of read early. 
          backing_store.write_word(ca_in.get_addr(), data);
          // For hybrid coherence, we want to update the table if the address
          // falls into the region used by the hybrid coherence table.
          hybrid_directory.update_region_table(ca_in.get_addr(), data);
          break;
        }
        // Global atomic maximum.
        case I_ATOMMAX:
        {
          uint32_t mem_data;
          uint32_t op_data = ca_in.get_instr()->get_sreg1().data.i32;
          uint32_t op_addr = ca_in.get_instr()->get_sreg0().data.i32;
          // 1. READ
          mem_data = backing_store.read_word(op_addr);
          // 2. MODIFY
          mem_data = (op_data > mem_data) ? op_data : mem_data;
          // 3. WRITE
          backing_store.write_word(op_addr, mem_data);
          // Return the new value back to the destination register
          data = mem_data;
          break;
        }
        // Global atomic minimum.
        case I_ATOMMIN:
        {
          uint32_t mem_data;
          uint32_t op_data = ca_in.get_instr()->get_sreg1().data.i32;
          uint32_t op_addr = ca_in.get_instr()->get_sreg0().data.i32;
          // 1. READ
          mem_data = backing_store.read_word(op_addr);
          // 2. MODIFY
          mem_data = (op_data < mem_data) ? op_data : mem_data;
          // 3. WRITE
          backing_store.write_word(op_addr, mem_data);
          // Return the new value back to the destination register
          data = mem_data;
          break;
        }
        // Global atomic OR.
        case I_ATOMOR:
        {
          uint32_t mem_data;
          uint32_t op_data = ca_in.get_instr()->get_sreg1().data.i32;
          uint32_t op_addr = ca_in.get_instr()->get_sreg0().data.i32;
          // 1. READ
          mem_data = backing_store.read_word(op_addr);
          // 2. MODIFY
          mem_data = op_data | mem_data;
          // 3. WRITE
          backing_store.write_word(op_addr, mem_data);
          // Return the new value back to the destination register
          data = mem_data;
          // For hybrid coherence, we want to update the table if the address
          // falls into the region used by the hybrid coherence table.
          hybrid_directory.update_region_table(op_addr, data);
          break;
        }
        // Global atomic AND.
        case I_ATOMAND:
        {
          uint32_t mem_data;
          uint32_t op_data = ca_in.get_instr()->get_sreg1().data.i32;
          uint32_t op_addr = ca_in.get_instr()->get_sreg0().data.i32;
          // 1. READ
          mem_data = backing_store.read_word(op_addr);
          // 2. MODIFY
          mem_data = op_data & mem_data;
          // 3. WRITE
          backing_store.write_word(op_addr, mem_data);
          // Return the new value back to the destination register
          data = mem_data;
          // For hybrid coherence, we want to update the table if the address
          // falls into the region used by the hybrid coherence table.
          hybrid_directory.update_region_table(op_addr, data);
          break;
        }
        // Global atomic AND.
        case I_ATOMXOR:
        {
          uint32_t mem_data;
          uint32_t op_data = ca_in.get_instr()->get_sreg1().data.i32;
          uint32_t op_addr = ca_in.get_instr()->get_sreg0().data.i32;
          // 1. READ
          mem_data = backing_store.read_word(op_addr);
          // 2. MODIFY
          mem_data = op_data ^ mem_data;
          // 3. WRITE
          backing_store.write_word(op_addr, mem_data);
          // Return the new value back to the destination register
          data = mem_data;
          // For hybrid coherence, we want to update the table if the address
          // falls into the region used by the hybrid coherence table.
          hybrid_directory.update_region_table(op_addr, data);
          break;
        }
        default:
        {
          DEBUG_HEADER();
          assert(0 && "Unhandled case!");
        }
      }

      // We can unlock the address for the core once we succeed.
      L2.nonatomic_ccache_op_unlock(ca_in.get_tid(), ca_in.get_addr());
      // Unlock arbitration.
      thread_arb_complete[ca_in.get_tid()] = true;
      stall = false;
      masr = NoStall;
      return masr;
    }
    // If we are doing atomic operations at the core instead of at the global
    // cache, we drop through here and issue another request to complete the
    // request that was just serviced.  
  }

  // If a global operation is already outstanding, try again later.  This would
  // get set on the first time a global access is attempted by the core and it
  // would keep coming through until the GlobalMemOpCompleted[core] bit is set.
  // We do some extra checks in here that we may or may not do depending on the
  // semantics of global operations in the final design. 
  if(L2.GlobalMemOpOutstanding[ca_in.get_tid()]) {
    // There is already a global memory operation outstanding and
    // it has not completed yet.  We stall the thread and have it try back again
    // next cycle.  Eventually the completed flag will be set by the message
    // returning from the global cache, but until then we wait. 
    Profile::accum_cache_stats.L2D_pending_misses++;
    masr = StuckBehindGlobal;
  } else {
    // Try to access the bus. 
    if (!helper_arbitrate_L2D_access(ca_in.get_addr(), ca_in.get_core_id(), ca_in.get_tid(), L2_READ_CMD))
    {
      stall = true;
      return L2DArbitrate;
    }
    // We check to see if the value is pending for a local memory operation or
    // if a previous atomic is already in flight for the address.  If so, we
    // stall and try again to maintain proper ordering.
    //    if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY)
    //    {
    //      MissHandlingEntry<rigel::cache::LINESIZE> *mshr = L2.IsPending(ca_in.get_addr());
    //      if (mshr != NULL) {
    //        mshr->AddCore(ca_in.get_core_id());
    //        stall = true;
    //        masr = L2DPending;
    //        return masr;
    //      }
    //    } else {
    //      MissHandlingEntry<rigel::cache::LINESIZE> *mshr = L2.IsPending(ca_in.get_addr());
    //      if ((mshr != NULL) && !L2.IsPendingNoCCFill(ca_in.get_addr())) {
    //        if(mshr->_ic_msg_type == IC_MSG_PREFETCH_BLOCK_GC_REQ)
    //          printf("GLOBAL IS WAITING ON BULK PF\n");
    //        mshr->AddCore(ca_in.get_core_id());
    //        stall = true;
    //        masr = L2DPending;
    //        return masr;
    //      }
    //    }
    //
    // We could turn this on or off.  Global operations "bypass" the cluster
    // cache and could either invalidate on the way up (which is probably safer)
    // or they could ignore the tag look up (which is probably more efficient).
    // As far as the simulation is concerned, invalidated lines being used for
    // atomics guarantees that we model timing correctly if we update something
    // with an atomic then read it on the same core without inserting the proper
    // synchronization.  In a real implementation, we would get the wrong
    // answer very fast.  In simulation we get both the right answer and we get
    // the right timing.
    //
    // For coherence, we do not want to invalidate the line since that breaks
    // the protocol (we drop a write on the floor).  At this point we do not
    // want the globals to participate in the coherence protocol, so the easiest
    // thing to do is just keep these two separate.  The global operation just
    // bypasses/updates the value in the cluster cache and when an eviction is
    // necessary, the cluster cache writes the line back to the global cache and
    // the directory is updated.
    //
    // Note that without coherence, we *must* invalidate the line and then try
    // to pend in this cycle.  Otherwise, local operations can continually
    // starve a global operation as the global op will just invalidate the word
    // and then stall, in the same cycle another request will pend, and in the
    // next cycle the global op will stall again...ad infinitum.
    if(rigel::ENABLE_EXPERIMENTAL_DIRECTORY && L2.IsValidWord(ca_in.get_addr())) {
      if (L2.IsValidWord(ca_in.get_addr())) {
        if (L2.mshr_full(false)) {
          stall = true;
          masr = L2DMSHR;
          return masr;
        }
        
        // Check data back in with the global cache directory before proceding
        // with global operation.
        icmsg_type_t msg_type = (L2.IsDirtyLine(ca_in.get_addr())) ?
        IC_MSG_EVICT_REQ : IC_MSG_CC_RD_RELEASE_REQ;
        
        CacheAccess_t ca_out = ca_in;      // same as input ca
        ca_out.set_icmsg_type( msg_type ); // new icmsg_type
        // Pend the request at the cluster cache.
        L2.pend(ca_out, true /* is eviction! */ );
        // Without coherence, we can simply drop the line.  With coherence, we
        // still need to invalidate.
        L2.invalidate_line(ca_in.get_addr());
        stall = true;
        masr = L2DPending;
        return masr;
      }
    }
    if(!rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
      if (L2.IsValidWord(ca_in.get_addr()))
        L2.invalidate_word(ca_in.get_addr());
    }

    // If we have not run out of MSHR's, grab one and try to insert the request
    // to the cluster cache to complete the global operation at the global cache.
    if(L2.mshr_full(false)) { //Not an eviction
      //Unable to snag an MSHR, try again next cycle
      Profile::accum_cache_stats.L2D_MSHR_stall_cycles++;
      masr = L2DMSHR;
    } else {
      CacheAccess_t ca_out = ca_in;        // copy input parameters
      ca_out.set_icmsg_type(icmsg_type); // update icmsg_type
      // If this is a non-blocking atomic, let pend() know.
      bool nonblocking_atomic = this->cluster->GetCore(ca_in.get_core_id()).get_thread_state(
      ca_in.get_instr()->get_core_thread_id())->nonblocking_atomics_enabled;
      if (nonblocking_atomic)
      {
        ca_out.set_nonblocking_atomic();
      }
      // Pend the request.  If it is an atomic, the atomic will occur at the global cache.
      // We fake it above by doing it when it returns to a later invocation of this method.
      L2.pend(ca_out, false);

      // The operation is pending so the core will begin polling on the
      // completed flag until the atomic is returned from the GCache.
      L2.GlobalMemOpOutstanding[ca_in.get_tid()] = true;

      assert((L2.GlobalMemOpCompleted[ca_in.get_tid()] == false) &&
        "Pending an global operation.  It cannot already be completed!");

      // For nonblocking atomics, we want to allow the instruction to proceed in
      // the next cycle.
      if (nonblocking_atomic) {
        L2.GlobalMemOpCompleted[ca_in.get_tid()] = true;
        L2.GlobalMemOpOutstanding[ca_in.get_tid()] = false;
      }
      masr = L2DPending;
    }
  }
  stall = true;
  return masr;
}
