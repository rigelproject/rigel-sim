
////////////////////////////////////////////////////////////////////////////////
// core.cpp
////////////////////////////////////////////////////////////////////////////////
//
// this file implements the core class, CoreInOrderLegacy
//
////////////////////////////////////////////////////////////////////////////////


#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu64
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for printf, NULL, stdout, etc
#include <string.h>                     // for strlen
#include <fstream>                      // for operator<<, ofstream, etc
#include <iostream>                     // for std::cout
#include <string>                       // for char_traits, string
#include <limits>
#include "../user.config"   // for ENABLE_VERIF
#include "core/bpredict.h"       // for BranchPredictorBase
#include "core/btb.h"            // for BranchTargetBuffer, etc
#include "cluster.h"        // for Cluster, etc
#include "core.h"           // for CoreInOrderLegacy, etc
#include "memory/dram.h"           // for BANKS, CONTROLLERS, RANKS, etc
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrCycleStats, InstrStats
#include "locality_tracker.h"  // for LocalityTracker, etc
#include "core/regfile_legacy.h"        // for SpRegisterFileLegacy, etc
#include "rigellib.h"       // for ::SPRF_CLUSTER_ID, etc
#include "core/scoreboard.h"     // for ScoreBoard, etc
#include "sim.h"            // for THREADS_PER_CORE, NullInstr, etc
#include "util/syscall.h"        // for Syscall
#include "util/ui_legacy.h"             // for UserInterfaceLegacy
#include "stage_base.h"  // for CCacheAccessStage, etc
#include "tlb.h"            // for TLB
#include "util/construction_payload.h"

////////////////////////////////////////////////////////////////////////////////
// is fetch_stalled()
////////////////////////////////////////////////////////////////////////////////
// Additions from reorginization for cyclical include files
////////////////////////////////////////////////////////////////////////////////
bool CoreInOrderLegacy::is_fetch_stalled() const { 
  return decode_stage->is_stalled(rigel::ISSUE_WIDTH-1);
}

////////////////////////////////////////////////////////////////////////////////
// get_regfile() 
////////////////////////////////////////////////////////////////////////////////
// returns a pointer to the register file for the given thread ID
RegisterFileLegacy* CoreInOrderLegacy::get_regfile(int tid) const { 
  return regfiles[tid];
}

////////////////////////////////////////////////////////////////////////////////
// get_sprf() 
////////////////////////////////////////////////////////////////////////////////
// returns a pointer to the sprf (special purpose reg file)
SpRegisterFileLegacy* CoreInOrderLegacy::get_sprf(int tid) const { 
  return sprfs[tid]; 
}
////////////////////////////////////////////////////////////////////////////////
// get_accumulator_regfile() 
////////////////////////////////////////////////////////////////////////////////
// returns a pointer to the sprf (special purpose reg file)
RegisterFileLegacy* CoreInOrderLegacy::get_accumulator_regfile(int tid) const { 
  return accfiles[tid]; 
}

