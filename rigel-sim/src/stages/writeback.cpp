////////////////////////////////////////////////////////////////////////////////
// writeback.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  Writeback stage implementations
//
//  includes the percycle update function and the writeback function
//
////////////////////////////////////////////////////////////////////////////////


#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu64
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for fprintf, stderr, NULL
#include <fstream>                      // for ofstream, operator<<, etc
#include <iostream>                     // for cerr
#include <map>                          // for map
#include <string>                       // for string, operator<, etc
#include "../user.config"   // for ENABLE_VERIF
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy, ::WB, etc
#include "define.h"         // for InstrSlot, etc
#include "instr.h"          // for InstrLegacy, etc
#include "instrstats.h"     // for InstrStats, InstrCacheStats
#include "profile/profile.h"        // for ProfileStat, Profile, etc
#include "profile/profile_names.h"  // for ::STATNAME_L2D_CACHE, etc
#include "core/regfile_legacy.h"        // for RegisterBase, RegisterFile, etc
#include "core/scoreboard.h"     // for ScoreBoard
#include "sim.h"            // for stats, etc
#include "util/util.h"           // for ExitSim
#include "stage_base.h"  // for WriteBackStage

////////////////////////////////////////////////////////////////////////////////
// Method Name: WriteBackStage::Update()
////////////////////////////////////////////////////////////////////////////////
// Description: called each cycle to compute writeback stage operation
// Inputs: none
// Outputs: none
////////////////////////////////////////////////////////////////////////////////
void 
WriteBackStage::Update()
{
  using namespace rigel;

  for (int j = 0; j < rigel::ISSUE_WIDTH; j++) {
    // Only place where instructions die
    core->latches[WB][j] = writeback(core->latches[CC2WB][j], j);

    if (core->latches[WB][j] != rigel::NullInstr ) { 
      assert(core->latches[WB][j] != NULL && "Trying to delete a NULL InstrSlot");
      // Sanity check that we are committing in order
      int temp_thread_id = core->latches[WB][j]->get_global_thread_id();

      //assert( (core->latches[WB][j]->GetInstrNumber() >
      //InstrLegacy::LAST_INSTR_NUMBER[temp_thread_id] )
      //                && "Out of order commit!");
      //If it isn't a bubble, update the LAST_INSTR_NUMBER
      //for use in ensuring in-order commit and for heartbeat_print_pcs

      //If NONBLOCKING_MEM is enabled, don't check for monotonically-increasing
      //instruction commit numbers, since loads/stores that stall and get shunted
      //into the secondary structure will commit out of order.
      bool check = rigel::NONBLOCKING_MEM ?
                   (core->latches[WB][j]->get_type() != I_NULL) :
                   (core->latches[WB][j]->get_type() != I_NULL &&
                    core->latches[WB][j]->GetInstrNumber() > InstrLegacy::LAST_INSTR_NUMBER[temp_thread_id]);
      if(check)
        InstrLegacy::LAST_INSTR_NUMBER[temp_thread_id] = core->latches[WB][j]->GetInstrNumber();
      // Set last PC for the benefit of the heartbeat PC printouts.  If we want to use last_pc to do checkpointing
      // again, we'll want to re-evaluate whether this is the best spot to set last_pc.
      core->set_last_pc( core->latches[WB][j]->get_currPC(), core->latches[WB][j]->get_core_thread_id() );
      // Keep track of last commit cycle to handle proper cycle counting.
      core->set_last_commit_cycle(rigel::CURR_CYCLE);
      // Handle per-instruction  profiling.
      InstrSlot instr = core->latches[WB][j];
      UpdateWBStats(instr, temp_thread_id);


      #ifdef DUMP_WB_TRACE
      // Move to method in InstrLegacy
      {
        fprintf(stdout, "0x%08x (0x%08x) ",
          (uint32_t)(rigel::CURR_CYCLE),
          (uint32_t)(cores->core->latches[WB][j]->GetInstrNumber()));
        fprintf(stdout, "ea; 0x%08x sreg0: 0x%08x sreg1: 0x%08x result: 0x%08x imm: 0x%04x ",
          core->latches[WB][j]->get_result_ea(),
          core->latches[WB][j]->get_sreg0().data.i32,
          core->latches[WB][j]->get_sreg1().data.i32,
          core->latches[WB][j]->get_result_reg().data.i32,
          core->latches[WB][j]->get_imm());
        fprintf(stdout, "PC: 0x%08x ", 
          (uint32_t)(cores[i]->core->latches[WB][j]->get_currPC()));
        core->latches[WB][j]->dis_print();
        fprintf(stdout, "\n");
      }
      #endif

      core->get_cluster()->getProfiler()->timing_stats.wb_active++;

      /*
      for(list<ICMsg>::iterator it =
        TileInterconnect->RequestBuffer.begin(); it !=
        TileInterconnect->RequestBuffer.end(); it++)
      {
        if(it->spawner == core->latches[WB][i][j])
        {
          cout << "ERROR: 0x" << std::hex << core->latches[WB][i][j]->get_raw_instr()
          << " has an ICMsg in TileInterconnect->RequestBuffer which refers
          to it!" << endl;
          it->dump(__FILE__, __LINE__);
          assert(0 && "icmsg spawner error");
        }
      }
      */      
  
      delete core->latches[WB][j];
      core->latches[WB][j] = NULL;
  
    } else {
      // last_WB == NullInstr
      core->get_cluster()->getProfiler()->timing_stats.wb_bubble++;
    }
  }
}



