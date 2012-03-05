////////////////////////////////////////////////////////////////////////////////
// fp_complete.cpp
////////////////////////////////////////////////////////////////////////////////
//
// fp_complete stage implementations
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Method Name: FPCompleteStage::Update()
////////////////////////////////////////////////////////////////////////////////
// Description:
//
// Inputs: none
// Outputs: none
////////////////////////////////////////////////////////////////////////////////
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy, ::FP2CC, ::MC2FP
#include "define.h"         // for InstrSlot
#include "instr.h"          // for InstrLegacy
#include "profile/profile.h"        // for InstrStat, Profile
#include "sim.h"            // for ISSUE_WIDTH, NullInstr
#include "stage_base.h"  // for FPCompleteStage

void 
FPCompleteStage::Update() 
{
  // for each pipe
  for (int j = 0; j < rigel::ISSUE_WIDTH; j++) {
    InstrSlot instr_next = fpcomplete(core->latches[MC2FP][j], j);

    //int tid = instr_next->get_core_thread_id();
    // pointer to thread state
    //pipeline_per_thread_state_t *ts = core->get_thread_state(tid);

    if (!is_stalled(j)) { 
      if ( core->nlatches[FP2CC][j]->get_type() != I_NULL ) { 
        // Account for "skipped" cores[i+x]->stage_name_here(latch...) calls
        core->get_cluster()->getProfiler()->timing_stats.fp_block += (rigel::ISSUE_WIDTH - j - 1);  
        break; 
      }
      core->latches[MC2FP][j] = rigel::NullInstr;
      core->nlatches[FP2CC][j] = instr_next;
    }

  }

}

////////////////////////////////////////////////////////////////////////////////
// Method Name: FPCompleteStage::fpcomplete()
////////////////////////////////////////////////////////////////////////////////
// Description:
//
// Inputs: instr, pipe
// Outputs: instr (same as input instr)
////////////////////////////////////////////////////////////////////////////////
InstrSlot 
FPCompleteStage::fpcomplete(InstrSlot instr, int pipe) {

		/* STALL */
		if (instr->get_type() == I_NULL) {
			core->get_cluster()->getProfiler()->timing_stats.fp_bubble++;
			return instr;
		}

		// Did any pipe prior to this one in program order stall?  If so, we stall too 
		if (pipe > 0 && is_stalled(pipe-1)) {
			core->get_cluster()->getProfiler()->timing_stats.fp_block++;
			return instr;
		}

/*
  instr_t instr_type = instr->get_type();
  switch (instr_type) {
    case I_FADD:
    {
      
      UPDATE_SCOREBOARD()
      break;
    }
    default:
      break;
  }
  */
	core->get_cluster()->getProfiler()->timing_stats.fp_active++;

	return instr;

}

