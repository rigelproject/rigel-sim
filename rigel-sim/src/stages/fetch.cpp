
////////////////////////////////////////////////////////////////////////////////
// fetch.cpp (fetch stage)
////////////////////////////////////////////////////////////////////////////////
//
// implementations for the fetch stage
//
// the fetch stage update and the fetch method live here 
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include "core/btb.h"            // for BranchTargetBufferBase
#include "caches_legacy/cache_model.h"    // for CacheModel
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy, etc
#include "define.h"         // for InstrSlot, etc
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrStats, InstrCacheStats, etc
#include "locality_tracker.h"  // for LocalityTracker
#include "profile/profile.h"        // for InstrStat, Profile, etc
#include "profile/profile_names.h"  // for ::STATNAME_BRANCH_TARGET
#include "sim.h"            // for NullInstr, etc
#include "util/ui_legacy.h"             // for UserInterfaceLegacy
#include "stage_base.h"  // for FetchStage

////////////////////////////////////////////////////////////////////////////////
// Method Name: FetchStage::Update()
////////////////////////////////////////////////////////////////////////////////
// Description: called each cycle for the fetch stage
//  updates the fetch stage state for the cycle
//
// Inputs: none
//
// Outputs: none
//
// Side Effects: Loads the IF2DC nlatches with the fetched instructions. If Fetch
// stalled due to cache miss, NullInstr instructions are loaded, which signal to
// the decode stage to do nothing. 
//
// Updates statistics for Fetch stage operation: active, blocked, stall etc.
////////////////////////////////////////////////////////////////////////////////
void FetchStage::Update() 
{
  assert(rigel::ISSUE_WIDTH == 2 && "This will only work if the machine is two wide!");


  // Only perform fetch if both slots are currently empty
  // FIXME is there an optimization available here to allow I$ miss
  // to begin even if these latch slots are blocked?
  if ( (I_NULL == core->nlatches[IF2DC][0]->get_type()) 
    && (I_NULL == core->nlatches[IF2DC][1]->get_type()) ) 
  {
    core->get_fetch_thread_state()->waiting_to_fetch=false;
    // Both InstrSlots are sent to fetch to be filled in.
    fetch_2wide(core->nlatches[IF2DC][0], core->nlatches[IF2DC][1]);

    // Per-instruction profiling
    if (core->nlatches[IF2DC][0] != rigel::NullInstr)
      core->nlatches[IF2DC][0]->stats.cycles.instr_stall = core->fetch_stall_counter[0];
    if (core->nlatches[IF2DC][1] != rigel::NullInstr)
      core->nlatches[IF2DC][1]->stats.cycles.instr_stall = core->fetch_stall_counter[1];
  } else {
    // Any stall at IF counts as two blocked calls
    core->get_cluster()->getProfiler()->timing_stats.if_block += 2;
    core->get_fetch_thread_state()->waiting_to_fetch=true;
  }

  // FIXME should this be in CoreInOrderLegacy::PerCycle ?
  // only call ui for interactive mode on the core being traced, so that
  // break only happens on traced core.  Because instr only has PC but not
  // corenum, interactive mode used to break on any core that hit breakPC
  if (core->get_core_num_global() == rigel::INTERACTIVE_CORENUM){
    core->get_cluster()->getUserInterface()->run_interface(core->nlatches[IF2DC]);
  }

}

