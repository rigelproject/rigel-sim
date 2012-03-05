////////////////////////////////////////////////////////////////////////////////
// regfile_read.cpp
////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Method Name: regfile_read()
////////////////////////////////////////////////////////////////////////////////
// a pseudo-stage (called at beginning of execute right now)
//
// register file is read for the provided instruction, results of the read are
// stored in the input instr
////////////////////////////////////////////////////////////////////////////////
#include <assert.h>                     // for assert
#include <stdio.h>                      // for fprintf, stderr
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy
#include "define.h"         // for InstrSlot, DEBUG_HEADER
#include "instr.h"          // for InstrLegacy
#include "profile/profile.h"        // for InstrStat, Profile
#include "core/regfile_legacy.h"        // for RegisterBase, RegisterFile
#include "sim.h"            // for NullInstr
#include "include/stage_base.h"  // for ExecuteStage
#include "core/scoreboard.h"     // for ScoreBoard (in included scoreboard*.h)
#include "util/util.h"           // for ExitSim (in included scoreboard*.h)

InstrSlot CoreInOrderLegacy::regfile_read(InstrSlot instr, int pipe) {
  if (!instr->GetDoneRegRead()) {
    // XXX BEGIN SCOREBOARD XXX
    using namespace simconst;

    // FIXME, why does this get set here instead of in the decode switch?
    instr_t instr_type = instr->get_type();
    if (instr_type == I_ATOMCAS) {
      instr->set_sreg2(instr->get_dreg());
    }
    // Vector operations have their proper registers set inside
    // scoreboard_read.h and scoreboard_write.h for sources and destinations,
    // respectively.
    #include "autogen/scoreboard_read.h"
    // Check whether we have enough register ports free to issue (issues with
    // 3-operand instructions like FMAC)
    if(get_regfile(instr->get_core_thread_id())->get_num_free_read_ports() < instr->get_num_read_ports()) {
      // Not enough free ports to issue the instruction
      goto ExecuteStalledReturn;
    }

    // TODO
    //
    // We could add an occupancy count to the InstrLegacy class to mimic multi-cycle
    // instructions in execute.  I had to do this as a kludgy hack for vector
    // operations for the meanwhile.  We could also modify execute here to let
    // vector operations drain through the pipeline instead of having them commit en
    // masse as I am having them do for now.  The second option is more realistic
    // since a vector operation on Rigel would just be simple vector operations
    // post-decode.
    //
    // Also note that this uses a form of pipeline restriction that we should flesh
    // out more for other operations.  We could keep a vector of allowable
    // combinations and set/clear bitmaps corresponding to available/unavailable
    // units here and at the end of execute, respectively.
    if (instr->get_is_vec_op()) {
      // Check to see if there is already a vector operation in flight.
      if (get_vector_op_pending(instr->get_core_thread_id())) { goto ExecuteStalledReturn; }
      // Tick away one cycle on the vector operation
      instr->execute_vec_op();
      if (!instr->completed_vec_op()) { goto ExecuteStalledReturn; }
      // Set that there is a vector operation.  Unset at the end of EX.
      set_vector_op_pending(false, instr->get_core_thread_id());
    }
    // Check if destination register is locked.  If so, we cannot issue this
    // instruction this cycle.  We are being a bit conservative here.  The list
    // seen here is copied from writeback stage
     
    #include "autogen/scoreboard_write.h"
    // FIXME: insert scoreboard_write() instead of inline include...

    //if(get_core_num_global() == 0) printf("Pipe %d Made it past SB write\n", pipe);

    execute_stage->unstall(pipe);
    instr->SetDoneRegRead();
    if (instr->is_done_regaccess()) {
    #ifdef DEBuG
      cerr << "RG: currPC 0x" << HEX_COUT << instr->get_currPC() << " Access already completed" << endl;
    #endif
      //if(get_core_num_global() == 0) printf("Pipe %d is_done_regaccess()\n", pipe);
      return instr;
    }
    // These are inlined anyway, but for clarity's sake
    RegisterBase sreg0 = instr->get_sreg0();
    RegisterBase sreg1 = instr->get_sreg1();
    RegisterBase sreg2 = instr->get_sreg2();
    RegisterBase sreg3 = instr->get_sreg3(); //  Modified for vectors
    RegisterBase sacc = instr->get_sacc();   // Modified for Accumulator instructions
    // Note that we can overwrite these since WB uses the result_RF(_rnum) values
    RegisterBase dreg = instr->get_dreg();
    RegisterBase dacc = instr->get_dacc();
    // Added for vectors
    RegisterBase sreg4 = instr->get_sreg4();
    RegisterBase sreg5 = instr->get_sreg5();
    RegisterBase sreg6 = instr->get_sreg6();
    RegisterBase sreg7 = instr->get_sreg7();
    RegisterBase dreg1 = instr->get_dreg1();
    RegisterBase dreg2 = instr->get_dreg2();
    RegisterBase dreg3 = instr->get_dreg3();

    //cerr << "sreg0: 0x" << HEX_COUT << sreg0 << endl;
    //cerr << "sreg1: 0x" << HEX_COUT << sreg1 << endl;

    /* We have access to our sources, time to execute! */
    #include "autogen/scoreboard_update.h"

    //if(get_core_num_global() == 0) printf("Pipe %d Made it past SB update\n", pipe);

done_regread:
    // Update the values carried with the instruction.  May or may not overwrite
    // the original register number that is stored 
    instr->set_regs_val(sreg0, sreg1, sreg2, sreg3, sacc, dreg);
    // Used by the vector instructions.  TODO: We may want to change this so we
    // can pipeline/chain vectors appropriately.
    if (instr->get_is_vec_op()) {
      instr->set_regs_val_vec(sreg4, sreg5, sreg6, sreg7, dreg1, dreg2, dreg3);
    }
    instr->set_done_regaccess();

    return instr;
  }

bad_itype:
  DEBUG_HEADER();
  fprintf(stderr, "itype: %d", instr->get_type());
  assert(0 && "regfile_read: Bad Instruction Type Found");

ExecuteStalledReturn:
  //if(get_core_num_global() == 0) printf("Pipe %d stalling\n", pipe);
  execute_stage->stall(pipe);
  get_cluster()->getProfiler()->timing_stats.ex_stall++;
  return rigel::NullInstr;
}


