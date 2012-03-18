///////////////////////////////////////////////////////////////////////////////
// execute.cpp
////////////////////////////////////////////////////////////////////////////////
//
// implementations for the ExecuteStage
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for stderr, fprintf
#include <sys/types.h>                  // for int32_t
#include <cmath>                        // for fabs, sqrtf
#include "memory/address_mapping.h"  // for AddressMapping
#include "caches_legacy/cache_model.h"    // for CacheModel
#include "caches_legacy/hybrid_directory.h"  // for HybridDirectory
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy, etc
#include "define.h"         // for RES_TYPE::L2_READ_CMD, etc
#include "util/event_track.h"    // for EventTrack, global_tracker
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrCycleStats, InstrStats
#include "profile/profile.h"        // for InstrStat, Profile, etc
#include "profile/profile_names.h"  // for ::STATNAME_BRANCH_TARGET
#include "core/regfile_legacy.h"        // for RegisterBase, etc
#include "rigellib.h"       // for event_track_t, etc
#include "core/scoreboard.h"     // for ScoreBoard
#include "core/scoreboard_macros.h"  // for UPDATE_SCOREBOARD, etc
#include "sim.h"            // for PC_REG, VOID_RESULT, etc
#include "util/syscall.h"        // for syscall_struct, Syscall
#include "util/util.h"           // for ExitSim
#include "stage_base.h"  // for ExecuteStage
#include "util/rigelprint.h"

//#define DEBUG

////////////////////////////////////////////////////////////////////////////////
// Method Name: ExecuteStage::Update()
////////////////////////////////////////////////////////////////////////////////
// Description:
//
// Inputs: none
// Outputs: none
////////////////////////////////////////////////////////////////////////////////
void ExecuteStage::Update()
{
  bool mispredict = false;
  int issue_count = 0;
  bool stalled[rigel::ISSUE_WIDTH];
  for (int i=0 ; i < rigel::ISSUE_WIDTH; i++){
    stalled[i] = true;
  }
  // pointer to core state
  //pipeline_per_core_state_t *cs = core->get_core_state();

  for (int j=0; j < rigel::ISSUE_WIDTH; j++) {
    stalled[j] = true;
    InstrSlot instr_next = execute(core->latches[DC2EX][j], j);

    //int tid = instr_next->get_core_thread_id();
    // pointer to thread state
    //pipeline_per_thread_state_t *ts = core->get_thread_state(tid);

    if (!is_stalled(j) && !mispredict) {
      if (core->nlatches[EX2MC][j]->get_type() != I_NULL) {
        // Account for "skipped" cores[i+x]->stage_name_here(latch...) calls
        core->get_cluster()->getProfiler()->timing_stats.ex_block += (rigel::ISSUE_WIDTH - j - 1);
        break;
      }
      // If the instruction is valid, the pipeline issued an instruction this cycle!
      if (I_NULL != instr_next->get_type()) issue_count++;

      stalled[j] = false;
      core->latches[DC2EX][j] = rigel::NullInstr;
      core->nlatches[EX2MC][j] = instr_next;
      // Handle mispredicts
      if (instr_next->is_mispredict()) {
        mispredict = true;
        core->flush(instr_next->get_core_thread_id());
      }
    }
  }
  for (int j=0; j < rigel::ISSUE_WIDTH; j++) {
    // Instruction is completed.  Release resource.  TODO: Clean this up so it is
      // a member of InstrLegacy()
      if( core->latches[DC2EX][j]->get_type()  == I_NULL && 
          core->nlatches[EX2MC][j]->get_type() != I_NULL &&
          !stalled[j])
      {
          core->get_regfile(core->nlatches[EX2MC][j]->get_core_thread_id())->free_read_ports(core->nlatches[EX2MC][j]->get_num_read_ports());
          core->PipeCoreState.funit_available_count[INSTR_FUNIT[core->nlatches[EX2MC][j]->get_type()]]++;
      }
  }
  // Count the number of cycles where we dual issue
  switch (issue_count) {
    case 2: core->get_cluster()->getProfiler()->timing_stats.two_issue_cycles++; break;
    case 1: core->get_cluster()->getProfiler()->timing_stats.one_issue_cycles++; break;
    case 0: core->get_cluster()->getProfiler()->timing_stats.zero_issue_cycles++; break;
    default:
      DEBUG_HEADER();  fprintf(stderr, "Impossible issue count!\n");
      assert(0);
  }

}