void WriteBackStage::UpdateWBStats(InstrSlot instr, int temp_thread_id )
{
  using namespace rigel;
  {

    // Count of retired instructions to tabulate IPC.
    profiler::stats[STATNAME_RETIRED_INSTRUCTIONS].inc();
    // Global instruction count used internally.
    ProfileStat::inc_retired_instrs(temp_thread_id);

    // Profile atomics.
    if (instr->stats.is_global_atomic()) {
      profiler::stats[STATNAME_GLOBAL_ATOMICS].inc();
    }
    if (instr->stats.is_local_atomic()) {
      profiler::stats[STATNAME_LOCAL_ATOMICS].inc();
    }
    if (instr->stats.is_atomic()) {
       profiler::stats[STATNAME_ATOMICS].inc();
    }
    if (instr->stats.is_global()) {
       profiler::stats[STATNAME_GLOBAL_MEMORY].inc();
    }
    if (instr->stats.is_local_memaccess()) {
       profiler::stats[STATNAME_LOCAL_MEMORY].inc();
    }


    //Per-instruction stall profiling
    if (rigel::profiler::CMDLINE_DUMP_PER_PC_STATS)
      Profile::per_pc_stat_add_stalls(instr->get_currPC(), instr->stats.cycles);
    else
    {
      Profile::accumulate_stall_cycles(instr->stats.cycles);
    }

    #if 0
        uint64_t cycles_in_flight = (rigel::CURR_CYCLE-instr->get_spawn_cycle());
        if(instr->stats.cycles.total_cycles()-1 != cycles_in_flight)
        {
          printf("------------------------------\n");
          instr->stats.cycles.dump();
          printf("IN FLIGHT %"PRIu64" CYCLES\n", cycles_in_flight);
          printf("TOTAL-1 = %"PRIu64" CYCLES\n", (instr->stats.cycles.total_cycles()-1));
        }
        else
          printf("MATCH\n");
    #endif

    update_memaccess_stats(instr);

    // Update the per-PC profiler statistics.
    uint32_t instr_latency = rigel::CURR_CYCLE - instr->get_fetch_cycle();
    // L1D/L2D hits tallyed in WB stage
    Profile::per_pc_stat_add_latency(instr->get_currPC(), instr_latency);
  } 
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: WriteBackStage::update_memaccess_stats()
////////////////////////////////////////////////////////////////////////////////
// Description: Update profile statistics for memory operations including L1/2
//              D/I latencies.   Called once per instruction at retirement.
// Inputs: Instruction being retired.
// Outputs: none
////////////////////////////////////////////////////////////////////////////////
void
WriteBackStage::update_memaccess_stats(InstrSlot instr)
{
  using namespace rigel;

  bool l1i_hit = instr->stats.cache.l1i_hit();
  bool l2i_hit = !l1i_hit ? instr->stats.cache.l2i_hit() : false ;
  Profile::per_pc_stat_add_icache(instr->get_currPC(), l1i_hit, l2i_hit);
  // Instead of using latency, which might include time blocking on a
  // previous memory operation, we are going to use the number of retries
  // at the memory system.
  //uint64_t access_latency = rigel::CURR_CYCLE - instr->get_fetch_cycle();
  uint64_t access_latency = instr->get_last_memaccess_cycle() - instr->get_first_memaccess_cycle();

  // Update L1I hits/misses.
  if (instr->stats.cache.l1i_hit()) {
    profiler::stats[STATNAME_L1I_CACHE].inc_hits();
    core->get_cluster()->getProfiler()->cache_stats.L1I_cache_hits++;
  } else {
    profiler::stats[STATNAME_L1I_CACHE].inc_misses();
    core->get_cluster()->getProfiler()->cache_stats.L1I_cache_misses++;
    uint32_t ea = instr->get_result_ea();
    profiler::stats[STATNAME_HISTOGRAM_L1I_CACHE_MISSES].inc_mem_histogram(ea);
  }

  // Update L2I only on L1I miss.
  if (!instr->stats.cache.l1i_hit()) {
    if (instr->stats.cache.l2i_hit()) {
      profiler::stats[STATNAME_L2I_CACHE].inc_hits();
      core->get_cluster()->getProfiler()->cache_stats.L2I_cache_hits++;
    } else {
       profiler::stats[STATNAME_L2I_CACHE].inc_misses();
       core->get_cluster()->getProfiler()->cache_stats.L2I_cache_misses++;
       uint32_t ea = instr->get_result_ea();
       profiler::stats[STATNAME_HISTOGRAM_L2I_CACHE_MISSES].inc_mem_histogram(ea);
    }
  }
  // Only local memory operations are checked for L1D/L2D accesses.
  if (instr->stats.is_local_memaccess()) {
    bool l1d_hit = instr->stats.cache.l1d_hit();
    bool l2d_hit = instr->stats.cache.l2d_hit();
    bool l3d_hit = instr->stats.cache.l3d_hit();
    // Update per-PC profiler.  
    Profile::per_pc_stat_add_dcache(instr->get_currPC(), l1d_hit, l2d_hit, l3d_hit);
    // L2D updates only on L1D misses.
    if (!instr->stats.cache.l1d_hit()) {
      if (instr->stats.cache.l2d_hit()) {
        if (instr->instr_is_prefetch()) {
          profiler::stats[STATNAME_L2D_CACHE].inc_prefetch_hits();
        } else {
          profiler::stats[STATNAME_L2D_CACHE].inc_hits();
          // FIXME: It looks like we are adding in latencies from sleeping
          // during init.  This is bad.
          if (access_latency < L2D_MISS_LAT_PROFILE_MAX) 
          {
            // Handle ATTEMPTS tracking for memory operations
            switch (instr->get_type_decode()) {
              case I_LDW: 
                profiler::stats[STATNAME_ATTEMPTS_LDW_HIT_L2D].counted_inc(access_latency); break;
              case I_LDL: 
                profiler::stats[STATNAME_ATTEMPTS_LDL_HIT_L2D].counted_inc(access_latency); break;
              case I_STW: 
                profiler::stats[STATNAME_ATTEMPTS_STW_HIT_L2D].counted_inc(access_latency); break;
              case I_STC: 
                profiler::stats[STATNAME_ATTEMPTS_STC_HIT_L2D].counted_inc(access_latency); break;
              default: break;
            } 
          } else {
            // We will assume this is a spurious value.  Too many of these
            // signifies a problem where too many is > 1-4 K 
            profiler::stats[STATNAME_L2D_PROFILE_IGNORE].inc();
          }
        }
      } else {
        if (instr->instr_is_prefetch()) {
          profiler::stats[STATNAME_L2D_CACHE].inc_prefetch_misses();
        } else {
          profiler::stats[STATNAME_L2D_CACHE].inc_misses();
          // FIXME: It looks like we are adding in latencies from sleeping
          // during init.  This is bad.
          if (access_latency < L2D_MISS_LAT_PROFILE_MAX) 
          {
            // Handle ATTEMPTS tracking for memory operations
            switch (instr->get_type_decode()) {
              case I_LDW: 
                profiler::stats[STATNAME_ATTEMPTS_LDW_MISS_L2D].counted_inc(access_latency); 
                break;
              case I_LDL: 
                profiler::stats[STATNAME_ATTEMPTS_LDL_MISS_L2D].counted_inc(access_latency); break;
              case I_STW: 
                profiler::stats[STATNAME_ATTEMPTS_STW_MISS_L2D].counted_inc(access_latency); break;
              case I_STC: 
                profiler::stats[STATNAME_ATTEMPTS_STC_MISS_L2D].counted_inc(access_latency); break;
              default: break;
            }
          } else {
            // We will assume this is a spurious value.  Too many of these
            // signifies a problem where too many is > 1-4 K
            profiler::stats[STATNAME_L2D_PROFILE_IGNORE].inc();
          }
          uint32_t ea = instr->get_result_ea();
          profiler::stats[STATNAME_HISTOGRAM_L2D_CACHE_MISSES].inc_mem_histogram(ea);
        }
      }
    }
   // L1D updates for hits/misses.  Reads and writes are segregated.
   if (instr->stats.cache.l1d_hit()) {
     profiler::stats[STATNAME_L1D_CACHE].inc_hits();
     // Handle ATTEMPTS tracking for memory operations
     switch (instr->get_type_decode()) {
       case I_LDW: 
         if (access_latency > 14000) fprintf(stderr, "EA: %08x\n", instr->get_result_ea());
         profiler::stats[STATNAME_ATTEMPTS_LDW_HIT_L1D].counted_inc(access_latency); break;
       case I_LDL: 
         profiler::stats[STATNAME_ATTEMPTS_LDL_HIT_L1D].counted_inc(access_latency); break;
       case I_STW: 
         profiler::stats[STATNAME_ATTEMPTS_STW_HIT_L1D].counted_inc(access_latency); break;
       case I_STC: 
         profiler::stats[STATNAME_ATTEMPTS_STC_HIT_L1D].counted_inc(access_latency); break;
       default: break;
     }
     if (instr->stats.is_local_store()) {
       core->get_cluster()->getProfiler()->cache_stats.L1D_cache_write_hits++;
     } else {
       core->get_cluster()->getProfiler()->cache_stats.L1D_cache_read_hits++;
     }
   } else {
     profiler::stats[STATNAME_L1D_CACHE].inc_misses();
     // Handle ATTEMPTS tracking for memory operations
     switch (instr->get_type_decode()) {
       case I_LDW: 
         profiler::stats[STATNAME_ATTEMPTS_LDW_MISS_L1D].counted_inc(access_latency); break;
       case I_LDL: 
         profiler::stats[STATNAME_ATTEMPTS_LDL_MISS_L1D].counted_inc(access_latency); break;
       case I_STW: 
         profiler::stats[STATNAME_ATTEMPTS_STW_MISS_L1D].counted_inc(access_latency); break;
       case I_STC: 
         profiler::stats[STATNAME_ATTEMPTS_STC_MISS_L1D].counted_inc(access_latency); break;
       default: break;
     }
     if (instr->stats.is_local_store())  {
       core->get_cluster()->getProfiler()->cache_stats.L1D_cache_write_misses++;
     } else{
       core->get_cluster()->getProfiler()->cache_stats.L1D_cache_read_misses++;
       uint32_t ea = instr->get_result_ea();
       profiler::stats[STATNAME_HISTOGRAM_L1D_CACHE_MISSES].inc_mem_histogram(ea);
      }
    }
  }
}
////////////////////////////////////////////////////////////////////////////////
// WriteBackStage::writeback()
////////////////////////////////////////////////////////////////////////////////
// Description:
// performs the writeback stage actions for the given instr and pipe
// inputs: the instr to operate on, and the pipe it is in
//
// returns updated InstrSlot (InstrLegacy pointer)
// (updates the one passed in as a parameter)
////////////////////////////////////////////////////////////////////////////////
InstrSlot
WriteBackStage::writeback(InstrSlot instr, int pipe) {
  using namespace simconst;
  /* TODO: Update RF and PC if necessary  */
  bool setDestRegFloat = false;
  instr_t instr_type = instr->get_type();

  /* STALL */
  if (instr_type  == I_NULL) {
    goto done_stall;
  }

  // Handle FU use count
  switch (INSTR_FUNIT[instr->get_type()])
  {
    using namespace rigel;
    case FU_NONE:         profiler::stats[STATNAME_COMMIT_FU_NONE].inc();        break;
    case FU_ALU:          profiler::stats[STATNAME_COMMIT_FU_ALU].inc();         break;
    case FU_ALU_VECTOR:   profiler::stats[STATNAME_COMMIT_FU_ALU_VECTOR].inc();  break;
    case FU_FPU:          
          if(instr_type == I_FMADD)   profiler::stats[STATNAME_COMMIT_FU_FPU].inc(); // fadds count as double flops :)
          if(instr_type == I_FA2R || instr_type == I_FR2A) { break; }
          profiler::stats[STATNAME_COMMIT_FU_FPU].inc(); 
          break;
    case FU_FPU_VECTOR:   profiler::stats[STATNAME_COMMIT_FU_FPU_VECTOR].inc();  break;
    case FU_FPU_SPECIAL:  profiler::stats[STATNAME_COMMIT_FU_FPU_SPECIAL].inc(); break;
    case FU_ALU_SHIFT:    profiler::stats[STATNAME_COMMIT_FU_ALU_SHIFT].inc();   break;
    case FU_SPFU:         profiler::stats[STATNAME_COMMIT_FU_SPFU].inc();        break;
    case FU_COMPARE:      profiler::stats[STATNAME_COMMIT_FU_COMPARE].inc();     break;
    case FU_MEM:          profiler::stats[STATNAME_COMMIT_FU_MEM].inc();         break;
    case FU_BRANCH:       profiler::stats[STATNAME_COMMIT_FU_BRANCH].inc();      break;
    case FU_BOTH:         profiler::stats[STATNAME_COMMIT_FU_BOTH].inc();        break;
    case FU_COUNT:        profiler::stats[STATNAME_COMMIT_FU_COUNT].inc();       break;

    default: assert(0 && "Unknown FUNIT for instruction.");
  }

  switch (instr_type) {
    /***********************************/
    /* XXX BEGIN VECTOR OPERATIONS XXX */
    case (I_VADD):
    case (I_VSUB):
    case (I_VFADD):
    case (I_VFSUB):
    case (I_VFMUL):
    case (I_VLDW):
    case (I_VADDI):
    case (I_VSUBI):
    {
      RegisterBase result_reg = instr->get_result_reg();
      RegisterBase result_reg1 = instr->get_result_reg1();
      RegisterBase result_reg2 = instr->get_result_reg2();
      RegisterBase result_reg3 = instr->get_result_reg3();

      core->get_regfile(instr->get_core_thread_id())->write(result_reg,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(result_reg1,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(result_reg2,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(result_reg3,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);

      core->get_scoreboard(instr->get_core_thread_id())->put_reg(result_reg.addr,  instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(result_reg1.addr, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(result_reg2.addr, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(result_reg3.addr, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(result_reg.addr);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(result_reg1.addr);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(result_reg2.addr);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(result_reg3.addr);

      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg.addr);
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg1.addr);
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg2.addr);
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg3.addr);
      }
      
      goto done_writeback;
    }
    case (I_VSTW):
    {
      goto done_writeback;
    }
    /* XXX END VECTOR OPERATIONS XXX */
    /*********************************/
    case (I_SYNC):
    {
      assert(core->get_thread_state(instr->get_core_thread_id())->pending_sync_req
        && "SYNC cannot come to writeback if it did not generate a request!");
      assert(!core->get_thread_state(instr->get_core_thread_id())->pending_sync_ack 
        && "Last sync should have cleared tye ACK flag!");

      core->get_thread_state(instr->get_core_thread_id())->pending_sync_ack = true;
      core->get_thread_state(instr->get_core_thread_id())->pending_sync_req = false;
      goto done_writeback;
    }
    case (I_DONE):// Instruction finished early.  Usually do to SYNC
    {
      goto done_writeback;
    }
    case (I_UNDEF):
    {
      DEBUG_HEADER(); 
			std::cerr << "FIXME: 'undef' should generate an exception!\n";
      goto done_writeback;
    }
    case (I_HLT):
    case (I_REP_HLT):
    {
      core->halt( instr->get_core_thread_id() );
      fprintf(stderr, "core %d (local %d) thread %d halting @ cycle %"PRIu64", PC 0x%08"PRIx32"\n",
        core->get_core_num_global(),
        core->get_core_num_local(),
        instr->get_core_thread_id(),
        rigel::CURR_CYCLE,
				instr->get_currPC()
      );
      goto done_writeback;
    }
    case (I_ABORT):
    {
      DEBUG_HEADER();
      fprintf(stderr, "!!ABORT!! (I_ABORT instr_type found) core: %d pc: %08x\n", 
        core->get_core_num_global(), instr->get_currPC());
      assert(0);
    }
    case (I_NOP): 
    case (I_CMNE_NOP):
    case (I_CMEQ_NOP):
    case (I_PRINTREG):
    case (I_START_TIMER):
    case (I_STOP_TIMER):
    case (I_SYSCALL):
    case (I_EVENT):
    case (I_PRIO):
    // Prefetches do nothing at writeback
    case (I_PREF_L):
    case (I_BCAST_INV):
    case (I_PREF_B_GC):
    case (I_PREF_B_CC):
    case (I_PREF_NGA):
    {
      /* NADA */
      goto done_writeback;
    } 

    case (I_ADD): 
    case (I_SUB):
    case (I_ADDI):
    case (I_ADDIU):
    case (I_ADDU):
    case (I_SUBI):
    case (I_SUBIU):
    case (I_SUBU):
    case (I_AND):
    case (I_OR):
    case (I_XOR):
    case (I_NOR):
    case (I_ANDI):
    case (I_ORI):
    case (I_XORI):

    case (I_SLL):
    case (I_SRL):
    case (I_SRA):
    case (I_SLLI):
    case (I_SRLI):
    case (I_SRAI):

    case (I_CEQ): 
    case (I_CLT): 
    case (I_CLE):
    case (I_CLTU): 
    case (I_CLEU):
    case (I_CEQF): 
    case (I_CLTF): 
    case (I_CLTEF):
  
    case (I_CMEQ): 
    case (I_CMNE): 
    case (I_MFSR): 
    case (I_MVUI):
    case (I_MUL):
        
    case (I_SEXTS):
    case (I_ZEXTS):
    case (I_SEXTB):
    case (I_ZEXTB):
    {
      RegisterBase result_reg = instr->get_result_reg();
      //cerr << "result reg: " << result_reg << endl;
      core->get_regfile(instr->get_core_thread_id())->write(result_reg,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(result_reg.addr, instr_type);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg.addr);
      }
      goto done_writeback;
    }

    /* Writing special-purpose register file */
    case (I_MTSR): 
    {
      RegisterBase result_reg = instr->get_result_reg();
      core->get_sprf(instr->get_core_thread_id())->write(result_reg);
      core->get_sp_scoreboard(instr->get_core_thread_id())->put_reg(result_reg.addr, instr_type);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_sp_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg.addr);
      }
      goto done_writeback;
    }

    case (I_BE):
    case (I_BN):
    case (I_BEQ):
    case (I_BNE):
    case (I_BLT): 
    case (I_BGT): 
    case (I_BLE): 
    case (I_BGE):
    case (I_LJ):
    case (I_JMP):
    case (I_JMPR):
    // Cache management instructions
    case (I_LINE_INV):
    case (I_LINE_WB):
    case (I_LINE_FLUSH):
    case (I_IC_INV):
    case (I_CC_INV):
    case (I_RFE):
    {
      goto done_writeback;
    }

    case (I_JAL):
    case (I_JALR):
    case (I_LJL):
    {
      RegisterBase rb( 
        rigel::regs::LINK_REG, // addr (rfnum)
        instr->get_result_ea()  // value
      );

      //core->get_regfile(instr->get_core_thread_id())->write(instr->get_result_ea(), rigel::regs::LINK_REG,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(rb,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(rigel::regs::LINK_REG, instr_type);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(rigel::regs::LINK_REG);
      }
      goto done_writeback;
    }
  
    case (I_ATOMXCHG):
    #if 0
    {
      RegisterBase result_reg = instr->get_result_reg();
      DEBUG_HEADER(); fprintf(stderr, "Retiring ATOMXCHG: addr: %08x core: %d rf(%d): 0x%08x\n",
      instr->get_result_ea(), core_num, result_reg.addr, result_reg.data.i32);
    }
    #endif
    case (I_ATOMCAS):
    case (I_ATOMADDU):
    case (I_ATOMINC):
    case (I_ATOMDEC):
    case (I_ATOMMAX):
    case (I_ATOMMIN):
    case (I_ATOMOR):
    case (I_ATOMAND):
    case (I_ATOMXOR):
    {
      RegisterBase result_reg = instr->get_result_reg();
      core->get_regfile(instr->get_core_thread_id())->write(result_reg, instr->get_currPC(), core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(result_reg.addr, instr_type);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg.addr);
      }
      // Unlocking handled in memory_req stage        sb->clear_reg(result_reg.addr);
      goto done_writeback;
    }

    case (I_GLDW):
    case (I_LDW):
    case (I_LDL):
    {
      RegisterBase result_reg = instr->get_result_reg();
      core->get_regfile(instr->get_core_thread_id())->write(result_reg,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(result_reg.addr, instr_type);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg.addr);
      }
      // Unlocking now handled in setCCAccess    sb->clear_reg(result_reg.addr);
      goto done_writeback;
    }

    case (I_TQ_LOOP):
    case (I_TQ_INIT):
    case (I_TQ_END):
    case (I_TQ_ENQUEUE):
    {
      using namespace rigel::task_queue;

      RegisterBase rb( 
        TQ_RET_REG, // addr/rfnum
        instr->TQ_Data.retval   // value
      );

      // TODO: Should write value returned from MEM stage
      //core->get_regfile(instr->get_core_thread_id())->write(instr->TQ_Data.retval, TQ_RET_REG,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(rb,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(TQ_RET_REG, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(TQ_RET_REG);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(TQ_RET_REG);
      }
      goto done_writeback;
    }

    case (I_TQ_DEQUEUE):
    {
      using namespace rigel::task_queue;

      core->get_regfile(instr->get_core_thread_id())->write(instr->TQ_Data.retval, TQ_RET_REG,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(instr->TQ_Data.v1, TQ_REG0,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(instr->TQ_Data.v2, TQ_REG1,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(instr->TQ_Data.v3, TQ_REG2,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_regfile(instr->get_core_thread_id())->write(instr->TQ_Data.v4, TQ_REG3,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);

      core->get_scoreboard(instr->get_core_thread_id())->put_reg(TQ_RET_REG, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(TQ_REG0, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(TQ_REG1, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(TQ_REG2, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(TQ_REG3, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(TQ_RET_REG);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(TQ_REG0);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(TQ_REG1);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(TQ_REG2);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(TQ_REG3);

      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(TQ_RET_REG);
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(TQ_REG0);
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(TQ_REG1);
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(TQ_REG2);
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(TQ_REG3);
      }

      goto done_writeback;
    }

    case (I_STC):
    {
      int stc_reg = instr->get_result_reg().addr;
      if (instr->get_bad_stc()) {
        core->get_regfile(instr->get_core_thread_id())->write(0, stc_reg,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      } else {
        core->get_regfile(instr->get_core_thread_id())->write(1, stc_reg,instr->get_currPC(),core->reg_trace_file[instr->get_core_thread_id()]);
      }

      core->get_scoreboard(instr->get_core_thread_id())->put_reg(stc_reg, instr_type);
      core->get_scoreboard(instr->get_core_thread_id())->clear_reg(stc_reg);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(stc_reg);
      }

      goto done_writeback;
    }

    case (I_BCAST_UPDATE):
    {
      goto done_writeback;
    }

    case (I_STW):
    case (I_GSTW):
    {
      goto done_writeback;
    }

    case (I_BRK): 
    {
      rigel::BREAKPOINT_PENDING = true;
      goto done_writeback;
    }

    //case (I_WAIT): TODO: delete me, need anymore?
    case (I_MB):
    //case (I_IB): TODO: delete me, need anymore?
    // case (I_NOP): 
    {
      goto done_writeback;
    }
    /* FLOATING-POINT OPERATIONS */
    case (I_FA2R):
    case (I_FADD):
    case (I_FSUB): 
    case (I_FMUL):
    case (I_FRCP):
    case (I_FRSQ):
    case (I_FABS):
    case (I_FMRS):
      setDestRegFloat = true;
    case (I_I2F):
    case (I_F2I):
    case (I_CLZ):
    case (I_TBLOFFSET):
    {
      RegisterBase result_reg = instr->get_result_reg();
      if (setDestRegFloat) result_reg.SetFloat();
      core->get_regfile(instr->get_core_thread_id())->write(result_reg, instr->get_currPC(), core->reg_trace_file[instr->get_core_thread_id()]);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(result_reg.addr, instr_type);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg.addr);
      }
      goto done_writeback;
    }
    /* FLOATING-POINT OPS WRITING TO THE ACCUMULATOR */
    //case (I_FMSUB): TODO: delete me, need anymore?
    //case (I_FMSUBR): TODO: delete me, need anymore?
    case (I_FMADD):
    {
      setDestRegFloat = true;
      RegisterBase result_reg = instr->get_result_reg();
      if (setDestRegFloat) result_reg.SetFloat();
      core->get_regfile(instr->get_core_thread_id())->write(
        result_reg, instr->get_currPC(),
        core->reg_trace_file[instr->get_core_thread_id()],
        true);
      core->get_scoreboard(instr->get_core_thread_id())->put_reg(
        result_reg.addr, instr_type, true);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg.addr);
      }
      goto done_writeback;
    }
    case (I_FR2A):
    {
      setDestRegFloat = true;
      RegisterBase result_reg = instr->get_result_reg();
      if (setDestRegFloat) result_reg.SetFloat();
      core->get_accumulator_regfile(instr->get_core_thread_id())->write(
        result_reg, instr->get_currPC(),
        core->reg_trace_file[instr->get_core_thread_id()],
        false);
      core->get_ac_scoreboard(instr->get_core_thread_id())->put_reg(
        result_reg.addr, instr_type, false);
      // If no bypass paths, unlock the register
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_none) {
        core->get_ac_scoreboard(instr->get_core_thread_id())->unlockReg(result_reg.addr);
      }
      goto done_writeback;
    }
  
    case (I_BRANCH_FALL_THROUGH):
    {
      /* Branch did nothing...just let fetch update PC and move on */
      goto done_writeback;
    }

    default: 
    {
      goto bad_writeback;
    }

  } // end switch
  
bad_writeback:
	std::cerr << "Instruction type: " << instr->get_type() << "\n";
  ExitSim((char *)"writeback: Unknown Instruction type!", 1);

done_writeback:
  // Reset watchdog
  core->set_watchdog(rigel::DEADLOCK_WATCHDOG, instr->get_core_thread_id());
  
  core->get_cluster()->getProfiler()->timing_stats.ret_num_instrs++;
  // Global count used for system timers etc.
  //Profile::global_timing_stats.ret_num_instrs++;

  // Count the number of each type of instruction to commit.
  ProfileStat::inc_instr_commit_count(instr_type);

  instr->update_type(I_DONE);

done_stall:

  return instr;
}
