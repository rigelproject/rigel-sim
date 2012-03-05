////////////////////////////////////////////////////////////////////////////////
// ccaccess.cpp (Cluster Cache access stage)
////////////////////////////////////////////////////////////////////////////////
//
// This file contains the cluster cache access stage.  When a load misses in the
// line buffer, it will not block in the MEM stage and filter down to here.  If
// the value is back from the cluster cache, it does not lock.  Otherwise, the
// cluster cache will lockup here until the load completes.
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, stderr
#include <list>                         // for list, _List_iterator, etc
#include "cache_model.h"    // for CacheModel
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy, ::CC2WB, ::FP2CC
#include "define.h"         // for InstrSlot, DEBUG_HEADER, etc
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrStats, InstrCycleStats, etc
#include "profile/profile.h"        // for Profile, InstrStat, etc
#include "profile/profile_names.h"  // for ::STATNAME_L2_ARB_TIME, etc
#include "core/regfile_legacy.h"        // for RegisterBase, etc
#include "core/scoreboard.h"     // for ScoreBoard
#include "sim.h"            // for ISSUE_WIDTH, etc
#include "stage_base.h"  // for CCacheAccessStage
#include "memory/backing_store.h"

////////////////////////////////////////////////////////////////////////////////
// Method Name: CCachAccessStage::Update()
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void
CCacheAccessStage::Update()
{
  //Unstall all
  for(int i = 0; i < rigel::ISSUE_WIDTH; i++)
    unstall(i);
  
  bool oldSuccess = false;
  if(rigel::NONBLOCKING_MEM)
  {
    rigel::profiler::stats[STATNAME_NBL_QUEUE_LENGTH].inc_mem_histogram(core->memoryOps.size(), 1);
    //if(core->memoryOps.size() > 0)
    //  fprintf(stderr, "%"PRIu64" Core %d has %zu\n", rigel::CURR_CYCLE, core->get_core_num_global(), core->memoryOps.size());
    //if(core->memoryOps.size() > 0)
    //  fprintf(stderr, "Cycle %"PRIu64" Core %d, dumping memoryOps:\n", rigel::CURR_CYCLE, core->get_core_num_global());
    for(std::list<InstrSlot>::iterator it = core->memoryOps.begin(), end = core->memoryOps.end(); it != end; ++it)
    {
      //(*it)->dump();
      InstrSlot i = ccaccess(*it, rigel::ISSUE_WIDTH-1, true);
      if(i != rigel::NullInstr) //Succeeded
      {
        oldSuccess = true;
        core->nlatches[CC2WB][rigel::ISSUE_WIDTH-1] = i;
        it = core->memoryOps.erase(it);
          break;
        //return;
      }
    }
  }
  //If a blocked load/store from memoryOps completed, we insert it into pipe 1's latch, hold back the
  //instruction that is in pipe 1's FP2CC latch, and stall pipe 1 so that that held-back instruction
  //doesn't get scribbled over in CoreInOrderLegacy::UpdateLatches().
  for (int j = 0; j < (oldSuccess ? rigel::ISSUE_WIDTH-1 : rigel::ISSUE_WIDTH); j++) {
    InstrSlot instr_next = ccaccess(core->latches[FP2CC][j], j, false);

    if (!is_stalled(j)) {
      if (core->nlatches[CC2WB][j]->get_type() != I_NULL) {
        // Account for "skipped" cores[i+x]->stage_name_here(latch...) calls
        core->get_cluster()->getProfiler()->timing_stats.cc_block += (rigel::ISSUE_WIDTH - j - 1);
        break;
      }
      core->latches[FP2CC][j] = rigel::NullInstr;
      core->nlatches[CC2WB][j] = instr_next;
    }
  }

  if(oldSuccess) //Need to stall pipe 1 so that the instruction in latches[FP2CC][1] doesn't get squashed
    stall(rigel::ISSUE_WIDTH-1);
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: CCachAccessStage::ccaccess()
////////////////////////////////////////////////////////////////////////////////
// Description: Implements the ccaccess pipeline stage
////////////////////////////////////////////////////////////////////////////////
InstrSlot
CCacheAccessStage::ccaccess(InstrSlot instr, int pipe, bool stalledMemoryOp)
{
  /* STALL */
  if (instr->get_type() == I_NULL) {
    core->get_cluster()->getProfiler()->timing_stats.cc_bubble++;
    return instr;
  }

  // Did any pipe prior to this one in program order stall?  If so, we stall too
  if (pipe > 0 && is_stalled(pipe-1)) {
    core->get_cluster()->getProfiler()->timing_stats.cc_block++;
    return instr;
  }

  instr_t instr_type = instr->get_type();

  switch(instr_type) {
     case (I_PREF_L):
    case (I_PREF_NGA):
    {
      // Short circuit for finished loads
      if (instr->doneCCAccess()) { goto done_ccaccess; }

      bool stage_stall = false;
      uint32_t ea = instr->get_result_ea();

      stage_stall = !core->get_cluster()->getCacheModel()->prefetch_line(
                  *instr,
                  core->get_core_num_local(),
                  ea,
                  instr->get_local_thread_id()
                );

      if (stage_stall && !rigel::cache::CMDLINE_NONBLOCKING_PREFETCH) {
        if(rigel::NONBLOCKING_MEM) {
          if(!stalledMemoryOp) { core->memoryOps.push_back(instr); }
          return rigel::NullInstr;
        } else {
          Profile::accum_cache_stats.prefetch_line_stall_total++;
          stall(pipe);
          core->get_cluster()->getProfiler()->timing_stats.cc_stall++;
          return instr;
        }
      }
      
      unstall(pipe);
      using namespace simconst;
      instr->set_result_RF(
        0xdeadbeef,      // Should never be read
        NULL_REG,       // Should never be written
        ea
      );

      // Only count on valid prefetch.
      Profile::accum_cache_stats.prefetch_line_accesses++;
      instr->setCCAccess();

      core->ClearLoadAccessPending(instr->get_core_thread_id());

      goto done_ccaccess_this_cycle;
    }
    case (I_LDW):
    case (I_LDL):
    case (I_VLDW):
    {
      // Short circuit for finished loads
      if (instr->doneCCAccess()) { goto done_ccaccess; }
      //fprintf(stderr,"Trying to finish memory operation at ccache \n");

      uint32_t memval = 0xdeadbeef;
      bool stage_stall = false;
      uint32_t ea = instr->get_result_ea();
      assert((0 == (ea & 0x03)) && "Unaligned load access!");

      // For profiler.
      bool l1d_hit, l2d_hit, l3d_hit;

      MemoryAccessStallReason masr = core->get_cluster()->getCacheModel()->read_access(
        *instr, core->get_core_num_local(), ea, memval,
        stage_stall, l3d_hit, l2d_hit, l1d_hit,
        CoreInOrderLegacy::ClusterNum(core->get_core_num_global()),
        instr->get_local_thread_id()
      );

      switch(masr)
      {
        case NoStall:
          break;
        case L2DPending:
          #ifdef DEBUG_MT
          fprintf(stderr,"CCache LD Miss detected, force_thread_swap() \n");
          #endif
          //core->force_thread_swap();
          instr->stats.cycles.l2d_pending++;
          break;
        case L1DPending:
          instr->stats.cycles.l1d_pending++;
          break;
        case L1DMSHR:
          instr->stats.cycles.l1d_mshr++;
          break;
        case L2DArbitrate:
          instr->stats.cycles.l2d_arbitrate++;
          break;
        case L2DAccess:
          instr->stats.cycles.l2d_access++;
          break;
        case L2DMSHR:
          instr->stats.cycles.l2d_mshr++;
          break;
        case L2DAccessBitInterlock:
          instr->stats.cycles.l2d_access_bit_interlock++;
          break;
        default:
          assert(0 && "Invalid MemoryAccessStallReason");
          break;
      }

      // Update profiler if necessary
      instr->stats.cache.set_l1d_hit(l1d_hit);
      instr->stats.cache.set_l2d_hit(l2d_hit);
      instr->stats.cache.set_l3d_hit(l3d_hit);

      #ifdef DEBUG
      cerr << " [[CC: LOAD ACCESS]] EA: " << HEX_COUT << ea;
      cerr << " V: " << HEX_COUT << memval;
      cerr << " stall: " << boolalpha << stall << endl;
      #endif

      if (stage_stall) {
        if(rigel::NONBLOCKING_MEM) {
          if(!stalledMemoryOp) { core->memoryOps.push_back(instr); }
          return rigel::NullInstr;
        } else {
          Profile::accum_cache_stats.load_stall_total++;
          stall(pipe);
          core->get_cluster()->getProfiler()->timing_stats.cc_stall++;
          return instr;
        }
      }

      //Increment the histogram for how many cycles this instruction had to wait to arb.
      rigel::profiler::stats[STATNAME_L2_ARB_TIME].inc_mem_histogram(instr->stats.cycles.l2d_arbitrate, 1);

      unstall(pipe);
      // For vector operations, since they are 128-bit (four word) aligned, we
      // can assume that once one is a hit, they all are a hit.
      if (instr->get_is_vec_op()) {
        uint32_t mv0, mv1, mv2, mv3;
        uint32_t addr0, addr1, addr2, addr3;
        if (((ea) & (0xF)) != (0x0)) {
          DEBUG_HEADER();
          fprintf(stderr, "Unaligned vector load addr: 0x%08x\n", ea);
          assert(0);
        }
        addr0 = ea + 0x0;
        addr1 = ea + 0x4;
        addr2 = ea + 0x8;
        addr3 = ea + 0xc;
        // addr0 already is a hit so the rest must be
        //FIXME So much indirection is ugly.
        mv0 = core->get_cluster()->getCacheModel()->backing_store.read_word(addr0);
        mv1 = core->get_cluster()->getCacheModel()->backing_store.read_word(addr1);
        mv2 = core->get_cluster()->getCacheModel()->backing_store.read_word(addr2);
        mv3 = core->get_cluster()->getCacheModel()->backing_store.read_word(addr3);
        // Set the results.  Be careful for the first parameter since it needs
        // to be the vector register number not the first scalar of the vector:
        //          v = s / 4
        instr->set_result_RF_vec(
          instr->get_result_reg().addr / 4,
          mv0, mv1, mv2, mv3, ea
        );
        // Forward data on the bypass network
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg().addr,
          instr->get_result_reg().data.i32);
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg1().addr,
          instr->get_result_reg1().data.i32);
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg2().addr,
          instr->get_result_reg2().data.i32);
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg3().addr,
          instr->get_result_reg3().data.i32);

        #if 0
        DEBUG_HEADER();
        #endif
                // "Upgrade" to a regular bypass register
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(
          instr->get_result_reg().addr);
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(
          instr->get_result_reg1().addr);
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(
          instr->get_result_reg2().addr);
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(
          instr->get_result_reg3().addr);
      } else {
        instr->set_result_RF(
          memval,
          instr->get_dreg().addr,
          ea
        );
        // Forward data on the bypass network
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(
          instr->get_result_reg().addr, instr->get_result_reg().data.i32);
        // "Upgrade" to a regular bypass register
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(
          instr->get_result_reg().addr);
      }

      // Load access actually completed
      Profile::accum_cache_stats.load_accesses++;
      instr->setCCAccess();

      // FIXME: Update scoreboard with values just calculated.  May want to fix this so
      // FIXME: that you make data available this cycle instead of forcing it to go
      // FIXME: through the RF in writeback

      // FIXME: Remove the barrier and allow subsequent memory operations to execute.
      //fprintf(stderr,"Clearing Load Access Pending\n");

      core->ClearLoadAccessPending(instr->get_core_thread_id());

      goto done_ccaccess_this_cycle;
    }
    default:
    {
      // Do nothing
    }
  }

done_ccaccess_this_cycle:
  // Set the last access cycle.  This can be repeated, monotonically
  // updating the value.
  instr->set_last_memaccess_cycle();
done_ccaccess:
  // If cache data is not available, stall.
  core->get_cluster()->getProfiler()->timing_stats.cc_active++;

  return instr;
}