////////////////////////////////////////////////////////////////////////////////
// Method Name: ExecuteStage::execute()
////////////////////////////////////////////////////////////////////////////////
// Description:
//
// Inputs:
//
// Outputs:
////////////////////////////////////////////////////////////////////////////////
InstrSlot
ExecuteStage::execute(InstrSlot instr, int pipe) {
  /* Used for resolving misfetches */
  bool is_fallthrough = false, mispredict;
  instr_t instr_type = instr->get_type();

  // Do not count when a SLEEP_ON() is pending. I.e., do not count cycles that
  // are not part of the kernel we want to test.
  if (!rigel::SPRF_LOCK_CORES) {
    // Every cycle execute can happen counts as an issue slot.
    core->get_cluster()->getProfiler()->timing_stats.issue_slots_total++;
  }

  /* STALL */
  if (instr_type == I_NULL) {
    #ifdef DEBUG
    cerr << "EX: [[BUBBLE]]" << endl;
    #endif
    core->get_cluster()->getProfiler()->timing_stats.ex_bubble++;
    //if(core->get_core_num_global() == 0) printf("Pipe %d BUBBLE\n", pipe); 
    return instr;
  }

  #ifdef DEBUG_MT
  cerr <<"EXECUTE: INSTR DUMP\n";
  instr->dump();
  #endif

    /* Did any pipe prior to this one in program order stall?  If so, we stall too */
    if (pipe > 0 && is_stalled(pipe-1)) {
      #ifdef DEBUG
      cerr << "EX: currPC: 0x" << hex << setw(8) << setfill('0')
        << instr->get_currPC() << " [[STALL]]" << endl;
      #endif
      core->get_cluster()->getProfiler()->timing_stats.ex_block++;
      //if(core->get_core_num_global() == 0) printf("PIPE %d STALLED SO PIPE %d IS STALLING\n", pipe-1, pipe); 
      return instr;
   }
    // SYNC already been REQ'ed, but not yet ACK'ed:  STALL.
    if (core->get_thread_state(instr->get_core_thread_id())->pending_sync_req && !core->get_thread_state(instr->get_core_thread_id())->pending_sync_ack) {
      stall(pipe);
      instr->stats.cycles.sync_waiting_for_ack++;
      core->get_cluster()->getProfiler()->timing_stats.ex_block++;
      //if(core->get_core_num_global() == 0) printf("Pipe %d SYNC NOT ACKED\n", pipe); 
      return instr;
    }
    // SYNC signalled and now ACK'ed
    if (core->get_thread_state(instr->get_core_thread_id())->pending_sync_ack) {
      // Trigger mispredict to restart at this instruction.
      instr->set_nextPC(instr->get_currPC());
      instr->set_mispredict();
      // NOTE: bp code removed, branch predictor appears deprecated and not in
      // use...
      //core->get_bp()->update(instr->get_currPC(), instr->get_currPC(), true, true,
      //  true /* Fake a branch */ , instr->get_glHist()
      //);
      // Reset sync ACK for next use
      core->get_thread_state(instr->get_core_thread_id())->pending_sync_ack = false;

      instr->set_doneExec();
      //if(core->get_core_num_global() == 0) printf("Pipe %d ACKing Sync\n", pipe); 
      unstall(pipe);

      // Flushes here included the instruction that initiates the flush (i.e.,
      // the one that follows the SYNC)  Unlike BP mispredicts, we must clean up
      // this instruction as well.  Since it has not done regread, we only need
      // to set it to I_NULL so that it flows through the pipeline innocuously.
      instr->update_type(I_DONE);
      core->RemoveSpecInstr(instr);
      return instr;
    }


    #ifdef DEBUG
    cerr << "EX: currPC: 0x" << hex << setw(8) << setfill('0') <<
    instr->get_currPC() << endl;
    #endif

    /* FIXME...this would be better fixed with proper latching or this could be
     * moved into the pipeline in cluster.cpp
     */
    if (instr->doneExec()) {
      // This was already counted as active in the cycle before it stalled
      core->get_cluster()->getProfiler()->timing_stats.ex_stall++;
      //if(core->get_core_num_global() == 0) printf("Pipe %d DoneExec\n", pipe); 
      return instr;
    }
    // Check whether we have a free functional unit to dispatch this instruction
    // to
    switch (INSTR_FUNIT[instr_type])
    {
      case FU_NONE:
        // no execute stalls
        break;
      case FU_ALU:
      case FU_ALU_VECTOR:
        if (core->PipeCoreState.funit_available_count[FU_ALU_VECTOR] > 0 &&
          core->PipeCoreState.funit_available_count[FU_ALU] > 0) break;
        instr->stats.cycles.resource_conflict_alu++;
        stall(pipe);
        //if(core->get_core_num_global() == 0) printf("Pipe %d ALU conflict\n", pipe); 
        return instr;
      case FU_FPU:
      case FU_FPU_VECTOR:
        if (core->PipeCoreState.funit_available_count[FU_FPU_VECTOR] > 0 &&
          core->PipeCoreState.funit_available_count[FU_FPU] > 0) break;
        instr->stats.cycles.resource_conflict_fpu++;
        stall(pipe);
        //if(core->get_core_num_global() == 0) printf("Pipe %d FPU conflict\n", pipe); 
        return instr;
      case FU_ALU_SHIFT:
        if (core->PipeCoreState.funit_available_count[FU_ALU_SHIFT] > 0) break;
        instr->stats.cycles.resource_conflict_alu_shift++;
        stall(pipe);
        //if(core->get_core_num_global() == 0) printf("Pipe %d SHIFT conflict\n", pipe); 
        return instr;
      case FU_MEM:
        if (core->PipeCoreState.funit_available_count[FU_MEM] > 0) break;
        instr->stats.cycles.resource_conflict_mem++;
        //if(core->get_core_num_global() == 0) printf("Pipe %d MEM conflict\n", pipe); 
        stall(pipe);
        return instr;
      case FU_BRANCH:
        if (core->PipeCoreState.funit_available_count[FU_BRANCH] > 0) break;
        instr->stats.cycles.resource_conflict_branch++;
        stall(pipe);
        //if(core->get_core_num_global() == 0) printf("Pipe %d BRANCH conflict\n", pipe); 
        return instr;
      case FU_BOTH:
        if ((core->PipeCoreState.funit_available_count[FU_ALU] > 0 &&
          core->PipeCoreState.funit_available_count[FU_FPU] > 0 &&
          core->PipeCoreState.funit_available_count[FU_FPU_VECTOR] > 0 &&
          core->PipeCoreState.funit_available_count[FU_ALU_VECTOR] > 0) ||
          instr_type == I_NOP) break;
        instr->stats.cycles.resource_conflict_both++;
        stall(pipe);
        //if(core->get_core_num_global() == 0) printf("Pipe %d BOTH conflict\n", pipe); 
        return instr;

      // Unsupported functional units
      default:
      case FU_SPFU:
        DEBUG_HEADER(); fprintf(stderr, "Unknown FU_TYPE\n");
        assert(0);
        break;
    }

  if (core->regfile_read(instr, pipe) == rigel::NullInstr) {
    return rigel::NullInstr;
  }
  // Do not count when a SLEEP_ON() is pending. I.e., do not count cycles that
  // are not part of the kernel we want to test.
  if (!rigel::SPRF_LOCK_CORES) {
    // Vector operations keep issue four times but stall for three of them so do
    // proper accounting here.
    instr->get_is_vec_op() ?
      core->get_cluster()->getProfiler()->timing_stats.issue_slots_filled += 4 :
      core->get_cluster()->getProfiler()->timing_stats.issue_slots_filled += 1;
  }

    // Instruction will execute, acquire resource.  TODO: Clean this up so it is
    // a member of InstrLegacy()
  core->PipeCoreState.funit_available_count[INSTR_FUNIT[instr_type]]--;
  core->get_regfile(instr->get_core_thread_id())->request_read_ports(instr->get_num_read_ports());
  // The instruction is no longer speculative so we are going to remove it so
  // it is not flushed later by an earlier instruction
  core->RemoveSpecInstr(instr);
  // Pseudo-stage
  unstall(pipe);
  // use to check for mispredicts
  uint32_t new_pc;
  new_pc = instr->get_currPC() + 4;
 
{
  using namespace simconst;
  // These are inlined anyway, but for clarity's sake
  RegisterBase sreg0 = instr->get_sreg0();
  RegisterBase sreg1 = instr->get_sreg1();
  RegisterBase sreg2 = instr->get_sreg2();
  RegisterBase sreg3 = instr->get_sreg3();
  RegisterBase sacc  = instr->get_sacc();
  RegisterBase dreg  = instr->get_dreg();
  RegisterBase dacc  = instr->get_dacc();
  // Added for vector operations
  RegisterBase sreg4 = instr->get_sreg4();
  RegisterBase sreg5 = instr->get_sreg5();
  RegisterBase sreg6 = instr->get_sreg6();
  RegisterBase sreg7 = instr->get_sreg7();
  //RegisterBase dreg1  = instr->get_dreg1();
  //RegisterBase dreg2  = instr->get_dreg2();
  //RegisterBase dreg3  = instr->get_dreg3();


#define DEBUG_VECTOROP()  \
  DEBUG_HEADER();         \
  fprintf(stderr, "\nsreg0 (addr) %d (val) %d\n", sreg0.addr, sreg0.data.u32);\
  fprintf(stderr, "sreg1 (addr) %d (val) %d\n", sreg1.addr, sreg1.data.u32);\
  fprintf(stderr, "sreg2 (addr) %d (val) %d\n", sreg2.addr, sreg2.data.u32);\
  fprintf(stderr, "sreg3 (addr) %d (val) %d\n", sreg3.addr, sreg3.data.u32);\
  fprintf(stderr, "sreg4 (addr) %d (val) %d\n", sreg4.addr, sreg4.data.u32);\
  fprintf(stderr, "sreg5 (addr) %d (val) %d\n", sreg5.addr, sreg5.data.u32);\
  fprintf(stderr, "sreg6 (addr) %d (val) %d\n", sreg6.addr, sreg6.data.u32);\
  fprintf(stderr, "sreg7 (addr) %d (val) %d\n", sreg7.addr, sreg7.data.u32);
  /* We have access to our sources, time to execute! */
  switch (instr_type)
  {
    /***********************************/
    /* XXX BEGIN VECTOR OPERATIONS XXX */
    /***********************************/
    case (I_VADD):
    {
      // Handle bypassing as necessary.
      IS_BYPASS_VEC_REG0(); // Vector register s
      IS_BYPASS_VEC_REG1(); // Vector register t
      // Do the vector operation.
      instr->set_result_RF_vec(
        (dreg.addr / 4), /* convert to vector register number */
        (uint32_t)sreg0.data.u32 + (uint32_t)sreg4.data.u32,
        (uint32_t)sreg1.data.u32 + (uint32_t)sreg5.data.u32,
        (uint32_t)sreg2.data.u32 + (uint32_t)sreg6.data.u32,
        (uint32_t)sreg3.data.u32 + (uint32_t)sreg7.data.u32
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD_VEC();

      goto done_execute;
    }
    case (I_VSUB):
    {
      // Handle bypassing as necessary.
      IS_BYPASS_VEC_REG0(); // Vector register s
      IS_BYPASS_VEC_REG1(); // Vector register t
      // Do the vector operation.
      instr->set_result_RF_vec(
        (dreg.addr / 4), /* convert to vector register number */
        (uint32_t)sreg0.data.u32 - (uint32_t)sreg4.data.u32,
        (uint32_t)sreg1.data.u32 - (uint32_t)sreg5.data.u32,
        (uint32_t)sreg2.data.u32 - (uint32_t)sreg6.data.u32,
        (uint32_t)sreg3.data.u32 - (uint32_t)sreg7.data.u32
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD_VEC();

      goto done_execute;
    }
    case (I_VFADD):
    {
      // Handle bypassing as necessary.
      IS_BYPASS_VEC_REG0(); // Vector register s
      IS_BYPASS_VEC_REG1(); // Vector register t
      // Do the vector operation.
      instr->set_result_RF_vec(
        (dreg.addr / 4), /* convert to vector register number */
        sreg0.data.f32 + sreg4.data.f32,
        sreg1.data.f32 + sreg5.data.f32,
        sreg2.data.f32 + sreg6.data.f32,
        sreg3.data.f32 + sreg7.data.f32
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD_VEC();

      goto done_execute;
    }
    case (I_VFSUB):
    {
      // Handle bypassing as necessary.
      IS_BYPASS_VEC_REG0(); // Vector register s
      IS_BYPASS_VEC_REG1(); // Vector register t
      // Do the vector operation.
      instr->set_result_RF_vec(
        (dreg.addr / 4), /* convert to vector register number */
        sreg0.data.f32 - sreg4.data.f32,
        sreg1.data.f32 - sreg5.data.f32,
        sreg2.data.f32 - sreg6.data.f32,
        sreg3.data.f32 - sreg7.data.f32
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD_VEC();

      goto done_execute;
    }
    case (I_VFMUL):
    {
      // Handle bypassing as necessary.
      IS_BYPASS_VEC_REG0(); // Vector register s
      IS_BYPASS_VEC_REG1(); // Vector register t
      // Do the vector operation.
      instr->set_result_RF_vec(
        (dreg.addr / 4), /* convert to vector register number */
        sreg0.data.f32 * sreg4.data.f32,
        sreg1.data.f32 * sreg5.data.f32,
        sreg2.data.f32 * sreg6.data.f32,
        sreg3.data.f32 * sreg7.data.f32
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD_VEC();

      goto done_execute;
    }
    case (I_VADDI):
    {
      // Handle bypassing as necessary.
      IS_BYPASS_VEC_REG0(); // Vector register s
      // Do the vector operation.
      instr->set_result_RF_vec(
        (dreg.addr / 4), /* convert to vector register number */
        (uint32_t)sreg0.data.u32 + (int32_t)instr->get_simm(),
        (uint32_t)sreg1.data.u32 + (int32_t)instr->get_simm(),
        (uint32_t)sreg2.data.u32 + (int32_t)instr->get_simm(),
        (uint32_t)sreg3.data.u32 + (int32_t)instr->get_simm()
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD_VEC();

      goto done_execute;
    }
    case (I_VSUBI):
    {
      // Handle bypassing as necessary.
      IS_BYPASS_VEC_REG0(); // Vector register s
      // Do the vector operation.
      instr->set_result_RF_vec(
        (dreg.addr / 4), /* convert to vector register number */
        (uint32_t)sreg0.data.u32 - (int32_t)instr->get_simm(),
        (uint32_t)sreg1.data.u32 - (int32_t)instr->get_simm(),
        (uint32_t)sreg2.data.u32 - (int32_t)instr->get_simm(),
        (uint32_t)sreg3.data.u32 - (int32_t)instr->get_simm()
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD_VEC();

      goto done_execute;
    }
    case (I_VLDW):
    {
      IS_BYPASS_VEC_REG_BASE();
      uint32_t addr;

      // Note the different base register from scalar load
      addr = sreg4.data.u32 + (instr->get_simm());
      instr->set_result_RF_vec(
        dreg.addr / 4,
        VOID_RESULT,
        VOID_RESULT,
        VOID_RESULT,
        VOID_RESULT,
        addr
      );

      // arb ahead of time if we will miss L1 (or if we don't have an L1)
      core->get_cluster()->getCacheModel()->ArbEarly(
        addr,
        core->get_core_num_local(),
        instr->get_local_thread_id(),
        L2_READ_CMD
      );

      goto done_execute;

    }
    case (I_VSTW):
    {
      IS_BYPASS_VEC_REG_BASE();
      IS_BYPASS_VEC_REG0();
      uint32_t addr;

      // Note the different base register from scalar load
      addr = sreg4.data.u32 + (instr->get_simm());
      instr->set_result_RF_vec(
        dreg.addr / 4,
        sreg0.data.u32,
        sreg1.data.u32,
        sreg2.data.u32,
        sreg3.data.u32,
        addr
      );

      // arb ahead of time if we will miss L1 (or if we don't have an L1)
      core->get_cluster()->getCacheModel()->ArbEarly(
        addr,
        core->get_core_num_local(),
        instr->get_local_thread_id(),
        L2_READ_CMD
      );

      goto done_execute;

    }
    /***********************************/
    /* XXX END VECTOR OPERATIONS XXX */
    /*********************************/

    case (I_NOP):
    case (I_UNDEF):
    case (I_HLT):
    case (I_REP_HLT):
    case (I_ABORT):
    // Cache management instructions
    case (I_IC_INV):
    case (I_CC_INV):
    {
      /* Handled at WB */
      goto done_execute;
    }
    case (I_TQ_ENQUEUE):
    case (I_TQ_LOOP):
    case (I_TQ_INIT):
    {
      using namespace rigel::task_queue;

      IS_BYPASS_TASK_QUEUE4();
      // Update the register values if they were on the bypass network
      instr->set_regs_val(sreg0, sreg1, sreg2, sreg3, sacc);
      // Handled at memory stage
      goto done_execute;
    }
    case (I_TQ_END):
    case (I_TQ_DEQUEUE):
    {
      // No source regs
      // Handled at memory stage
      goto done_execute;
    }
    case (I_ADD):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()
      instr->set_result_RF(
        (uint32_t)sreg0.data.u32 +
        (uint32_t)sreg1.data.u32,
        dreg.addr
      );

      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_ADDU):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        sreg0.data.u32 +
        sreg1.data.u32,
        dreg.addr
      );

      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SUBU): {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        (uint32_t)sreg0.data.u32 -
        (uint32_t)sreg1.data.u32,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SUB): {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        sreg0.data.u32 -
        sreg1.data.u32,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_ADDIU):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        (uint32_t)sreg0.data.u32 +
        (uint32_t)instr->get_simm(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_PRIO):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      using namespace rigel::event;

      int level = instr->get_simm();

      // Update piority immediately
      core->set_net_priority(level);

      // There is no real return value from an event, so the dreg is sort of
      // useless here.
      instr->set_result_RF(
        level,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_EVENT):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      using namespace rigel::event;

      // Handle the event.
      event_track_t event = (event_track_t)instr->get_simm();
      rigel::event::global_tracker.HandleEvent(
        event,
        instr->get_global_thread_id(),
        core
      );

      // There is no real return value from an event, so the dreg is sort of
      // useless here.
      instr->set_result_RF(
        event,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_ADDI):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32 +
        instr->get_simm(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SUBI):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32 -
        instr->get_simm(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SUBIU):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        (uint32_t)sreg0.data.u32 -
        (uint32_t)instr->get_simm(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_AND):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        sreg0.data.u32 &
        sreg1.data.u32,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_OR):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        sreg0.data.u32 |
        sreg1.data.u32,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_XOR):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        sreg0.data.u32 ^
        sreg1.data.u32,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()


      goto done_execute;
    }
    case (I_NOR):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        ~(sreg0.data.u32 |
        sreg1.data.u32),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_ANDI):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32 &
        instr->get_imm(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_ORI):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32 |
        instr->get_imm(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_XORI):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32 ^
        instr->get_imm(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SLL):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        sreg0.data.u32 <<
        (sreg1.data.u32 & 0x0000001FU),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SRL):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        sreg0.data.u32 >>
        (sreg1.data.u32 & 0x0000001FU),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SRA):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      uint32_t mask = 0x00000000;
      uint32_t sreg = sreg0.data.u32;
      uint32_t shift = sreg1.data.u32 & 0x0000001FU;
      if (sreg & 0x80000000) { /* Negative */
        mask = 0xFFFFFFFF << (32 - shift);
      }

      instr->set_result_RF(
        (sreg >> shift) | mask,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SLLI):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32 <<
        instr->get_imm5(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SRLI):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32 >>
        instr->get_imm5(),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SRAI):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      uint32_t mask = 0x00000000;
      uint32_t sreg = sreg0.data.u32;
      uint32_t shift = instr->get_imm5();
      if (sreg & 0x80000000) { /* Negative */
        mask = 0xFFFFFFFF << (32 - shift);
      }

      instr->set_result_RF(
        (sreg >> shift) | mask,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CEQ):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        (int)((int32_t)sreg0.data.u32 ==
        (int32_t)sreg1.data.u32),
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CLT):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        (int)((int32_t)sreg0.data.u32 <
        (int32_t)sreg1.data.u32),
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CLE):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        (int)((int32_t)sreg0.data.u32 <=
        (int32_t)sreg1.data.u32),
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CLTU):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        (int)((uint32_t)sreg0.data.u32 <
        (uint32_t)sreg1.data.u32),
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CLEU):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      instr->set_result_RF(
        (int)(sreg0.data.u32 <=
        sreg1.data.u32),
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CEQF):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      bool result = sreg1.data.f32 == sreg0.data.f32;
      instr->set_result_RF(
        result,
        dreg.addr
        );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CLTF):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      bool result = sreg0.data.f32 < sreg1.data.f32;
      instr->set_result_RF(
        result,
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CLTEF):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      bool result = sreg0.data.f32 <= sreg1.data.f32;
      instr->set_result_RF(
        result,
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CMEQ):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      uint32_t cond = sreg1.data.u32;

      if (0 == cond) {
        instr->set_result_RF(
          sreg0.data.u32,
          dreg.addr
        );

      } else {
        instr->update_type(I_CMEQ_NOP);
        instr->set_result_RF(
          VOID_RESULT,
          NULL_REG
        );
      }

      goto done_execute;
    }
    case (I_CMNE):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      uint32_t cond = sreg1.data.u32;

      if (0 != cond) {
        instr->set_result_RF(
          sreg0.data.u32,
          dreg.addr
        );

      } else {
        instr->update_type(I_CMNE_NOP);
        instr->set_result_RF(
          VOID_RESULT,
          NULL_REG
        );
      }
      goto done_execute;
    }
    case (I_MFSR):
    {
      // Get bypass results
      if (instr->is_bypass_sreg0()) { sreg0.data.u32 = core->get_sp_scoreboard(instr->get_core_thread_id())->get_bypass(sreg0.addr); }

      instr->set_result_RF(
        sreg0.data.u32,
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_MTSR):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32,  /* Value to write */
        dreg.addr
      );
      // Enable/disable non-blocking atomics.
      if (dreg.addr == SPRF_NONBLOCKING_ATOMICS) {
//DEBUG_HEADER();
        if (0 == sreg0.data.u32) { 
          core->get_thread_state(
            instr->get_core_thread_id())->nonblocking_atomics_enabled = false;
        } else {
          core->get_thread_state(
            instr->get_core_thread_id())->nonblocking_atomics_enabled = true;
        }
      }

      core->get_sp_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg().addr, instr->get_result_reg().data.u32);

      goto done_execute;
    }
    case (I_MVUI):
    {
      // Get bypass results
      if (instr->is_bypass_sreg0()) { sreg0.data.u32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg0.addr); }

      // MVUI henceforth clobbers the lower 16b We may want to fix this
      // (and remove the bypass above)
      uint32_t curr = instr->get_imm() << 16;
      instr->set_result_RF(
        curr,
        dreg.addr
      );
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_BE):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      if (0 == sreg0.data.u32) {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
        instr->set_result_RF(
          new_pc,
          PC_REG
        );
      } else {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4);
        instr->update_type(I_BRANCH_FALL_THROUGH);
        is_fallthrough = true;
      }
      goto done_execute;
    }
    case (I_BN):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      if (0 != sreg0.data.u32) {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
        instr->set_result_RF(
          new_pc,
          PC_REG
        );
      } else {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4);
        instr->update_type(I_BRANCH_FALL_THROUGH);
        is_fallthrough = true;
      }
      goto done_execute;
    }
    case (I_BEQ):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      if (sreg1.data.u32 == sreg0.data.u32) {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
        instr->set_result_RF(
          new_pc,
          PC_REG
        );
      } else {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4);
        instr->update_type(I_BRANCH_FALL_THROUGH);
        is_fallthrough = true;
      }
      goto done_execute;
    }
    // SYNC stalls until the previous instruction has committed.  On first
    // entry, pending_sync_req is set
    //
    // When it commits, pending_sync_ack will be set.  SYNC clears the ack and
    // clears the req and then triggers a branch mispredict to reset the pipe.
    case (I_SYNC):
    {
      core->get_thread_state(instr->get_core_thread_id())->pending_sync_req = true;

      goto done_execute;
    }
    case (I_BNE):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      if (sreg1.data.u32 != sreg0.data.u32) {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
        instr->set_result_RF(
          new_pc,
          PC_REG
        );
      } else {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4);
        instr->update_type(I_BRANCH_FALL_THROUGH);
        is_fallthrough = true;
      }
      goto done_execute;
    }
    case (I_BLT):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      if ((int32_t)sreg0.data.u32 < 0) {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
        instr->set_result_RF(
          new_pc,
          PC_REG
        );
      } else {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4);
        instr->update_type(I_BRANCH_FALL_THROUGH);
        is_fallthrough = true;
      }
      goto done_execute;
    }
    case (I_BGT):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      if ((int32_t)sreg0.data.u32 > 0) {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
        instr->set_result_RF(
          new_pc,
          PC_REG
        );
      } else {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4);
        instr->update_type(I_BRANCH_FALL_THROUGH);
        is_fallthrough = true;
      }
      goto done_execute;
    }
    case (I_BLE):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      if ((int32_t)sreg0.data.u32 <= 0) {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
        instr->set_result_RF(
          new_pc,
          PC_REG
        );
      } else {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4);
        instr->update_type(I_BRANCH_FALL_THROUGH);
        is_fallthrough = true;
      }
      goto done_execute;
    }
    case (I_BGE):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      if ((int32_t)sreg0.data.u32 >= 0) {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
        instr->set_result_RF(
          new_pc,
          PC_REG
        );
      } else {
        new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4);
        instr->update_type(I_BRANCH_FALL_THROUGH);
        is_fallthrough = true;
      }
      goto done_execute;
    }
    case (I_JMP):
    {

      new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
      instr->set_result_RF(
        new_pc,
        PC_REG
      );
      goto done_execute;
    }
     case (I_LJ):
    {
      new_pc = (0xF0000000 & (instr->get_currPC())) | 4*(instr->get_raw_instr() & 0x03FFFFFF);
      instr->set_result_RF(
        new_pc,
        PC_REG
      );
      goto done_execute;
    }

     case (I_LJL):
    {
      new_pc = (0xF0000000 & (instr->get_currPC())) | 4*(instr->get_raw_instr() & 0x03FFFFFF);
      instr->set_result_RF(
        new_pc,
        rigel::regs::LINK_REG,
        // The value below may be irrelevant.
        instr->get_currPC()+4
      );
      instr->set_nextPC(new_pc);
      // Update scoreboard with values just calculated
      core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(rigel::regs::LINK_REG, instr->get_currPC()+4);

      goto done_execute;
    }

     case (I_JAL):
    {
      //sb->get_reg(dreg, instr_type);

      new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4 *instr->get_simm() + 4);
      //new_pc = 0xFFFFFFFF & (instr->get_currPC() + 4*instr->get_simm() + 4);
      instr->set_result_RF(
        new_pc,
        rigel::regs::LINK_REG,
        // The value below may be irrelevant.
        instr->get_currPC()+4
      );
      instr->set_nextPC(new_pc);
      // Update scoreboard with values just calculated
      core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(rigel::regs::LINK_REG, instr->get_currPC()+4);

      goto done_execute;
    }
     case (I_JALR):
    {
      //sb->get_reg(dreg, instr_type);
      // Get bypass results
      IS_BYPASS_SREG0()

      new_pc = sreg0.data.u32;
      instr->set_result_RF(
        new_pc,
        PC_REG,
        // The value below may be irrelevant.
        instr->get_currPC()+4  /* Where you should return to after return? */
      );
      instr->set_nextPC(new_pc);
      // Update scoreboard with values just calculated
      core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(rigel::regs::LINK_REG, instr->get_currPC()+4);

      goto done_execute;
    }
    // Pass along the address in sreg0 and invalidate it in MB stage
     case (I_LINE_INV):
     case (I_LINE_WB):
     case (I_LINE_FLUSH):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.u32,
        NULL_REG,
        (sreg0.data.u32 & ~0x03) /* EA of the invalidate */
      );
      goto done_execute;
    }
     case (I_JMPR):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      new_pc = sreg0.data.u32;
  //DEBUG_HEADER(); cerr << "JMPR new_pc: 0x" << hex << new_pc  << endl;
      instr->set_result_RF(
        new_pc,
        PC_REG
      );
      goto done_execute;
    }
     case (I_RFE):
    {
      goto bad_itype;
    }
    // EXECUTE: Prefetch a line, bypass G$
    case (I_PREF_NGA):
    // I am disabling this until we get the big CG/prefetch 8
    // bug fixed. (Bug 72?)
    instr->dump();
    assert(0 && "EXECUTING A NGA PREFETCH NO!!!!!");
    // EXECUTE: Get the address of a line to globally invalidate
    case (I_BCAST_INV):
    // EXECUTE: Prefetch a line from the G$.
    case (I_PREF_L):
    {
      IS_BYPASS_SREG0();
      uint32_t addr;

      addr = sreg0.data.u32 + (instr->get_simm());
      instr->set_result_RF(
        VOID_RESULT,
        VOID_RESULT,
        addr
      );

      goto done_execute;
    }
    case (I_PREF_B_GC):
    {
       // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      uint32_t addr;
      //move the starting address to a cacheline boundary
      addr = (sreg1.data.u32 + (instr->get_simm()))&rigel::cache::LINE_MASK;

      //truncate the block prefetch to a gcache "row" boundary.
      //this forces alignement
      uint32_t linesToPrefetch=0;
      unsigned int originalGCacheBank = AddressMapping::GetGCacheBank(addr);
      //Walk from addr to addr+32*sreg0.data.u32 until a line maps to a different G$ bank than addr.
      //TODO: May want this to be different DRAM channel and allow the 4 G$ banks to service the IC_Msg.
      while(1)
      {
        //If the next line would map to a different G$ bank, stop.
        if(AddressMapping::GetGCacheBank(addr + (rigel::cache::LINESIZE*(linesToPrefetch+1))) != originalGCacheBank)
          break;
        //If the next line would be farther than specified in the instruction, stop.
        if(linesToPrefetch == (sreg0.data.u32-1))
          break;
        //Otherwise, proceed.
        linesToPrefetch++;
      }
      //Include the line "addr" is on.
      linesToPrefetch++;

      //fprintf(stderr,"Block Prefetch at addr:0x%08x size:%d\n", addr, linesToPrefetch);

      sreg0.data.u32 = linesToPrefetch;

      instr->set_result_RF(
        sreg0.data.u32,
        dreg.addr,
        addr
      );

      core->get_cluster()->getCacheModel()->ArbEarly(
        addr,
        core->get_core_num_local(),
        instr->get_local_thread_id(),
        L2_READ_CMD
      );

      goto done_execute;
    }
    //TODO For now, we enforce the same constraints on C$-allocated BulkPF.  In the future,
    //we may want to allow the C$ controller to break up a single I_PREF_B_CC into multiple ICMsgs.
    case (I_PREF_B_CC):
    {
       // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      uint32_t addr;
      //move the starting address to a cacheline boundary
      addr = (sreg1.data.u32 + (instr->get_simm()))&rigel::cache::LINE_MASK;

      //truncate the block prefetch to a gcache "row" boundary.
      //this forces alignement
      uint32_t linesToPrefetch=0;
      unsigned int originalGCacheBank = AddressMapping::GetGCacheBank(addr);
      //Walk from addr to addr+32*sreg0.data.u32 until a line maps to a different G$ bank than addr.
      //TODO: May want this to be different DRAM channel and allow the 4 G$ banks to service the IC_Msg.
      while(1)
      {
        //If the next line would map to a different G$ bank, stop.
        if(AddressMapping::GetGCacheBank(addr + (rigel::cache::LINESIZE*(linesToPrefetch+1))) != originalGCacheBank)
          break;
        //If the next line would be farther than specified in the instruction, stop.
        if(linesToPrefetch == (sreg0.data.u32-1))
          break;
        //Otherwise, proceed.
        linesToPrefetch++;
      }
      //Include the line "addr" is on.
      linesToPrefetch++;

      //fprintf(stderr,"Block Prefetch at addr:0x%08x size:%d\n", addr, linesToPrefetch);

      sreg0.data.u32=linesToPrefetch;

      instr->set_result_RF(
        sreg0.data.u32,
        dreg.addr,
        addr
      );

      core->get_cluster()->getCacheModel()->ArbEarly(
        addr,
        core->get_core_num_local(),
        instr->get_local_thread_id(),
        L2_READ_CMD
      );

      goto done_execute;
    }
    case (I_LDL):
    case (I_LDW):
    case (I_GLDW):
    {
      // Get bypass results
      IS_BYPASS_SREG0()
      uint32_t addr;
//DEBUG_HEADER();
//cerr << "LDW: sreg0: " << dec << sreg0.addr << " " << hex << sreg0.data.u32;
//cerr << " imm: " << dec << instr->get_simm();
//cerr << " sreg1: " << dec << sreg1.addr << " 0x" << hex << sreg1.data.u32;
//cerr << " dreg: " << dec << dreg.addr << " 0x" << hex << dreg.data.u32;
//cerr << endl;
      if(instr_type == I_LDL)
        addr = sreg0.data.u32;
      else
        addr = sreg0.data.u32 + (instr->get_simm());
      instr->set_result_RF(
        VOID_RESULT,
        dreg.addr,
        addr
      );

     // #define ARB_EARLY
     // #ifdef ARB_EARLY
        // arb ahead of time if we will miss L1 (or if we don't have an L1)
        core->get_cluster()->getCacheModel()->ArbEarly(
          addr,
          core->get_core_num_local(),
          instr->get_local_thread_id(),
          L2_READ_CMD
        );
    //  #endif

      goto done_execute;
    }
    case (I_ATOMCAS):
      IS_BYPASS_SREG2(); //Only ATOMCAS has an sreg2, I think (it's an alias
                         //for dreg, since the swap value register is both read and written).
      //Fall through
    case (I_ATOMADDU):
    case (I_ATOMMAX):
    case (I_ATOMMIN):
    case (I_ATOMOR):
    case (I_ATOMAND):
    case (I_ATOMXOR):
    {
      // Get bypass results
#if 0
DEBUG_HEADER();
fprintf(stderr, "ATOMCAS: r0: %d (0x%08x) r1: %d (0x%08x) r2: %d (0x%08x) [[PRE BYPASS]]\n",
  sreg0.addr, sreg0.data.u32,
  sreg1.addr, sreg1.data.u32,
  sreg2.addr, sreg2.data.u32);
#endif

      IS_BYPASS_SREG0_SREG1();
#if 0
DEBUG_HEADER();
fprintf(stderr, "ATOMCAS or ATOMADDU: r0: %d (0x%08x) r1: %d (0x%08x) r2: %d (0x%08x) [[POST BYPASS]]\n",
  sreg0.addr, sreg0.data.u32,
  sreg1.addr, sreg1.data.u32,
  sreg2.addr, sreg2.data.u32);
#endif

      //Unlike our other instructions, these instrs (especially CAS) need sreg2's value,
      //so we call the wider version of set_regs_val() again.
      instr->set_regs_val(sreg0, sreg1, sreg2, sreg3, sacc, dreg);

      instr->set_result_RF(
        dreg.data.u32,  /* Read/Write value */ //garbage
        dreg.addr,  /* Read Write register*/
        sreg0.data.u32   /* EA of the atomic op */
      );
      //FIXME i'm not sure if we need this.  the result of an atomic op is not known until
      //the MEM stage, where the atomic op actually completes.  We call set_result_RF() there
      //too, which overwrites this garbage value with the correct answer.
      //We may at least need the effective address.
      goto done_execute;
    }
     case (I_ATOMXCHG):
     case (I_ATOMINC):
     case (I_ATOMDEC):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      uint32_t addr;

      addr = sreg1.data.u32 + (instr->get_simm());
      //FIXME i'm not sure if we need this.  the result of an atomic op is not known until
      //the MEM stage, where the atomic op actually completes.  We call set_result_RF() there
      //too, which overwrites this garbage value with the correct answer.
      //We may at least need the effective address.
      instr->set_result_RF(
        sreg0.data.u32,
        dreg.addr,
        addr
      );
      goto done_execute;
    }
     case (I_STC):
    {
      IS_BYPASS_SREG0_SREG1()
      uint32_t addr;
      addr = sreg0.data.u32;
      instr->set_result_RF(
        sreg1.data.u32,
        dreg.addr,
        addr
      );
      // arb ahead of time if we will miss L1 (or if we don't have an L1)
      core->get_cluster()->getCacheModel()->ArbEarly(
        addr,
        core->get_core_num_local(),
        instr->get_local_thread_id(),
        L2_WRITE
      );

      goto done_execute;
    }
    // EXECUTE: We need the value to globally broadcast
    case (I_BCAST_UPDATE):
    case (I_STW):
    case (I_GSTW):
    /* Stall cycle store is only allowed to writeback once it decrements tall
     * signal  The memory system has accepted it at this point so if a load is
     * in one of the other pipes of this core, it will work properly
     */
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()

      uint32_t addr;
      addr = sreg1.data.u32 + (instr->get_simm());
      instr->set_result_RF(
        sreg0.data.u32,
        dreg.addr,
        addr
      );

      // arb ahead of time if we will miss L1 (or if we don't have an L1)
      core->get_cluster()->getCacheModel()->ArbEarly(
        addr,
        core->get_core_num_local(),
        instr->get_local_thread_id(),
        L2_WRITE
      );

      goto done_execute;
    }
    case (I_MB):
    {
      instr->set_result_RF(
        VOID_RESULT,
        NULL_REG
      );
      goto done_execute;
    }
     case (I_BRK):
    {
      instr->set_result_RF(
        VOID_RESULT,
        NULL_REG
      );
      goto done_execute;
    }
    case (I_FADD):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()
      float fres = sreg1.data.f32 + sreg0.data.f32;