////////////////////////////////////////////////////////////////////////////////
// CoreInOrderLegacy Constructor
////////////////////////////////////////////////////////////////////////////////
// a more complete constructor, actually used in practice
////////////////////////////////////////////////////////////////////////////////
CoreInOrderLegacy::CoreInOrderLegacy(rigel::ConstructionPayload cp, ClusterLegacy *_cluster) :
  CoreBase(cp.change_name("CoreInOrderLegacy")), // base constructor
  system_calls(cp.syscall),
  core_num(cp.component_index),
  cluster(_cluster),
  last_fetch_instr_num(std::numeric_limits<uint64_t>::max()),
  last_commit_cycle(0),
  fetch_thread_id(0) //set starting thread to local thread 0
{
	cp.parent = this;
	cp.component_name.clear();

  fetch_stage     = new FetchStage(this);
  decode_stage    = new DecodeStage(this);
  execute_stage   = new ExecuteStage(this);
  mem_stage       = new MemoryStage(this);
  fpaccess_stage  = new FPCompleteStage(this);
  ccache_stage    = new CCacheAccessStage(this);
  writeback_stage = new WriteBackStage(this);

  for(int i = 0; i < rigel::ISSUE_WIDTH; i++)
  {
    fetch_stall_counter[i] = 0;
    fetch_stall_pc[i]      = 0;
  }

  ///////////////////////////////////
  // Pipeline Stages
  ///////////////////////////////////
  // FIXME: the enum for these is in core.h, should be a global enum
  for( int i = 0; i < NUM_STAGES; i++ ) {
    if(i == FETCH_STAGE)
      stages[i] = fetch_stage;
    if(i == DECODE_STAGE)
      stages[i] = decode_stage;
    if(i == EXECUTE_STAGE)
      stages[i] = execute_stage;
    if(i == MEM_ACCESS_STAGE)
      stages[i] = mem_stage; 
    if(i == FP_COMPLETE_STAGE)
      stages[i] = fpaccess_stage;
    if(i == CACHE_ACCESS_STAGE)
      stages[i] = ccache_stage;
    if(i == WRITE_BACK_STAGE)
      stages[i] = writeback_stage;
  }

  ///////////////////////////////////
  // register files
  ///////////////////////////////////
  // standard register file 
  //regfile  = new RegisterFileLegacy(rigel::NUM_REGS);
  // array of regfiles, one per thread
  regfiles = new RegisterFileLegacy*[rigel::THREADS_PER_CORE];
  for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
    regfiles[i] = new RegisterFileLegacy(rigel::NUM_REGS);
  }

  ///////////////////////////////////
  // accumulator files
  ///////////////////////////////////
  // standard register file-like structure
  // array of accregfiles, one per thread
  accfiles = new RegisterFileLegacy*[rigel::THREADS_PER_CORE];
  for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
    accfiles[i] = new RegisterFileLegacy(rigel::NUM_AREGS);
  }

  // FIXME: This code is FUGLY
  // special purpose register file
  // NOTE: this does not need to be per-thread except the thread ID 
  // (but it is easier to dupe it all than seperate right now - in HW this can
  // be combined)
  sprfs = new SpRegisterFileLegacy*[rigel::THREADS_PER_CORE];
  for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
    sprfs[i] = new SpRegisterFileLegacy(rigel::NUM_SREGS);
    /* Set SPR0 to the core number at boot */
    sprfs[i]->write(core_num,SPRF_CORE_NUM);
    /* Set SPR1 to number of cores total at boot */
    sprfs[i]->write(rigel::CORES_TOTAL,SPRF_CORES_TOTAL);
    /* Set SPR2 to number of cores per cluster at boot */
    sprfs[i]->write(rigel::CORES_PER_CLUSTER,SPRF_CORES_PER_CLUSTER);
    /* Set SPR4 to the cluster number */
    sprfs[i]->write(cluster->GetClusterID(),SPRF_CLUSTER_ID);
    /* Set SPR5 to the number of clusters */
    sprfs[i]->write(rigel::NUM_CLUSTERS, SPRF_NUM_CLUSTERS);
    /* Set SPR6 to the thread number - TODO: does this change dynamically? or
     * multiple SPRFs/core for multithreading?*/
    // right now, this is the CURRENT_THREAD's THREAD_NUM, changes at runtime
    // initialize to the first threadnum
    int tid = rigel::THREADS_PER_CORE * core_num + i;
    sprfs[i]->write(tid,SPRF_THREAD_NUM);
    /* Set SPR7 to the total number of threads in the system */
    sprfs[i]->write(rigel::THREADS_TOTAL,SPRF_THREADS_TOTAL);
    /* Set SPR8 to the number of threads per core */
    sprfs[i]->write(rigel::THREADS_PER_CORE,SPRF_THREADS_PER_CORE);
    /* Set SPR3 to the number of threads per cluster */
    sprfs[i]->write(rigel::THREADS_PER_CORE*rigel::CORES_PER_CLUSTER,
      SPRF_THREADS_PER_CLUSTER);
    // Add SPRF for checking if incoherent malloc is enabled.
    sprfs[i]->write(rigel::CMDLINE_ENABLE_INCOHERENT_MALLOC,
      SPRF_ENABLE_INCOHERENT_MALLOC);
		// Add SPRF for sim only execution
		sprfs[i]->write(1,SPRF_IS_SIM);
    // ARGC, ARGV
    sprfs[i]->write(1,SPRF_ARGC); /// FIXME: remove, overridden below?
    sprfs[i]->write(1,SPRF_ARGV); /// FIXME: ditto
    //Write argc, argv values
    // will overwrite INIT_REGISTER_FILE?
    //NOTE: argv will still be in memory, but this should be OK, since it's at the top of
    //the code segment and you shouldn't be reading (from software's point of view)
    //uninitialized memory anyway.
    sprfs[i]->write(rigel::TARGET_ARGS.size(), SPRF_ARGC);
    sprfs[i]->write(0x400000-(4*rigel::TARGET_ARGS.size()), SPRF_ARGV);

  }
  
  char out_file[100];
  if(rigel::DUMP_REGISTER_TRACE){
    reg_trace_file = new std::ofstream[rigel::THREADS_PER_CORE];
    for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
      sprintf(out_file,"%s/rf.%d.%d.trace",rigel::DUMPFILE_PATH.c_str(),core_num,i);
      reg_trace_file[i].open(out_file);
    }
  }


  ///////////////////////////////////
  ///////////////////////////////////
  ///////////////////////////////////
  // pipeline shared state
  ///////////////////////////////////
  // put all this in an init()
  PipeThreadState = new PipelineThreadState[rigel::THREADS_PER_CORE];
  for(int i=0; i<rigel::THREADS_PER_CORE;i++) {
    int tid = core_num*rigel::THREADS_PER_CORE + i;
    PipeThreadState[i].last_fetch.clear();
		PipeThreadState[i].last_pc                = ((uint32_t)rigel::ENTRY_POINTS[tid] - 4);
    PipeThreadState[i]._halt                  = false;
    PipeThreadState[i].pending_load_access    = false;
    PipeThreadState[i].last_pending_addr      = 0xffffffff; // This address is never valid so it will work
    PipeThreadState[i].watchdog               = rigel::DEADLOCK_WATCHDOG;
    PipeThreadState[i].vector_op_pending      = false;
    PipeThreadState[i].EX2IF.FixupPC          = 0xFFFFFFFF;
    PipeThreadState[i].EX2IF.FixupNeeded      = false;
    PipeThreadState[i].EX2IF.MispredictPC     = 0xFFFFFFFF;
		PipeThreadState[i].IF2IF.NextPC           = rigel::ENTRY_POINTS[tid];
    PipeThreadState[i].IF2IF.UnalignedPending = false;
    PipeThreadState[i].IF2IF.BTRHitPending    = false;
    PipeThreadState[i].pending_sync_req       = false;
    PipeThreadState[i].pending_sync_ack       = false;
    PipeThreadState[i].nonblocking_atomics_enabled = false;

    PipeThreadState[i].SpecInstr.ListLength   = rigel::ISSUE_WIDTH*3;
    PipeThreadState[i].SpecInstr.ListHead     = 0;
    // Either a speculative instruction should be in the list or a pointer to the
    // NullInstr.  Anything else is bad.
    for (int j = 0; j < PipeThreadState[i].SpecInstr.ListLength; j++) {
      PipeThreadState[i].SpecInstr.List[j] = rigel::NullInstr;
    }
    PipeThreadState[i].btb = new BranchTargetBuffer;
    // Set up the branch target register (BTR)
    PipeThreadState[i].btb->init();
    PipeThreadState[i].req_to_fetch=true;
    // force sim to start with thread 0, don't switch at time 0
    if (i==0) {
      PipeThreadState[i].waiting_to_fetch=true;
    } else {
      PipeThreadState[i].waiting_to_fetch=false;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
	// Halt all threads except 0 if in single-threaded mode
	//////////////////////////////////////////////////////////////////////////////
	if(rigel::SINGLE_THREADED_MODE) {
		for(int i=0; i<rigel::THREADS_PER_CORE;i++) {
			if(core_num == 0 && i == 0) //Don't halt thread 0
				continue;
			else PipeThreadState[i]._halt = true;
		}
	}


  //////////////////////////////////////////////////////////////////////////////
  // PipeCoreState init
  //////////////////////////////////////////////////////////////////////////////
  // FIXME: this would ideally be a class and initialize itself (TODO)
  PipeCoreState.schedule_table = new int[rigel::THREADS_PER_CORE];
  //for(int ii=0; ii<rigel::THREADS_PER_CORE; ii++) {
  //  PipeCoreState.schedule_table[ii]=1;
  //}
  PipeCoreState.net_priority = 0;

  ///////////////////////////////////
  ///////////////////////////////////
  ///////////////////////////////////

  // keep one scoreboard per thread (regular, special purpose, and accumulator) 
  scoreboards = new ScoreBoard*[rigel::THREADS_PER_CORE];
  sp_sbs      = new ScoreBoard*[rigel::THREADS_PER_CORE];
  ac_sbs      = new ScoreBoard*[rigel::THREADS_PER_CORE];
  for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
    scoreboards[i] = new ScoreBoardBypass(rigel::NUM_REGS);
    sp_sbs[i]      = new ScoreBoardBypass(rigel::NUM_SREGS);
    ac_sbs[i]      = new ScoreBoardBypass(rigel::NUM_AREGS);
  }

  /* Change branch predictor type here */
  // NOTE: this no longer has ANY clear function and MAY or MAY NOT be used for
  // anything at all. The structure appears to be update from execute still but
  // does not seem to be used for anything else.
  //bp = new BranchPredictorGShare();
  //this->bp = new BranchPredictorNextPC();
 
  // initialize latches to NULL 
  for( int j=0; j<NUM_STAGE_LATCHES; j++ ) { 
    for (int i = 0; i < rigel::ISSUE_WIDTH; i++) {
      latches[j][i]  = rigel::NullInstr;
      nlatches[j][i] = rigel::NullInstr;
    }
  }
 
  // Set up functional unit complement.  They are checked at the start of
  // execute and decremented.  They are incremented when the unit is free.
  // TODO: Push the increment/decrement stuff into autogen.
  PipeCoreState.funit_available_count[FU_NONE] = -1; // Ignored
  PipeCoreState.funit_available_count[FU_ALU] = 1;
  PipeCoreState.funit_available_count[FU_ALU_VECTOR] = 1;
  PipeCoreState.funit_available_count[FU_FPU] = 1;
  PipeCoreState.funit_available_count[FU_FPU_VECTOR] = 1;
  PipeCoreState.funit_available_count[FU_ALU_SHIFT] = 1;
  PipeCoreState.funit_available_count[FU_SPFU] = 0;
  PipeCoreState.funit_available_count[FU_MEM] = 1;
  PipeCoreState.funit_available_count[FU_BRANCH] = 1;
  PipeCoreState.funit_available_count[FU_BOTH] = -1; // Ignored

  forceSwap = false;

  //Set up locality tracking
  if(rigel::ENABLE_LOCALITY_TRACKING)
  {
    perThreadLocalityTracker = new LocalityTracker *[rigel::THREADS_PER_CORE];
    for(int i = 0; i < rigel::THREADS_PER_CORE; i++)
    {
      char name[128];
      snprintf(name, 128, "Core %d Thread %d Raw Accesses", core_num, i);
      std::string cppname(name);
      perThreadLocalityTracker[i] = new LocalityTracker(cppname, 8, LT_POLICY_RR,
        rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
        rigel::DRAM::BANKS, rigel::DRAM::ROWS);
    }
    char name[128];
    snprintf(name, 128, "Core %d Raw Accesses", core_num);
    std::string cppname(name);
    aggregateLocalityTracker = new LocalityTracker(cppname, 8, LT_POLICY_RR,
      rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
      rigel::DRAM::BANKS, rigel::DRAM::ROWS);
  }

  //Set up TLBs
  if(rigel::ENABLE_TLB_TRACKING)
  {
    //num_tlbs = 5*3*3*1;
    //num_tlbs = (5+5+4+4+3+3+2+2+1)*6;
    num_tlbs=11*16;
    if(rigel::THREADS_PER_CORE > 1) {
      perThreadITLB= new TLB **[rigel::THREADS_PER_CORE];
      perThreadDTLB= new TLB **[rigel::THREADS_PER_CORE];
      perThreadUTLB= new TLB **[rigel::THREADS_PER_CORE];
      for(int t = 0; t < rigel::THREADS_PER_CORE; t++) {
        char name[128];
        snprintf(name, 128, "Core %d Thread %d Instructions", core_num, t);

        perThreadITLB[t] = new TLB *[num_tlbs];
        TLB **walker = perThreadITLB[t];
        for(unsigned int i = 0; i <= 0; i++) {
          for(unsigned int j = 0; j <= 10; j += 1) {
            for(unsigned int k = 5; k <= 20; k += 1) {
              *(walker++) = new TLB(name, (1 << i), (1 << j), k, LRU);
              //*(walker++) = new TLB(name, (1 << i), (1 << j), k, MRU);
              //*(walker++) = new TLB(name, (1 << i), (1 << j), k, LFU);
            }
          }
        }

        snprintf(name, 128, "Core %d Thread %d Data", core_num, t);

        perThreadDTLB[t] = new TLB *[num_tlbs];
        walker = perThreadDTLB[t];
        for(unsigned int i = 0; i <= 0; i++) {
          for(unsigned int j = 0; j <= 10; j += 1) {
            for(unsigned int k = 5; k <= 20; k += 1) {
              *(walker++) = new TLB(name, (1 << i), (1 << j), k, LRU);
              //*(walker++) = new TLB(name, (1 << i), (1 << j), k, MRU);
              //*(walker++) = new TLB(name, (1 << i), (1 << j), k, LFU);
            }
          }
        }

        snprintf(name, 128, "Core %d Thread %d Unified", core_num, t);

        perThreadUTLB[t] = new TLB *[num_tlbs];
        walker = perThreadUTLB[t];
        for(unsigned int i = 0; i <= 0; i++) {
          for(unsigned int j = 0; j <= 10; j += 1) {
            for(unsigned int k = 5; k <= 20; k += 1) {
              *(walker++) = new TLB(name, (1 << i), (1 << j), k, LRU);
              //*(walker++) = new TLB(name, (1 << i), (1 << j), k, MRU);
              //*(walker++) = new TLB(name, (1 << i), (1 << j), k, LFU);
            }
          }
        }
      }
    }

    char name[128];
    snprintf(name, 128, "Core %d Aggregate Instructions", core_num);

    aggregateITLB = new TLB *[num_tlbs];
    TLB **walker = aggregateITLB;
    for(unsigned int i = 0; i <= 0; i++) {
      for(unsigned int j = 0; j <= 10; j += 1) {
        for(unsigned int k = 5; k <= 20; k += 1) {
          *(walker++) = new TLB(name, (1 << i), (1 << j), k, LRU);
          //*(walker++) = new TLB(name, (1 << i), (1 << j), k, MRU);
          //*(walker++) = new TLB(name, (1 << i), (1 << j), k, LFU);
        }
      }
    }

    snprintf(name, 128, "Core %d Aggregate Data", core_num);

    aggregateDTLB = new TLB *[num_tlbs];
    walker = aggregateDTLB;
    for(unsigned int i = 0; i <= 0; i++) {
      for(unsigned int j = 0; j <= 10; j += 1) {
        for(unsigned int k = 5; k <= 20; k += 1) {
          *(walker++) = new TLB(name, (1 << i), (1 << j), k, LRU);
          //*(walker++) = new TLB(name, (1 << i), (1 << j), k, MRU);
          //*(walker++) = new TLB(name, (1 << i), (1 << j), k, LFU);
        }
      }
    }

    snprintf(name, 128, "Core %d Aggregate Unified", core_num);

    aggregateUTLB = new TLB *[num_tlbs];
    walker = aggregateUTLB;
    for(unsigned int i = 0; i <= 0; i++) {
      for(unsigned int j = 0; j <= 10; j += 1) {
        for(unsigned int k = 5; k <= 20; k += 1) {
          *(walker++) = new TLB(name, (1 << i), (1 << j), k, LRU);
          //*(walker++) = new TLB(name, (1 << i), (1 << j), k, MRU);
          //*(walker++) = new TLB(name, (1 << i), (1 << j), k, LFU);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// CoreInOrderLegacy Destructor
////////////////////////////////////////////////////////////////////////////////
CoreInOrderLegacy::~CoreInOrderLegacy() {
  if (regfiles) { 
    for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
      delete regfiles[i];
    }
    delete[] regfiles; regfiles = NULL;
  }
  if (sprfs)  {
    for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
      delete sprfs[i];
    }
    delete[] sprfs; sprfs = NULL;
  }
  if (accfiles)  {
    for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
      delete accfiles[i];
    }
    delete[] accfiles; accfiles = NULL;
  }
  if(scoreboards) {
    for(int i=0;i<rigel::THREADS_PER_CORE;i++) {
      delete scoreboards[i];
    }
    delete[] scoreboards; scoreboards = NULL;
  }
  if (sp_sbs) { 
    for(int i=0;i<rigel::THREADS_PER_CORE;i++) {
      delete sp_sbs[i];
    }
    delete[] sp_sbs; sp_sbs = NULL;
  }
  if (ac_sbs) { 
    for(int i=0;i<rigel::THREADS_PER_CORE;i++) {
      delete ac_sbs[i];
    }
    delete[] ac_sbs; ac_sbs = NULL;
  }
	//Branch prediction code is deprecated.
  //if (bp) { delete bp; bp = NULL; }
	
  // Clean up any remaining instructions (for all threads)
  for(int i=0;i<rigel::THREADS_PER_CORE;i++) {
    this->FlushSpecInstrList(i);
  }
  // cleanup dynamically allocated shared pipe state structs
  if(PipeThreadState) {
    for (int i = 0; i < rigel::THREADS_PER_CORE; i++) {
      if (PipeThreadState[i].btb) { delete PipeThreadState[i].btb; PipeThreadState[i].btb = NULL; }
    }
    delete[] PipeThreadState; PipeThreadState = NULL;
  }

  if(rigel::DUMP_REGISTER_TRACE){
    for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
      reg_trace_file[i].close();
    }
    delete[] reg_trace_file;
  }
}

////////////////////////////////////////////////////////////////////////////////
// CoreInOrderLegacy::PerCycle()
////////////////////////////////////////////////////////////////////////////////
// one "clock cycle" update, update each stage's latches
// step through each stage of the pipeline and update the latches
// returns 1 if halted, 0 if not halted  what
// about some halted some not?)
////////////////////////////////////////////////////////////////////////////////
int CoreInOrderLegacy::PerCycle() {

   // don't allow switch if selected core has not yet initiated a fetch to prevent starvation
   if(rigel::THREADS_PER_CORE > 1 && !get_fetch_thread_state()->waiting_to_fetch)
     thread_switch();

   //if(forceSwap && rigel::THREADS_PER_CORE > 1) {
   //  thread_switch();
     //forceSwap = false;
   //}

  // FETCH
  stages[FETCH_STAGE]->Update(); 

  // DECODE 
  stages[DECODE_STAGE]->Update(); 

  // EXECUTE
  stages[EXECUTE_STAGE]->Update();

  // MEM_ACCESS
  stages[MEM_ACCESS_STAGE]->Update();

  // FP_COMPLETE
  stages[FP_COMPLETE_STAGE]->Update();

  // CCACHE_ACCESS
  stages[CACHE_ACCESS_STAGE]->Update();

  // WRITEBACK
  stages[WRITE_BACK_STAGE]->Update();

  return 0; // return something!!!
}



////////////////////////////////////////////////////////////////////////////////
// UpdateLatches()
////////////////////////////////////////////////////////////////////////////////
// updates the latches for the core (clock edge)
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::UpdateLatches() {

  /////////* Clock Latches *///////////////////////////////////////////////////
  //
  // FIXME: this is AWFUL! latching relies on latches and nlatches depending
  // on a variety of circumstances instead of always using the current
  // latched values. In addition, if we simply executed the stages in
  // reverse order as we attempt to latch, we could avoid executing things
  // we don't need due to stalls
  //////////////////////////////////////////////////////////////////////////
  
  // latch back to front

  //////////////////////////////////////////////////////////////////////////
  // CLUSTER CACHE ACCESS -> WRITEBACK
  //////////////////////////////////////////////////////////////////////////
  for (int  k = rigel::ISSUE_WIDTH-1; k >= 0; k--) {
    nlatches[CC2WB][k]->stats.cycles.stuck_behind_other_instr--;
    latches[CC2WB][k]  = nlatches[CC2WB][k];
    nlatches[CC2WB][k] = rigel::NullInstr;
  }

  //////////////////////////////////////////////////////////////////////////
  // FP COMPLETE -> CLUSTER CACHE ACCESS
  //////////////////////////////////////////////////////////////////////////
  for (int  k = rigel::ISSUE_WIDTH-1; k >= 0; k--) {
    // TODO: We should only subtract k from cc_b accumulation when there is
    // a useful instruction coming from EX.  We should do this same thing in
    // execute as well.
    if (ccache_stage->is_stalled(k)) {
      force_thread_swap(); //FIXME This is currently ignored.
      goto done_latching;
    } else {
      nlatches[FP2CC][k]->stats.cycles.stuck_behind_other_instr--;
      latches[FP2CC][k]  = nlatches[FP2CC][k];
      nlatches[FP2CC][k] = rigel::NullInstr;
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // MEMORY -> FP_COMPLETE
  //////////////////////////////////////////////////////////////////////////
  for (int  k = rigel::ISSUE_WIDTH-1; k >= 0; k--) {
    if (fpaccess_stage->is_stalled(k)) {

      //FIXME Put in per-instruction profiling code for FP stalls if need be.  
      //For now, it never stalls.
      //std::cout << "FP Stalling" <<"\n";

      goto done_latching;
    } else {
      nlatches[MC2FP][k]->stats.cycles.stuck_behind_other_instr--;
      latches[MC2FP][k]  = nlatches[MC2FP][k];
      nlatches[MC2FP][k] = rigel::NullInstr;
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // EXECUTE -> MEMORY
  //////////////////////////////////////////////////////////////////////////
  for (int  k = rigel::ISSUE_WIDTH-1; k >= 0; k--) {
    if (mem_stage->is_stalled(k)) {

      #ifdef DEBUG_MT
      int cnum = get_core_num_global();
      int tnum = get_fetch_thread_id_core();
      cerr << "MEM Stalling - (core " << cnum << ":thr " << tnum << ") (pipe " << k;
      cerr << ") Cycle " << rigel::CURR_CYCLE << "\n";
      #endif

      goto done_latching;
    } else {
     /* if (nlatches[EX2MC][k] != rigel::NullInstr){
        fprintf(stderr, "Core: %d \n",core_num );
        nlatches[EX2MC][k]->dump();
      }*/
      // do the latching
      nlatches[EX2MC][k]->stats.cycles.stuck_behind_other_instr--;
      latches[EX2MC][k]  = nlatches[EX2MC][k];
      nlatches[EX2MC][k] = rigel::NullInstr;

    }
  }

  //////////////////////////////////////////////////////////////////////////
  // DECODE -> EXECUTE
  //////////////////////////////////////////////////////////////////////////
  for (int  k = rigel::ISSUE_WIDTH-1; k >= 0; k--) {
    if (execute_stage->is_stalled(k)) {
      #ifdef DEBUG_MT
      int cnum = get_core_num_global();
      int tnum = get_fetch_thread_id_core();
      cerr << "EX Stalling - (core " << cnum << ":thr " << tnum << ") (pipe " << k;
      cerr << ") Cycle " << rigel::CURR_CYCLE << "\n";
      #endif
      goto done_latching;
    } else {
      nlatches[DC2EX][k]->stats.cycles.stuck_behind_other_instr--;
      latches[DC2EX][k]  = nlatches[DC2EX][k];
      nlatches[DC2EX][k] = rigel::NullInstr;
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // FETCH -> DECODE
  //////////////////////////////////////////////////////////////////////////
  for (int  k = rigel::ISSUE_WIDTH-1; k >= 0; k--) {
    if (!fetch_stage->is_stalled(k)) {
      nlatches[IF2DC][k]->stats.cycles.stuck_behind_other_instr--;
      latches[IF2DC][k]  = nlatches[IF2DC][k];
      nlatches[IF2DC][k] = rigel::NullInstr;
    }
    //else
    //std::cout << "IF Stalling" << "\n";
  }
  // If we find a stall, all latching stops
      
 done_latching:
 ;
 //return 0;

}

////////////////////////////////////////////////////////////////////////////////
// CoreInOrderLegacy::AddSpecInstr()
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::AddSpecInstr(InstrSlot instr) {
  int tid = instr->get_core_thread_id(); 
  int LH = PipeThreadState[tid].SpecInstr.ListHead;
  assert( PipeThreadState[tid].SpecInstr.List[ LH  ] == rigel::NullInstr 
    && "Trying to overwrite a valid instruction!");

  instr->spec_list_idx = LH;
  PipeThreadState[tid].SpecInstr.List[LH] = instr;
  PipeThreadState[tid].SpecInstr.ListHead = (LH + 1) % PipeThreadState[tid].SpecInstr.ListLength;
}

////////////////////////////////////////////////////////////////////////////////
// CoreInOrderLegacy::RemoveSpecInstr()
////////////////////////////////////////////////////////////////////////////////
// At this point we know that the instruction is non-speculative so it must
// commit and be deleted at writeback.
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::RemoveSpecInstr(InstrSlot instr) {
  int tid = instr->get_core_thread_id(); 
  int idx = instr->spec_list_idx;
  assert(PipeThreadState[tid].SpecInstr.List[idx] == instr && "SpecList index is corrupt or wrong!");
  PipeThreadState[tid].SpecInstr.List[idx] = rigel::NullInstr;
}

////////////////////////////////////////////////////////////////////////////////
// CoreInOrderLegacy::FlushSpecInstrList
////////////////////////////////////////////////////////////////////////////////
// clear front-end of pipe
//
// MT: we cannot indiscriminately flush the front end; instructions may belong
// to different threads. We must flush only those instructions belonging to the
// thread inducing the front-end flush.
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::FlushSpecInstrList(int tid) {

  for(int i = 0; i < PipeThreadState[tid].SpecInstr.ListLength; i++ ) {
    InstrSlot instr = PipeThreadState[tid].SpecInstr.List[i];

    // If it is an empty slot skip it
    if (instr == rigel::NullInstr) continue;

    // invalid (NullInstr) tids are already accounted for
    assert( PipeThreadState[tid].SpecInstr.List[i]->get_core_thread_id() == tid &&
            "ERROR: flushing instruction owned by another thread!" );

    // Free the registers locked by the instruction
    FlushInstr(instr);
    PipeThreadState[tid].SpecInstr.List[i] = rigel::NullInstr;
  }

}

///////////////////////////////////////////////////////////////////////////////
// CoreInOrderLegacy::FlushInstr
//
// This function cleans up the scoreboard and deletes the instruction
///////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::FlushInstr(InstrSlot instr) {
  uint32_t reg = ~0;
  int tid = instr->get_core_thread_id();
  if(instr != rigel::NullInstr) 
  {
    // See  note above about this
    if(instr->has_lock(reg)) {
      scoreboards[tid]->clear_reg(reg);
    }
    // clean up the scoreboard
    if(instr->did_sb_get()) {
      instr_t instr_type = instr->get_type();
      // add any instruction that writes SPRF here
      if (instr_type == I_JAL || instr_type == I_JALR || instr_type == I_LJL) {
        scoreboards[tid]->put_reg(rigel::regs::LINK_REG);
      } else if (instr_type == I_MTSR) {
        sp_sbs[tid]->put_reg(instr->get_dreg());
      } else if (instr->get_is_vec_op())  {
        // Vector operations have multiple SB gets
        scoreboards[tid]->put_reg(instr->get_dreg());
        scoreboards[tid]->put_reg(instr->get_dreg1());
        scoreboards[tid]->put_reg(instr->get_dreg2());
        scoreboards[tid]->put_reg(instr->get_dreg3());
      } else {
        scoreboards[tid]->put_reg(instr->get_dreg());
      }
    }
    #ifdef DEBUG_MT
    cerr << "deleting instruction:\n";
    instr->dump();
    cerr << "\n";
    #endif
    delete instr;
  }
}

////////////////////////////////////////////////////////////////////////////////
// print_stage()
////////////////////////////////////////////////////////////////////////////////
// helper function that prints interactive mode info for the provided pipeline
// stage using a common format
////////////////////////////////////////////////////////////////////////////////
void
CoreInOrderLegacy:: print_stage(const char *stage_name, latch_name_t stage) {

	FILE *disassembly_out_stream = stdout;
  // iterate across each PIPE in the core
  for (int i = rigel::ISSUE_WIDTH-1; i >= 0; i--) {

    int pipe = i;
    //char stageprint[128]; // way more than enough...
    
    //////////////////////////////
    // nlatches (WHAT ARE THESE?)
    //////////////////////////////
    //std::cout << "[" << pipe << "]" << stage_name << "_n || " ;
    {

    int tid = nlatches[stage][i]->get_core_thread_id();
    if( tid<0 )
      printf("[%d] %s_n ||    || ", pipe, stage_name);
    else
      printf("[%d] %s_n || %2d || ", pipe, stage_name, tid);

    //for( int t = 0; t<rigel::THREADS_PER_CORE; t++) {

      // nlatch is NOT NULL
      if (nlatches[stage][i] != rigel::NullInstr) {
        printf(" %9"PRIu64 " %08"PRIx32" ",
          nlatches[stage][i]->GetInstrNumber(),
          nlatches[stage][i]->get_currPC()
        );
#ifndef _WIN32
        size_t num_chars = nlatches[stage][i]->dis_print(disassembly_out_stream);
        if (nlatches[stage][i]->is_badPath()) {
          const char tmp[] = "[INVALIDATING]";
          printf("%s",tmp);
          num_chars += strlen(tmp);
        }
        const char tmp2[] = " [STALL]";
        printf("%s",tmp2);
        num_chars += strlen(tmp2);

        int MAX = 36;

        //assert(num_chars <= MAX && "num_chars exceeds MAX");
        for(int m = MAX-num_chars; m>0; m-- ) {
          printf("_");
        }
#endif // _WIN32

      }
      // nlatch is NULL (NullInstr)
      else {
        printf("           [BUBBLE]                                     ");
      }
      //std::cout << "\n";
      std::cout  << " || ";

    //}
    }
    printf("\n"); // newline only after all threads printed now
    //////////////////////////////

    //////////////////////////////
    // latches (not nlatches, WHAT ARE THESE?)
    //////////////////////////////
    {
    int tid = latches[stage][i]->get_core_thread_id();
    if(tid<0)
      printf("[%d] %s   ||    || ", pipe, stage_name);
    else
      printf("[%d] %s   || %2d || ", pipe, stage_name, tid);

    //for( int t = 0; t<rigel::THREADS_PER_CORE; t++) {

      // NON-NULL instructions
      if (latches[stage][i] != rigel::NullInstr) {
        printf(" %9"PRIu64" %08"PRIx32" ",
          latches[stage][i]->GetInstrNumber(),
          latches[stage][i]->get_currPC()
        );
#ifndef _WIN32
        size_t num_chars = latches[stage][i]->dis_print(disassembly_out_stream);
        int MAX = 36;
        for(int m = MAX-num_chars; m>0; m-- ) {
          printf("_");
        }
#endif // _WIN32
        if (latches[stage][i]->is_badPath()) {
          printf(" [INVALIDATING]");
        }

      }
      // latch is NULL (NullInstr)
      else {
        printf( "           [BUBBLE]                                     ");
      }

      //std::cout << "\n";
      std::cout  << " || ";
    //}
    }
    printf("\n"); // newline only after all threads printed now
    //////////////////////////////

   } // end loop over pipes (0..ISSUE_WIDTH)

   //fclose(disas);

}


////////////////////////////////////////////////////////////////////////////////
// PrintCoreDisplay()
////////////////////////////////////////////////////////////////////////////////
// prints the interactive mode display info for all stages in a core
////////////////////////////////////////////////////////////////////////////////
//
// DANGER: FIXME: BADNESS! This could be HOSED for MT.
//
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::PrintCoreDisplay(UserInterfaceLegacy *ui) {
  const char hseparator0[] =
  "===========================================================================================================";
  const char hseparator1[] =
  "-----------------------------------------------------------------------------------------------------------";

  ///////// DISPLAY ///////////////////////////////////////////////////
  if (ui->is_interactive()) {
    printf("%s\n",hseparator0); 
    printf("[CYCLE: %8"PRIu64"] [CORE: %4d] \n",rigel::CURR_CYCLE,core_num);
    printf("IF %01d %01d | DC %01d %01d | EX %01d %01d | MC %01d %01d | FP %01d %01d | CC %01d %01d\n",
           fetch_stage->is_stalled(0), fetch_stage->is_stalled(1),
           decode_stage->is_stalled(0), decode_stage->is_stalled(1),
           execute_stage->is_stalled(0), execute_stage->is_stalled(1),
           mem_stage->is_stalled(0), mem_stage->is_stalled(1),
           fpaccess_stage->is_stalled(0), fpaccess_stage->is_stalled(1),
           ccache_stage->is_stalled(0), ccache_stage->is_stalled(1));
    printf("ALU %d, ALU_VECTOR %d, FPU %d, FPU_VECTOR %d, FPU_SPECIAL %d, ALU_SHIFT %d, SPFU %d, COMPARE %d, MEM %d, BRANCH %d, BOTH %d\n",
           PipeCoreState.funit_available_count[FU_ALU],
           PipeCoreState.funit_available_count[FU_ALU_VECTOR],
           PipeCoreState.funit_available_count[FU_FPU],
           PipeCoreState.funit_available_count[FU_FPU_VECTOR],
           PipeCoreState.funit_available_count[FU_FPU_SPECIAL],
           PipeCoreState.funit_available_count[FU_ALU_SHIFT],
           PipeCoreState.funit_available_count[FU_SPFU],
           PipeCoreState.funit_available_count[FU_COMPARE],
           PipeCoreState.funit_available_count[FU_MEM],
           PipeCoreState.funit_available_count[FU_BRANCH],
           PipeCoreState.funit_available_count[FU_BOTH]);

    printf("%s\n",hseparator0); 
    // FETCH
    for (int k = rigel::ISSUE_WIDTH-1; k >= 0; k--) {
      std::cout << "IF:      || " << get_fetch_thread_id_core() << "  || 0x" << HEX_COUT << get_fetch_thread_state()->IF2IF.NextPC+4*k << "\n";
    }
    printf("%s\n",hseparator1);
    // DECODE
    print_stage("DC",IF2DC);
    printf("%s\n",hseparator1);
    // EXEC
    print_stage("EX",DC2EX);
    printf("%s\n",hseparator1);
    // MEM
    print_stage("MC",EX2MC);
    printf("%s\n",hseparator1);
    // FPU
    print_stage("FP",MC2FP);
    printf("%s\n",hseparator1);
    // CC
    print_stage("CC",FP2CC);
    printf("%s\n",hseparator1);
    // WB
    for (int k = rigel::ISSUE_WIDTH-1; k >= 0; k--) {
      int pipe = k;
      int tid = latches[CC2WB][k]->get_core_thread_id();
      if(tid<0) {
        printf("[%d] %s   ||    || ", pipe, "WB" );
      } else {
        printf("[%d] %s   || %2d || ", pipe, "WB", tid);
      }

      if (latches[CC2WB][k] == rigel::NullInstr) {
        printf("           [BUBBLE]\n");
      } else {
        printf(" %9"PRIu64" %08"PRIx32" ",
          latches[CC2WB][pipe]->GetInstrNumber(),
          latches[CC2WB][pipe]->get_currPC()
        );
        latches[CC2WB][k]->dis_print();  

        RegisterBase rr = latches[CC2WB][k]->get_result_reg();
        RegisterBase rd = latches[CC2WB][k]->get_dreg();
        RegisterBase ad = latches[CC2WB][k]->get_dacc();
        if( rd.addr < simconst::NULL_REG ) { // something gets into the address that is NOT a register number, ignore
          printf("  [ R%02u <- 0x%x ]", rd.addr, rr.data.i32 ); // the result is in one placed the (reliable) dest is in another...
          //printf("  [ R%02d <- 0x%u ]", rr.addr, rr.data.i32 ); // the result is in one placed the (reliable) dest is in another...
        }
        if( ad.addr < simconst::NULL_REG ) { // something gets into the address that is NOT a register number, ignore
          printf("  [ A%02u <- 0x%x ]", ad.addr, rr.data.i32 ); // the result is in one placed the (reliable) dest is in another...
          //printf("  [ R%02d <- 0x%u ]", rr.addr, rr.data.i32 ); // the result is in one placed the (reliable) dest is in another...
        }

        if (latches[CC2WB][k]->is_badPath()) { std::cout << " [INVALIDATING]"; } 
        std::cout << "\n";
      }
    }
    printf("%s\n",hseparator1);
  }
}


////////////////////////////////////////////////////////////////////////////////
// flush()
////////////////////////////////////////////////////////////////////////////////
// Flushing the pipeline on a mispredict is still a bit hairy.  For all of the
// instructions in the pipeline before the mispredict, locks must be cleared for
// any that have reached regread and locked a register (LDW).  Setting the
// instruciton as a bad path should suffice to clear out the pipeline of any
// mispredict since badpath is checked at each stage above execute
////////////////////////////////////////////////////////////////////////////////
void
CoreInOrderLegacy::flush(int tid) {
  using namespace rigel;

  for (int i = 0; i < ISSUE_WIDTH; i++) {

    // FIXME: (for MT): don't flush other thread's instructions!
    // flush this thread's instructions from up to (and including) dc2exec latches
    // FIXME: We cannot trust get_core_thread_id() here since the instruction may
    // have been deleted already.  We may want to fix FlushSpecInstrList and
    // FlushInstr to deal with these cases better.

    // for one thread, safe to flush everything in the front end
    // this seperate code should not be needed but just in case the other code
    // path gets busted, single-thread should still be ok 
    if( rigel::THREADS_PER_CORE == 1 ) {
      latches[IF2DC][i]  = rigel::NullInstr;
      latches[DC2EX][i]  = rigel::NullInstr;
      nlatches[IF2DC][i] = rigel::NullInstr;
      nlatches[DC2EX][i] = rigel::NullInstr;
    }
    // for MT, don't flush instructions belonging to another thread
    else if( rigel::THREADS_PER_CORE > 1 ) {
      for( int s = IF2DC; s <= DC2EX; s++ ) { // ordered latch-name enum
        if( latches[s][i]->get_core_thread_id() == tid ) {
          latches[s][i]  = rigel::NullInstr;
        }
        if( nlatches[s][i]->get_core_thread_id() == tid ) {
          nlatches[s][i] = rigel::NullInstr;
        }
      }
    } else { // unreachable
      assert(0);
    }
  
    // NOTE: these seem to not impact correctness or performance in any way
    // (stalls seem to be cleared for each cycle and recomputed anyway)
    //execute_stage->unstall(i);
    //decode_stage->unstall(i);
    //fetch_stage->unstall(i);
  }

  // this must happen AFTER setting the latches to NullInstr because it will
  // actually delete the instructions themselves which are needed for tid checks
  // in the code above
  FlushSpecInstrList(tid);
  
  // Optionally, let the whole scoreboard flush before allowing other
  // instructions to issue.  Possibly useful for debugging.
  // set in sim.h, this is the DEFAULT
  if (rigel::core::FORCE_SB_FLUSH) {
    assert( (tid>=0 && tid<rigel::THREADS_PER_CORE)  && "invalid thread ID!");
    get_scoreboard(tid)->wait_for_clear();
    sp_sbs[tid]->wait_for_clear();
    ac_sbs[tid]->wait_for_clear();
  }

}

////////////////////////////////////////////////////////////////////////////////
// thread_flush()
////////////////////////////////////////////////////////////////////////////////
// originally based on flush() but modified to handle conditions for flushing
// the pipeline for a thread swap in MultiThreading
////////////////////////////////////////////////////////////////////////////////
void
CoreInOrderLegacy::thread_flush(int tid) {
  using namespace rigel;

  FlushSpecInstrList(tid);
  for (int i = 0; i < ISSUE_WIDTH; i++) {

    // FIXME: (for MT): don't flush other thread's instructions!
    latches[IF2DC][i]  = rigel::NullInstr;
    latches[DC2EX][i]  = rigel::NullInstr;
    nlatches[IF2DC][i] = rigel::NullInstr;
    nlatches[DC2EX][i] = rigel::NullInstr;
    // for MT, kill only the EX2MC NLATCH since the NLATCH state is not actually
    // finished with the Execute stage yet
    FlushInstr(nlatches[EX2MC][i]);
    nlatches[EX2MC][i] = rigel::NullInstr;

   // unstall front end
    execute_stage->unstall(i);
    decode_stage->unstall(i);
    fetch_stage->unstall(i);

    // unstall back end
    mem_stage->unstall(i);
    fpaccess_stage->unstall(i);
    ccache_stage->unstall(i);
    writeback_stage->unstall(i);
    
  }
  
  // Optionally, let the whole scoreboard flush before allowing other
  // instructions to issue.  Possibly useful for debugging.
  // set in sim.h, this is the DEFAULT
  if (rigel::core::FORCE_SB_FLUSH) {
    assert( (tid>=0 && tid<rigel::THREADS_PER_CORE)  && "invalid thread ID!");
    get_scoreboard(tid)->wait_for_clear();
    sp_sbs[tid]->wait_for_clear();
    ac_sbs[tid]->wait_for_clear();
  }
}

// DELETE ME
// FIXME: this is not needed anymore, really, for per-thread per-stage
// latches...
void CoreInOrderLegacy::thread_restore(int tid) {
}


////////////////////////////////////////////////////////////////////////////////
// thread_switch()
////////////////////////////////////////////////////////////////////////////////
// initiate a thread switch
// select a suitable replacement thread to run
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::thread_switch( ) {

  #ifdef DEBUG_MT
  fprintf(stderr,"thread_switch() begin \n"); 
  #endif

  // (0) set the next PC to something reasonable (select next thread)
  // (1) NO NEED to flush front-end of pipeline (BAD)
  // (2) NO NEED to checkpoint downstream state
  // (3) NO NEED to restore downstream state for new thread
  // (4) setup next thread to run

  // Update the restore Pc
  // Steve's hack which needs to be fixed later...
  // FIXME: what does this "hack" do? Should no longer be needed if nextPC is
  // saved with no frontend flushing...
  // FIXME: delete this from mem_stage: mem_stage->UpdateRestorePC();

  // simple, next thread
  int next_thread = schedule_next_thread();
  // TODO: if next_thread == current_thread ... why? (no other schedulable...what to do)
  //int next_thread = 0;

  #ifdef DEBUG_MT
	//FIXME isn't there a helper function to do this conversion?
	int global_next_thread = global_next_thread = core_num*rigel::THREADS_PER_CORE+next_thread;
  fprintf(stderr,"Cluster: %d Core: %d current thread: %d  Switch to thread %d (%dg) @ cycle %" PRIu64 " \n",
    ClusterNum(get_core_num_global()), get_core_num_local(),
    fetch_thread_id, next_thread, global_next_thread,
    rigel::CURR_CYCLE
  );
  #endif

  // update state needed to switch threads
  fetch_thread_id = next_thread;

}

////////////////////////////////////////////////////////////////////////////////
// set_mt_restore_pc()
////////////////////////////////////////////////////////////////////////////////
// for MULTITHREADING
// save a recovery PC for multithreading mode
// use committed latch values that have already been latched to avoid saving
// next pcs for instructions that have not passed execute
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::set_mt_restore_pc(uint32_t pc, int tid) {
  get_thread_state( tid )->MT.restore_pc = pc;
  // BRANCHES are resolved by the end of execute and the correct PC is in nextPC
}

////////////////////////////////////////////////////////////////////////////////
// schedule_next_thread()
////////////////////////////////////////////////////////////////////////////////
// selects the next thread based on some policy
// returns the thread number of selected thread (same as current if no change)
//
//  TODO: policy as PARAMETER to schedule!
//
////////////////////////////////////////////////////////////////////////////////
int CoreInOrderLegacy::schedule_next_thread() {
  int next_thread_id = fetch_thread_id;
  for(int i = 0; i < rigel::THREADS_PER_CORE; i++)
  {
    next_thread_id = (next_thread_id + 1) % rigel::THREADS_PER_CORE;
    // if next thread is ready to fetch, break out and return it, otherwise try next one
    if(!is_thread_halted(next_thread_id) && PipeThreadState[next_thread_id].req_to_fetch)
    {
      break;
      //fprintf(stderr,"Thread schedule skipped core %d thread %d\n",core_num,next_thread_id);
    }
  }
  return next_thread_id;
}

////////////////////////////////////////////////////////////////////////////////
// dump_locality()
////////////////////////////////////////////////////////////////////////////////
// dump per-thread and aggregate locality info
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::dump_locality() const
{
  for(int i = 0; i < rigel::THREADS_PER_CORE; i++)
    perThreadLocalityTracker[i]->dump();
  aggregateLocalityTracker->dump();
}

////////////////////////////////////////////////////////////////////////////////
// dump_tlb()
////////////////////////////////////////////////////////////////////////////////
// dump per-thread and aggregate TLB info
////////////////////////////////////////////////////////////////////////////////
void CoreInOrderLegacy::dump_tlb() const
{
  if(rigel::THREADS_PER_CORE > 1) {
    for(int i = 0; i < rigel::THREADS_PER_CORE; i++) {
      for(size_t j = 0; j < num_tlbs; j++) {
        perThreadITLB[i][j]->dump(stderr);
      }
      for(size_t j = 0; j < num_tlbs; j++) {
        perThreadDTLB[i][j]->dump(stderr);
      }
      for(size_t j = 0; j < num_tlbs; j++) {
        perThreadUTLB[i][j]->dump(stderr);
      }
    }
  }

  for(size_t j = 0; j < num_tlbs; j++) {
    aggregateITLB[j]->dump(stderr);
  }
  for(size_t j = 0; j < num_tlbs; j++) {
    aggregateDTLB[j]->dump(stderr);
  }
  for(size_t j = 0; j < num_tlbs; j++) {
    aggregateUTLB[j]->dump(stderr);
  } 
}


/// Dump
void 
CoreInOrderLegacy::Dump() {
  assert(0 && "unimplemented!");
}

/// Heartbeat
void 
CoreInOrderLegacy::Heartbeat() {
  assert(0 && "unimplemented!");
}

/// EndSim
void
CoreInOrderLegacy::EndSim() {
  assert(0 && "unimplemented!");
}

void CoreInOrderLegacy::save_state() const {
  for(int i = 0; i < rigel::THREADS_PER_CORE; i++) {
    rigelsim::ThreadState *ts = core_state->add_threads();
    ts->set_tid(i);
    get_regfile(i)->save_to_protobuf(ts->mutable_rf());
    get_sprf(i)->save_to_protobuf(ts->mutable_sprf());
    get_accumulator_regfile(i)->save_to_protobuf(ts->mutable_accrf());
    ts->set_pc(GetCurPC(i));
    ts->set_halted(is_thread_halted(i));
  }
  core_state->set_activetid(get_fetch_thread_id_core());
  //All cores are at the same frequency for now
  core_state->set_frequency(1000000000.0*rigel::CLK_FREQ);
  //All cores run in lockstep for now
  core_state->set_cycle(rigel::CURR_CYCLE);
}

void CoreInOrderLegacy::restore_state() {
  
}