////////////////////////////////////////////////////////////////////////////////
// fetch_2wide() 
////////////////////////////////////////////////////////////////////////////////
// fetches instructions 64-bits at a time
// (this is the Rigel default fetch mode)
// Force fetch to align to 64-bits.  There is a loss of efficiency for branches
// to unaligned targets with this enabled.  The mode also enables static branch
// prediction.  This mode is the planned mode for Rigel.  
// NOTE: this was previously a CoreInOrderLegacy member function!
////////////////////////////////////////////////////////////////////////////////
void 
FetchStage::fetch_2wide(InstrSlot &slot_a, InstrSlot &slot_b) 
{
  using namespace rigel;
  InstrSlot ret_instr_a, ret_instr_b;
  // Stall signals from I$
  bool stall_a = false, stall_b = false;
  MemoryAccessStallReason masr_a = NoStall, masr_b = NoStall;
  // Addresses to access (really one access)
  uint32_t nextPC_addr_a = 0x0;
  uint32_t nextPC_addr_b = 0x0;
  // Instructions coming back from the I$
  uint32_t raw_instr_a, raw_instr_b;
  // Nullify first instr if unaligned access is needed, e.g., on a mispredict
  // fixup the branch target may be in slot_b so slot_a must be squashed
  bool unaligned_if_access = false;
  // Cleared when there is a BTR hit in the second fetch slot.
  bool aligned_btr_hit_pending = true;
  // Set when a btr hit is handled this cycle
  bool btr_hit_pending = false;
  // Used for profiling.  Set by read_access_instr and used to create the new
  // InstrLegacy instance.
  // Initialize to false because read_access_instr will not be called to set
  // l1*_hit_a on an unaligned access.
  bool l1i_hit_a=false, l2i_hit_a=false;
  bool l1i_hit_b=false, l2i_hit_b=false;

  // HALTED
  if ( core->is_thread_halted(core->get_fetch_thread_id_core()) ) {
    core->get_cluster()->getProfiler()->timing_stats.halt_stall += 2;
    ret_instr_a = rigel::NullInstr;
    ret_instr_b = rigel::NullInstr;
  } 
  // NOT halted (proceed with fetch as normal)
  else { 
    #ifdef DEBUG_MT
    DEBUG_HEADER();
    cerr << "\nFETCH BEGIN Core " << core->get_core_num_global() <<" Thread " << core->get_fetch_thread_id_core() << std::endl;
    cerr << "Fetch PC 0x" << std::hex << core->get_fetch_thread_state()->IF2IF.NextPC << " Stalled=" << core->is_fetch_stalled() << std::endl;
    #endif
    // Watchdog: Reset on any retirement
    assert(core->get_watchdog(core->get_fetch_thread_id_core()) > 0 && "Watchdog expired!");
    // Reset BTR Hit bit
    core->get_fetch_thread_state()->IF2IF.BTRHitPending = false;

    // Check if EX signalled a mispredict last cycle.  This can get called more
    // than once if I$ stalls a fixup and that is why the reset for the fixup
    // signal is not cleared until the end.
    if (core->get_fetch_thread_state()->EX2IF.FixupNeeded) 
    { 
      // Clear the fixup signal (basically an ACK to EX)
      core->get_fetch_thread_state()->EX2IF.FixupNeeded = false;
      // If there was an unaligned IF pending, it gets squashed and we process
      // the mispredict instead since the unaligned IF was spurious.
      core->get_fetch_thread_state()->IF2IF.UnalignedPending = false;
      const uint32_t target_PC    = core->get_fetch_thread_state()->EX2IF.FixupPC; 
      const uint32_t branch_addr  = core->get_fetch_thread_state()->EX2IF.MispredictPC;
      core->get_fetch_thread_state()->btb->set(branch_addr, target_PC);
      
      if ( (core->get_fetch_thread_state()->EX2IF.FixupPC & 0x07) != 0) 
      {
        // Handle unaligned branch target
        unaligned_if_access = true;
        // Signal to next cycle that slot A is a bubble.  The mispredict should
        // get handled this cycle, but we need to set IF2IF.UnalignedPending
        // here in case a stall occurs since the fixup block is only run once.
        core->get_fetch_thread_state()->IF2IF.UnalignedPending = true;

        nextPC_addr_a = 0xFFFFFFFF;
        nextPC_addr_b = core->get_fetch_thread_state()->EX2IF.FixupPC;
      } else {
        // Handle aligned branch target
        nextPC_addr_a = core->get_fetch_thread_state()->EX2IF.FixupPC;
        nextPC_addr_b = core->get_fetch_thread_state()->EX2IF.FixupPC + 4;
      }
      // Keep the fixup PC saved in the IF2IF NextPC latch
      core->get_fetch_thread_state()->IF2IF.NextPC = core->get_fetch_thread_state()->EX2IF.FixupPC;
    } else { 
      // No mispredict last cycle
      if (core->get_fetch_thread_state()->IF2IF.UnalignedPending) {
        unaligned_if_access = true;
        nextPC_addr_a = 0xFFFFFFFF;
        nextPC_addr_b = core->get_fetch_thread_state()->IF2IF.NextPC;
      } else {
        unaligned_if_access = false;
        nextPC_addr_a = core->get_fetch_thread_state()->IF2IF.NextPC;
        nextPC_addr_b = core->get_fetch_thread_state()->IF2IF.NextPC + 4;
      }
    }

    if (! ((unaligned_if_access || (nextPC_addr_a & 0x07) == 0)) ) {
      DEBUG_HEADER(); assert(0 && "Fetches must be 64-bit aligned! ");
    }

    // Access I$. If one stalls, both should as the access is cache-line aligned.
    // Nullify on unaligned access, slot_b should always be valid.  Do not even
    // hit I$ if the access is unaligned (this is sort of a dummy thing).
#ifdef DEBUG_CACHES
    DEBUG_HEADER();
    cerr << "FETCH Stage ICache access begin\n";
#endif
    if (!unaligned_if_access) {
      masr_a = core->get_cluster()->getCacheModel()->read_access_instr(
        core->get_core_num_local(), 
        nextPC_addr_a,
        raw_instr_a, 
        stall_a, 
        l2i_hit_a, l1i_hit_a,
        core->get_fetch_thread_id_cluster()
      );
    }

    masr_b = core->get_cluster()->getCacheModel()->read_access_instr(
      core->get_core_num_local(), 
      nextPC_addr_b, 
      raw_instr_b, 
      stall_b, 
      l2i_hit_b, l1i_hit_b,
      core->get_fetch_thread_id_cluster()
    );

    assert( (unaligned_if_access || (stall_a == stall_b)) && "Invalid stall. "
      "Both instruction words should be on same cache line.");

    if (stall_a || stall_b) 
    {
#ifdef DEBUG_CACHES
      cerr << "FETCH Stage ICache access stall, I$ miss\n";
#endif
      //remove req to fetch status while handling L1I miss
      if (masr_a==L2IAccess || masr_b==L2IAccess){
        core->get_fetch_thread_state()->req_to_fetch=false;
#ifdef DEBUG_MT
	DEBUG_HEADER();
	fprintf(stderr,"Clearing req_to_fetch for core %d thread %d\n",
	  core->get_core_num_local(),core->get_fetch_thread_id_core());
#endif
      }

      //for PIP (per-instruction profiling)
      if(nextPC_addr_a == core->fetch_stall_pc[0])
        core->fetch_stall_counter[0]++;
      else
      {
        core->fetch_stall_counter[0] = 1;
        core->fetch_stall_pc[0] = nextPC_addr_a;
      }
      if(nextPC_addr_b == core->fetch_stall_pc[1])
        core->fetch_stall_counter[1]++;
      else
      {
        core->fetch_stall_counter[1] = 1;
        core->fetch_stall_pc[1] = nextPC_addr_b;
      }

      core->get_cluster()->getProfiler()->timing_stats.if_stall += 2;
      ret_instr_a = rigel::NullInstr;
      ret_instr_b = rigel::NullInstr;
      // Set to notify profiler that the instruction being fetch stalled IF.
      core->get_fetch_thread_state()->IF2IF.profiler_l1i_stall_pending = !(l1i_hit_a || l1i_hit_b);
      // Do not let L2I fill get counted as an L2I hit while waiting on L1I.
      if (!core->get_fetch_thread_state()->IF2IF.profiler_l2i_stall_pending) {
        core->get_fetch_thread_state()->IF2IF.profiler_l2i_stall_pending = !(l2i_hit_a || l2i_hit_b);
      }
    } else {
      // Do locality tracking for this fetch, both per-thread and aggregate (per-core)
      // This should only be done once per load-or-store, so we put it *after* we have
      // ascertained that stage_stall is false and the fetch was successful.
      if (rigel::ENABLE_LOCALITY_TRACKING)
      {
        // Only track slot A if access was aligned
        if(!unaligned_if_access)
        {
          core->aggregateLocalityTracker->addAccess(nextPC_addr_a);
          core->perThreadLocalityTracker[core->get_fetch_thread_id_core()]->addAccess(nextPC_addr_a);
        }
        // Always track slot B
        core->aggregateLocalityTracker->addAccess(nextPC_addr_b);
        core->perThreadLocalityTracker[core->get_fetch_thread_id_core()]->addAccess(nextPC_addr_b);
      }

      //Do TLB tracking for this fetch, both per-thread and aggregate (per-core),
      //instruction-only and unified TLBs.
      //This should only be done once per load-or-store, so we put it *after* we have
      //ascertained that stage_stall is false and the fetch was successful.
      if(rigel::ENABLE_TLB_TRACKING)
      {
        //NOTE Disabling per-thread and per-core ITLB/UTLB because they don't tell us much for our benchmarks.
        //Our benchmarks don't use much code, so small ITLBs give you huge hit rates.
        //Also, since ITLBs get hit once or twice every cycle, they add a ton more sim time overhead than
        //DTLBs.
#if 0
        for(size_t i = 0; i < core->num_tlbs; i++) {
          if(!unaligned_if_access) {
            core->aggregateITLB[i]->access(nextPC_addr_a);
            core->aggregateUTLB[i]->access(nextPC_addr_a);
          }
          core->aggregateITLB[i]->access(nextPC_addr_b);
          core->aggregateUTLB[i]->access(nextPC_addr_b);
          if(rigel::THREADS_PER_CORE > 1) {
            if(!unaligned_if_access) {
              core->perThreadITLB[core->get_fetch_thread_id_core()][i]->access(nextPC_addr_a);
              core->perThreadUTLB[core->get_fetch_thread_id_core()][i]->access(nextPC_addr_a);
            }
            core->perThreadITLB[core->get_fetch_thread_id_core()][i]->access(nextPC_addr_b);
            core->perThreadUTLB[core->get_fetch_thread_id_core()][i]->access(nextPC_addr_b);
          }
        }
#endif
      }

      // Handle BTB.  We need to check if the next fetch packet contains a BTR
      // entry in either slot.  Since IF2IF.NextPC may not be aligned, we need
      // to do some ugly math to make sure we do not skip to the next fetch
      // packet while also making sure we do not match a PC that has already
      // passed.
      const uint32_t nextPC =  core->get_fetch_thread_state()->IF2IF.NextPC;
      if ( (core->get_fetch_thread_state()->btb->hit(nextPC) 
              || core->get_fetch_thread_state()->btb->hit(nextPC+4)) && !unaligned_if_access ) 
      {
        // BTB Hit.  Signal to next cycle (not used currently) and keep for this
        // cycle to perform correct predictions for next cycle.
        core->get_fetch_thread_state()->IF2IF.BTRHitPending = true;
        btr_hit_pending = true;
        profiler::stats[STATNAME_BRANCH_TARGET].inc_pred_hits();
        // If the branch is in the first slot (slot A), we need to send a bubble
        // down next to it in the second slot so that it does not mispredict.  A
        // 'non-speculative' bit could probably accomplish the same task.
        uint32_t btb_target_PC;
        if (core->get_fetch_thread_state()->btb->hit(nextPC)) { 
          btb_target_PC = core->get_fetch_thread_state()->btb->access(nextPC); 
          aligned_btr_hit_pending =  true;
        } else { 
          btb_target_PC = core->get_fetch_thread_state()->btb->access(nextPC+4); 
          aligned_btr_hit_pending =  false;
        }

        // Hit in the branch target register, use the branch target PC.
        if ((btb_target_PC  & 0x07) == 0)
        {
          // Do aligned branch target.  Next cycle get the full fetch packet.
          core->get_fetch_thread_state()->IF2IF.NextPC = btb_target_PC;
          // BTR Hit with an aligned target. 
          core->get_fetch_thread_state()->IF2IF.UnalignedPending = false;
        } else {
          // This gets squashed by EX if a real mispredict occurs.
          core->get_fetch_thread_state()->IF2IF.NextPC = btb_target_PC;
          // BTR Hit with an *UN*aligned branch target.
          core->get_fetch_thread_state()->IF2IF.UnalignedPending = true;
        }
      } else { // No BTR hit
        // Fetch the next instruction after slot_b by default.  This works for
        // both PC+4 normal fetches and for fixup fetches.
        core->get_fetch_thread_state()->IF2IF.NextPC = nextPC_addr_b + 4;
        // Once we are here, there is no pending mispredict fixup so we can clear
        // the unaligned flag.
        core->get_fetch_thread_state()->IF2IF.UnalignedPending = false;
      }
  
      // Unaligned accesses put a bubble in the A pipe.  B pipe is always valid.
      if (unaligned_if_access) {
        ret_instr_a = rigel::NullInstr;
      } else {
        if (aligned_btr_hit_pending && btr_hit_pending) {
          // If the first slot is a BTR hit, the next PC is not the second slot
          // but instead the predicted PC (IF2IF.NextPC).
          ret_instr_a = new InstrLegacy(nextPC_addr_a,
            core->get_fetch_thread_id_global(),
            core->get_fetch_thread_state()->IF2IF.NextPC,
            raw_instr_a, I_PRE_DECODE);
        } else {
          // No BTR hit so the next instruction is slot B.
          ret_instr_a = new InstrLegacy(nextPC_addr_a,
            core->get_fetch_thread_id_global(), nextPC_addr_a+4, 
            raw_instr_a, I_PRE_DECODE);
        }
        ret_instr_a->stats.cache.set_l1i_hit(l1i_hit_a &&
          !core->get_fetch_thread_state()->IF2IF.profiler_l1i_stall_pending);
        ret_instr_a->stats.cache.set_l2i_hit(l2i_hit_a &&
          !core->get_fetch_thread_state()->IF2IF.profiler_l2i_stall_pending);
      }
      // BTR was a hit and it happened to be a branch in the first slot so the
      // second slot is not executed.  Send a bubble down after the branch to
      // avoid commiting garbage or triggering a mispredict.
      if (aligned_btr_hit_pending && btr_hit_pending) {
        ret_instr_b = rigel::NullInstr;
      } else {
        ret_instr_b = new InstrLegacy(nextPC_addr_b,
          core->get_fetch_thread_id_global(),
          core->get_fetch_thread_state()->IF2IF.NextPC,
          raw_instr_b, I_PRE_DECODE);
        ret_instr_b->stats.cache.set_l1i_hit(l1i_hit_b &&
          !core->get_fetch_thread_state()->IF2IF.profiler_l1i_stall_pending);
        ret_instr_b->stats.cache.set_l2i_hit(l2i_hit_b &&
          !core->get_fetch_thread_state()->IF2IF.profiler_l2i_stall_pending);
      }

      // 1. Track the last instruction number to allow for serialization
      // 2. Add to speculative instruction list so that they can be flushed on
      // future mispredicted branches if needed.
      if (!unaligned_if_access) {
        ret_instr_a->SetLastIntrOnCore(core->get_last_fetch_instr_num());
        core->set_last_fetch_instr_num(ret_instr_a->GetInstrNumber());
        core->AddSpecInstr(ret_instr_a);
        ret_instr_a->set_pipe(0);
      }
      // If there is an aligned BTR hit pending, the B instr is a bubble and
      // should not be added to the speculative list.
      if (!(aligned_btr_hit_pending && btr_hit_pending)) {
        ret_instr_b->SetLastIntrOnCore(core->get_last_fetch_instr_num());
        core->set_last_fetch_instr_num(ret_instr_b->GetInstrNumber());
        core->AddSpecInstr(ret_instr_b);
      }
      ret_instr_b->set_pipe(1);

      core->get_cluster()->getProfiler()->timing_stats.if_active += 2;
     
      // Done with non-stalling fetch. Clear profiler counters on hit.
      core->get_fetch_thread_state()->IF2IF.profiler_l1i_stall_pending = false;
      core->get_fetch_thread_state()->IF2IF.profiler_l2i_stall_pending = false;
    }
  }

  // Set return variables
  slot_a = ret_instr_a;
  slot_b = ret_instr_b;
}
/* CoreInOrderLegacy::fetch_2wide() [END] */