#ifdef NAN_ASSERT
      assert (res != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_FSUB):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()
      float fres = sreg0.data.f32 - sreg1.data.f32;
#ifdef NAN_ASSERT
      assert (res != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_FMUL):
    {
      // Get bypass results
      IS_BYPASS_SREG0_SREG1()
      float fres = sreg0.data.f32 * sreg1.data.f32;
#ifdef NAN_ASSERT
      assert (res != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
     case (I_FR2A):
     {

      if (instr->is_bypass_sreg0()) { sreg0.data.u32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg0.addr); }
      float f = (sreg0.data.f32);
      instr->set_result_RF(
        f,
        dacc.addr
      );
      core->get_ac_scoreboard(instr->get_core_thread_id())->bypass_reg(
              instr->get_result_reg().addr, 
              instr->get_result_reg().data.f32,
              false);  
      goto done_execute;
    }
     case (I_FA2R):
     {

      if (instr->is_bypass_sacc()) { sacc.data.f32 = core->get_ac_scoreboard(instr->get_core_thread_id())->get_bypass(sacc.addr, false); }
      float f = sacc.data.f32;
      instr->set_result_RF(
        f,
        dreg.addr
      );
      core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(
              instr->get_result_reg().addr, 
              instr->get_result_reg().data.u32
              );  
      goto done_execute;
    }
     case (I_FMADD):
    {
      // Get bypass results
//      IS_BYPASS_SREG0_SREG1_SACC()
      if (instr->is_bypass_sreg0()) { sreg0.data.u32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg0.addr); }
      if (instr->is_bypass_sreg1()) { sreg1.data.u32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg1.addr); }
// Uncomment the following line to use a separate accumulator file
// if (instr->is_bypass_sacc()) { sacc.data.f32 = core->get_ac_scoreboard(instr->get_core_thread_id())->get_bypass(sacc.addr, false); }
// Comment the next line to use a separate accumulator file
      if (instr->is_bypass_sacc()) { sacc.data.u32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sacc.addr, true); }
      float fres = sacc.data.f32 + (sreg0.data.f32 * sreg1.data.f32);

#ifdef NAN_ASSERT
      assert (fres != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dacc.addr
      );
      // Update scoreboard with values just calculated
      core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(
              instr->get_result_reg().addr, 
              instr->get_result_reg().data.u32,
              true);  

      goto done_execute;
    }
     case (I_FMSUB):
    {
      if (instr->is_bypass_sreg0()) { sreg0.data.u32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg0.addr); }
      if (instr->is_bypass_sreg1()) { sreg1.data.u32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg1.addr); }
// Uncomment the following line to use a separate accumulator file
// if (instr->is_bypass_sacc()) { sacc.data.f32 = core->get_ac_scoreboard(instr->get_core_thread_id())->get_bypass(sacc.addr, false); }
// Comment the next line to use a separate accumulator file
      if (instr->is_bypass_sacc()) { sacc.data.u32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sacc.addr, true); }
      float fres = sacc.data.f32 - (sreg0.data.f32 * sreg1.data.f32);

#ifdef NAN_ASSERT
      assert (fres != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dacc.addr
      );
      // Update scoreboard with values just calculated
      core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(
              instr->get_result_reg().addr, 
              instr->get_result_reg().data.u32,
              true);  

      goto done_execute;
    }
    case (I_FRCP):
    {
      // Get bypass results
      IS_BYPASS_SREG0()
      float fres = 1.0 / sreg0.data.f32;
#ifdef NAN_ASSERT
      assert (fres != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
     case (I_FRSQ):
    {
      // Get bypass results
      IS_BYPASS_SREG0()
      float fres =   1.0f / (float)sqrtf(sreg0.data.f32);
#ifdef NAN_ASSERT
      assert (fres != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_FABS):
    {
      // Get bypass results
      IS_BYPASS_SREG0()
      float fres = fabs((sreg0.data.f32));
#ifdef NAN_ASSERT
      assert (fres != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
     case (I_FMRS):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      instr->set_result_RF(
        sreg0.data.f32,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }

    case (I_F2I):
    {
      // Get bypass results
      IS_BYPASS_SREG0()

      float f = (sreg0.data.f32);
      instr->set_result_RF(
        (int32_t)f,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
     case (I_I2F):
    {
      // Get bypass results
      IS_BYPASS_SREG0()
      float fres = (float)((int32_t)(sreg0.data.i32));
#ifdef NAN_ASSERT
      assert (fres != 0xffc00000 && "NaN FOUND!!");
#endif

      instr->set_result_RF(
        fres,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_TBLOFFSET):
    {
      // Get bypass results
      IS_BYPASS_SREG0()
      // Calculate the offset into the table based on the target address passed
      // to tbloffset.
      uint32_t offset
        = HybridDirectory::calc_frt_access_table_offset(sreg0.data.u32)
          * sizeof(uint32_t);

      instr->set_result_RF(
        offset,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_CLZ):
    {
      //sb->get_reg(dreg, instr_type);
      // Get bypass results
      IS_BYPASS_SREG0()

      uint32_t val = sreg0.data.u32;
      uint32_t lz;

      for (lz = 0; lz < 32; ) {
        if (val & 0x80000000) { break; }
        val = val << 1;
        lz++;
      }
      instr->set_result_RF(
        lz,
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_MUL):
    {
      //sb->get_reg(dreg, instr_type);
      /* 16bx16b unsigned multiply multiply */
      // FIXME: A 32b integer multiply makes life easy
  //    const uint32_t MASK_LOW_16 = 0x0000FFFF;
      const uint32_t MASK_LOW_16 = 0xFFFFFFFF;

      IS_BYPASS_SREG0_SREG1()
      instr->set_result_RF(
        (MASK_LOW_16 & sreg0.data.u32) *
        (MASK_LOW_16 & sreg1.data.u32),
        dreg.addr
      );
      // Update scoreboard with values just calculated
      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_SEXTS):
    {
      IS_BYPASS_SREG0()

      if(sreg0.data.u32 & 0x8000)
      {
          instr->set_result_RF(sreg0.data.u32| 0xFFFF0000, dreg.addr);
      }
      else
      {
          instr->set_result_RF(0x0000FFFF & sreg0.data.u32, dreg.addr);
      }

      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_ZEXTS):
    {
      IS_BYPASS_SREG0()

      uint32_t val = sreg0.data.u32;
      val &= 0x0000FFFF;
      instr->set_result_RF(val, dreg.addr);

      UPDATE_SCOREBOARD();

      goto done_execute;
    }
    case (I_SEXTB):
    {
      IS_BYPASS_SREG0()

      if(sreg0.data.u32 & 0x80)
      {
        instr->set_result_RF(sreg0.data.u32 | 0xFFFFFF00, dreg.addr);
      }
      else
      {
        instr->set_result_RF(0x000000FF & sreg0.data.u32, dreg.addr);
      }

      UPDATE_SCOREBOARD()

      goto done_execute;
    }
    case (I_ZEXTB):
    {
      IS_BYPASS_SREG0()

      uint32_t val = sreg0.data.u32;
      val &= 0x000000FF;
      instr->set_result_RF(val, dreg.addr);

      UPDATE_SCOREBOARD();

      goto done_execute;
    }
    case (I_SYSCALL):
    {
    /*
     *  Defined in core.h
     *
     * struct syscall_struct {
     *   __uint32_t syscall_num;
     *   __uint32_t result;
     *   __uint32_t arg1;
     *   __uint32_t arg2;
     *   __uint32_t arg3;
     * };
     */

    // The syscalls are located in /lib/libc/unix_calls.c

      IS_BYPASS_SREG0()

      // We need the source register at MEM to slurp the syscall struct
      // from memory
      instr->set_regs_val(sreg0, RegisterBase(), RegisterBase(), RegisterBase(),RegisterBase());
      // FIXME  Remove if we do not return anything from system calls TBD
      // Actual execution should happen at the MEM stage

      goto done_execute;
    }
    case (I_PRINTREG):
    {
      IS_BYPASS_SREG0()

      // Enable supressing RigelPrint() output.
      if (!rigel::CMDLINE_SUPPRESS_RIGELPRINT) {
        // Enable short RigelPrint() printing for faster consumption by
        // scripts.
        if (rigel::CMDLINE_SHORT_RIGELPRINT) {
          // CYCLE, PC, DATA.u32 (hex), DATA.u32 (dec), CORE
					//FIXME Why not also print thread?
          fprintf(rigel::RIGEL_PRINT_FILEP, "0x%012llx, 0x%08x, 0x%08x, %9"PRIu32", %9"PRId32", %d\n",
            (unsigned long long)rigel::CURR_CYCLE,
            instr->get_currPC(),
            sreg0.data.u32,
						sreg0.data.u32,
						sreg0.data.i32,
            core->get_core_num_global());
        } else {
          fprintf(rigel::RIGEL_PRINT_FILEP,
            "0x%012llx (PC 0x%08x) PRINTREG: 0x%08x %09"PRIu32" %09"PRId32" (%f f) core: %d thread: %d\n",
            (unsigned long long)rigel::CURR_CYCLE,
            instr->get_currPC(),
            sreg0.data.u32, sreg0.data.u32,
						sreg0.data.i32,
						sreg0.data.f32,
            core->get_core_num_global(),
            instr->get_core_thread_id());
        }
      }
      goto done_execute;
    }
    // Timers converted from system calls into single instructions.  The
    // return value is meaningless and once executed, the instruction is
    // complete so our job here is done.
    case (I_START_TIMER):
    {
      IS_BYPASS_SREG0()
      struct syscall_struct sc_data;
      sc_data.arg1.u = sreg0.data.u32;

      core->system_calls->doSystemCall(sc_data);

      goto done_execute;
    }
    case (I_STOP_TIMER):
    {
      IS_BYPASS_SREG0()
      struct syscall_struct sc_data;
      sc_data.arg1.u = sreg0.data.u32;

      core->system_calls->doSystemCall(sc_data);

      goto done_execute;
    }
    default:
    {
      printf("BAD ITYPE! %d\n", instr_type);
      goto bad_itype;
    }
  }
}
done_execute:

  using namespace rigel;
  // TODO
  // We could manage resource conflicts here (and see the corresponding TODO in
  // regfile_read) by clearing the bitmap associated with a busy functional unit.
  if (instr->get_is_vec_op()) core->set_vector_op_pending(false, instr->get_core_thread_id());

  // Always assume PC+4, if not, trigger a mispredict
  //  mispredict = (new_pc != (instr->get_currPC() + 4));

  mispredict = (new_pc != (instr->get_predNextPC()));
  if (mispredict) {

    instr->set_nextPC(new_pc); // Probably not needed any longer
    instr->set_mispredict();
    //Now set for every instruction on writeback.
    //NOTE: I think this used to be used for checkpointing, so it's possible if we want that again we want
    //to only set last PC on mispredicts, but for the time being -heartbeat_print_pcs <N> uses last_pc to
    //tell you where the core is, and it would be good to know exactly what the last instruction you committed
    //was rather than the last mispredicted branch.
    //core->set_last_pc( instr->get_currPC(), instr->get_core_thread_id() ); // Probably not needed any longer

    // Set the fixup PC for the branch and notify IF
    core->get_thread_state(instr->get_core_thread_id())->EX2IF.FixupPC = new_pc;
    assert((false == core->get_thread_state(instr->get_core_thread_id())->EX2IF.FixupNeeded)
      && "Must be cleared by IF before reuse!");
    core->get_thread_state(instr->get_core_thread_id())->EX2IF.FixupNeeded = true;
    // Send the mispredicted PC back up to IF to insert into BTR.
    core->get_thread_state(instr->get_core_thread_id())->EX2IF.MispredictPC = instr->get_currPC();
  }

  // Tabulate branch misprediction statistics.
  if (instr->instr_is_branch()) {
    profiler::stats[STATNAME_BRANCH_TARGET].inc_pred_total();
    true == mispredict ?
      profiler::stats[STATNAME_BRANCH_TARGET].inc_pred_mispredicts() :
      profiler::stats[STATNAME_BRANCH_TARGET].inc_pred_correct();
  }


  instr->set_doneExec();
  core->get_cluster()->getProfiler()->timing_stats.ex_active++;

  // Instruction is completed.  Release resource.  TODO: Clean this up so it is
  // a member of InstrLegacy()
//  core->PipeCoreState.funit_available_count[INSTR_FUNIT[instr_type]]++;

  return instr;

bad_itype:
  //fprintf(stderr,"Core: %d\n",core->get_core_num_global());
  throw ExitSim("execute: Unknown Instruction type!", 1);

}
