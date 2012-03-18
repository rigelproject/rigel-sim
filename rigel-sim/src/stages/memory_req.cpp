
////////////////////////////////////////////////////////////////////////////////
// memory_req.cpp
////////////////////////////////////////////////////////////////////////////////
//
// MemoryStage implementations (memory writebacks in this stage)
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu64
#include <stdint.h>                     // for uint32_t, UINT32_MAX, etc
#include <stdio.h>                      // for fprintf, stderr, printf
#include "caches_legacy/cache_model.h"    // for CacheModel
#include "caches_legacy/cache_base.h"  // for CacheAccess_t
#include "cluster.h"        // for Cluster
#include "core.h"           // for CoreInOrderLegacy, ::EX2MC, ::MC2FP
#include "define.h"         // for DEBUG_HEADER, etc
#include "util/event_track.h"    // for EventTrack, global_tracker
#include "instr.h"          // for InstrLegacy, etc
#include "instrstats.h"     // for InstrStats, InstrCycleStats, etc
#include "locality_tracker.h"  // for LocalityTracker
#include "profile/profile.h"        // for Profile, InstrStat, etc
#include "core/regfile_legacy.h"        // for RegisterBase, etc
#include "core/scoreboard.h"     // for ScoreBoard
#include "sim.h"            // for MemoryModelType, NullInstr, etc
#include "util/syscall.h"        // for Syscall, syscall_struct, etc
#include "util/task_queue.h"     // for TaskDescriptor, etc
#include "stage_base.h"  // for MemoryStage
#include "memory/backing_store.h"

#if 0
#define __DEBUG_ADDR 0xff47fa60
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#undef __DEBUG_CYCLE
#endif


#define STDIN    0
#define STDOUT   1
#define STDERR   2

////////////////////////////////////////////////////////////////////////////////
// Method Name: MemoryStage::Update()
////////////////////////////////////////////////////////////////////////////////
// Description:
//
// Inputs: none
// Outputs: none
////////////////////////////////////////////////////////////////////////////////
void
MemoryStage::Update()
{
  // pointer to core state
  //pipeline_per_core_state_t *cs = core->get_core_state();

  // Update the restore pc
  UpdateRestorePC();

  for(int j=0; j < rigel::ISSUE_WIDTH; j++) {
    InstrSlot instr_next = memaccess(core->latches[EX2MC][j], j);

    // int tid = instr_next->get_core_thread_id();
    // pointer to thread state
    //pipeline_per_thread_state_t *ps = core->get_thread_state(tid);

    if(!is_stalled(j)) {
      if(core->nlatches[MC2FP][j]->get_type() != I_NULL) {
        // Account for "skipped" cores[i+x]->stage_name_here(latch...) calls
        core->get_cluster()->getProfiler()->timing_stats.mc_block += (rigel::ISSUE_WIDTH-j-1);
        break;
      }
      core->latches[EX2MC][j] = rigel::NullInstr;
      core->nlatches[MC2FP][j] = instr_next;
    }

  }
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: MemoryStage::UpdateRestorePC()
////////////////////////////////////////////////////////////////////////////////
// Description:
//
// Inputs:
// Outputs: none
////////////////////////////////////////////////////////////////////////////////
void
MemoryStage::UpdateRestorePC()
{
  bool setRestorePC = false;
  uint32_t restore_pc = 0;
  uint64_t max_instr_num = 0;
  uint32_t thread_id = UINT32_MAX;

  // pointer to core state
  //pipeline_per_core_state_t *cs = core->get_core_state();

  //int tid = core->get_thread_id_core();
  // pointer to thread state
  //pipeline_per_thread_state_t *ps = core->get_thread_state(tid);

  for(int j=0; j < rigel::ISSUE_WIDTH; ++j) {
    if(core->latches[EX2MC][j] != rigel::NullInstr) {
      if(core->latches[EX2MC][j]->GetInstrNumber() > max_instr_num) {
        restore_pc = core->latches[EX2MC][j]->get_nextPC();
        setRestorePC = true;
        thread_id = core->latches[EX2MC][j]->get_core_thread_id();
      }
    }
  }
  if(setRestorePC) {
    if(core->get_core_num_global() == 0) {
  //    printf("restore pc = 0x%x\n", restore_pc);
    }
    core->set_mt_restore_pc(restore_pc, thread_id);
  }
}


////////////////////////////////////////////////////////////////////////////////
// MemoryStage::memaccess()
////////////////////////////////////////////////////////////////////////////////
InstrSlot
MemoryStage::memaccess(InstrSlot instr, int pipe) {

  //cerr << " Running memaccess on core: " << core << endl;

  instr_t instr_type = instr->get_type();
  // Set the last access cycle.  This can be repeated, monotonically
  // updating the value.  If it goes to ccaccess stage, then it will get set
  // again there.
  instr->set_last_memaccess_cycle();

  assert(I_PRE_DECODE != instr->get_type());

  // check if an older instruction is stalled "ahead" of us in this pipeline
  // stage (enforce in-order) 
  // If this instruction is in slot 0, it is the oldest so there are none
  // already stalled ahead
  if (pipe > 0 && is_stalled(pipe - 1)) {
    core->get_cluster()->getProfiler()->timing_stats.mc_block++;
    return instr;
  }

  /* STALL */
  if (instr_type == I_NULL) {
    #ifdef DEBUG
    cerr << "MC: [[BUBBLE]]" << endl;
    #endif
    core->get_cluster()->getProfiler()->timing_stats.mc_bubble++;
    return instr;
  }

  #ifdef DEBUG
  cerr << "MC: currPC: 0x" << HEX_COUT << instr->get_currPC() << endl;
  #endif

#define TQ_MEMWB_PROLOG() \
      using namespace rigel::task_queue; \
      if (instr->doneMemAccess()) { goto done_memaccess; } \
      uint32_t CORE_NUM = core->get_core_num_global(); \
      instr->TQ_Data.v1 = 0xdead0001; \
      instr->TQ_Data.v2 = 0xdead0002; \
      instr->TQ_Data.v3 = 0xdead0003; \
      instr->TQ_Data.v4 = 0xdead0004; \
      instr->TQ_Data.retval = TQ_RET_SUCCESS;

// We want to make the instruction stall until the queue unblocks.  That way we
// only execute a single instruction no matter how long we wait for
#define TQ_MEMWB_EPILOG() \
      if (instr->TQ_Data.retval == TQ_RET_BLOCK) { \
        stall(pipe);      \
        core->get_cluster()->getProfiler()->timing_stats.tq_stall++; \
        return instr;          \
      }                        \
      unstall(pipe);       \
      instr->setMemAccess();   \
      goto done_memaccess;
  #ifdef DEBUG_MT
  DEBUG_HEADER();
  cerr << "  MEM-WB INSTR DUMP\n";
  instr->dump();
  #endif
  /* DO ACCESS */
  switch (instr_type) {
    case (I_TQ_INIT):
    {
      TQ_MEMWB_PROLOG();

      // FIXME: Add support for defining number of threads and task queue ID
      int max_tasks = instr->get_sreg1().data.i32;
      int qid = instr->get_sreg0().data.i32;

      TaskQueueDesc tqdesc(qid, max_tasks);
      // Initialize the queue with default values for now
      instr->TQ_Data.retval =
        GlobalTaskQueue->TQ_Init(CORE_NUM, max_tasks, tqdesc);

      TQ_MEMWB_EPILOG();
    }
    case (I_TQ_END):
    {
      TQ_MEMWB_PROLOG();

      // Shutdown the task queue
      instr->TQ_Data.retval =
        GlobalTaskQueue->TQ_End(CORE_NUM);

      TQ_MEMWB_EPILOG();
    }
    case (I_TQ_ENQUEUE):
    {
      TQ_MEMWB_PROLOG();

      TaskDescriptor tdesc;
      tdesc.v1 = instr->get_sreg0().data.i32;
      tdesc.v2 = instr->get_sreg1().data.i32;
      tdesc.v3 = instr->get_sreg2().data.i32;
      tdesc.v4 = instr->get_sreg3().data.i32;

      instr->TQ_Data.retval =
        GlobalTaskQueue->TQ_Enqueue(CORE_NUM, tdesc);

      TQ_MEMWB_EPILOG();
    }
    case (I_TQ_DEQUEUE):
    {
      TQ_MEMWB_PROLOG();

      TaskDescriptor tdesc;

      instr->TQ_Data.retval =
        GlobalTaskQueue->TQ_Dequeue(CORE_NUM, tdesc);
      instr->TQ_Data.v1 = tdesc.v1;
      instr->TQ_Data.v2 = tdesc.v2;
      instr->TQ_Data.v3 = tdesc.v3;
      instr->TQ_Data.v4 = tdesc.v4;

      TQ_MEMWB_EPILOG();
    }
    case (I_TQ_LOOP):
    {
      TQ_MEMWB_PROLOG();

      TaskDescriptor tdesc;
      // IP and data for the enqueued loop
      tdesc.v1 = instr->get_sreg0().data.i32;
      tdesc.v2 = instr->get_sreg1().data.i32;
      // These are fields filled in by the task queue hardware
      tdesc.v3 = 0;
      tdesc.v4 = 0;
      // N: Total iterations S: Iterations per task.  TQ HW handles the rest
      int N = instr->get_sreg2().data.i32;
      int S = instr->get_sreg3().data.i32;

      if (0 == S) {
        DEBUG_HEADER();
        fprintf(stderr, "[[ERROR]] Found iterations per task value of '0'.  Will generate a SIGFP!\n");
        assert(0);
      }

      instr->TQ_Data.retval =
        GlobalTaskQueue->TQ_EnqueueLoop(CORE_NUM, tdesc, N, S);


      TQ_MEMWB_EPILOG();
    }
    case (I_BCAST_INV):
    case (I_BCAST_UPDATE):
    {
      bool stage_stall = true;
      uint32_t ea = instr->get_result_ea();
      uint32_t memval = instr->get_result_reg().data.i32;
      assert((0 == (ea & 0x03)) && "Unaligned broadcast!");

      // FIXME: Only allow a single load in flight at a time.
      if (core->GetLoadAccessPending(instr->get_core_thread_id())) {
        Profile::accum_cache_stats.prefetch_line_stall_total++;
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return instr;
      }


      CacheAccess_t ca(
        ea,                           // address
        core->get_core_num_local(),   // core number
        instr->get_local_thread_id(), // thread id
        IC_MSG_ERROR,   // must be set to valid before access
        instr                         // instruction correlated to this access
      );
      // Hit the cache with a prefetch if we can
      stage_stall = core->get_cluster()->getCacheModel()->broadcast( ca, memval );

      instr->setMemAccess();
      // Broadcasts are like writes logically, but are implemented in the
      // simulator more like loads.  The reason for doing so is that RigelSim
      // updates memory instantaneously which means if we did the broadcast and
      // it hit in the cache, it would appear to take zero time to propagate to
      // all of the other caches.  Not what we want.  We should just stay
      // stalled until we get an Ack back from the global cache.  At that point
      // we update memory and make it visible to all of the other cores.
      if (stage_stall) {
        stall(pipe);
        return rigel::NullInstr;
      }

      if (is_stalled(pipe)) unstall(pipe);

      using namespace simconst;
      instr->set_result_RF(
        0xdeadbeef,  // Never read
        NULL_REG,    // Should not write either
        ea
      );

      instr->setMemAccess();

      goto done_memaccess;
    }
    case (I_PREF_L):
    case (I_PREF_NGA):
    case (I_PREF_B_GC):
    case (I_PREF_B_CC):
    {
      bool stage_stall = true;
      uint32_t ea = instr->get_result_ea();

        // Disabling SW prefetching for coherence tests.
        if (1 && !rigel::ENABLE_EXPERIMENTAL_DIRECTORY)
        {
        // FIXME: Only allow a single load in flight at a time.
        if (core->GetLoadAccessPending(instr->get_core_thread_id())) {
          Profile::accum_cache_stats.prefetch_line_stall_total++;
          stall(pipe);
          core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
          return instr;
        }

        // Hit the cache with a prefetch if we can
        stage_stall = !core->get_cluster()->getCacheModel()->prefetch_line(
          *instr,
          core->get_core_num_local(),
          ea,
          instr->get_local_thread_id()
        );

        instr->setMemAccess();
        // Prefetches are treated just like loads in the case of a stall.
        // Currently it should only stall if the L2 runs out of MHSRs.
        if (stage_stall && !rigel::cache::CMDLINE_NONBLOCKING_PREFETCH) {
          stall(pipe);
          goto done_memaccess;
        }
      } else {
        // Even in the PREF == NOP case, we need to set memory access so that this
        // does not get replayed.
            instr->setMemAccess();
      }

      // Prefetch went through to L2 controller
      instr->setCCAccess();
      unstall(pipe);
      using namespace simconst;
      instr->set_result_RF(
        0xdeadbeef,  // Never read
        NULL_REG,    // Should not write either
        ea
      );

      // Only count on valid prefetch.
      Profile::accum_cache_stats.prefetch_line_accesses++;

      goto done_memaccess;
    }
    case (I_MB):
    case (I_LINE_INV):
    case (I_IC_INV):
    case (I_CC_INV):
    {
      uint32_t ea = instr->get_result_ea();
      // Short circuit for finished memops
      if (instr->doneMemAccess()) { goto done_memaccess; }

      // FIXME: Only allow a single load in flight at a time.  This fixes a bug
      // where by an invalidate could potentially invalidate a previous load or
      // store operation still in flight.
      if (core->GetLoadAccessPending(instr->get_core_thread_id())) {
        Profile::accum_cache_stats.load_stall_total++;
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return instr;
      }

      switch (instr->get_type())
      {
        case (I_LINE_INV):
        {
          bool stage_stall = false;
          CacheAccess_t ca(
            ea,                          // address
            core->get_core_num_local(),  // core number (faked)
            instr->get_local_thread_id(),// thread id at cluster
            IC_MSG_ERROR,                // will fault if later accessed and not set valid
            instr                        // instruction correlated to this access
          );
          // Get CacheModel to invalidate the line in the CCache and our L1
          //core->get_cluster()->getCacheModel()->invalidate_line( core->get_core_num_local(), ea, stage_stall );
          core->get_cluster()->getCacheModel()->invalidate_line( ca, stage_stall );

          if (stage_stall) {
            stall(pipe);
            core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
            return rigel::NullInstr;
          }

          break;
        }
        case (I_IC_INV):
        {
          // Get CacheModel to invalidate the line in the CCache and our L1
          core->get_cluster()->getCacheModel()->invalidate_instr_all();
          break;
        }
        // FIXME: This may be buggy.
        case (I_CC_INV):
        {
          // Get CacheModel to invalidate the line in the CCache and our L1
          core->get_cluster()->getCacheModel()->invalidate_all();
          break;
        }
        case (I_MB):
        {
          if (core->get_cluster()->getCacheModel()->memory_barrier(*instr, core->get_core_num_local())) {
            // MB stall, try again next cycle
            Profile::accum_cache_stats.load_stall_total++;
            stall(pipe);
            core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
            return instr;
          }
          break;
        }
        default: assert(0 && "Unimplemented cache management instruction");
      }

      unstall(pipe);

      goto done_memaccess;
    }
    case (I_LDW):
    case (I_LDL):
    case (I_VLDW):
    {

      // For PER_CORE_STACK_SIZE
      using namespace rigel::memory;
      using namespace rigel;
    
      uint32_t ea = instr->get_result_ea();
      // Short circuit for finished loads
      if (instr->doneMemAccess()) { goto done_memaccess; }

      if (ea < CODEPAGE_HIGHWATER_MARK) {
        DEBUG_HEADER();
        fprintf(stderr, "[MC] Attempting to read to a code word! "
                        "PC: 0x%08x addr: 0x%08x core: %d \n",
                instr->get_currPC(), ea, core->get_core_num_global());
        // Dump stack trace
        {
          uint32_t sp_addr = core->get_regfile(instr->get_core_thread_id())->read(29).data.i32;
          fprintf(stderr, "--------- STACK TRACE --------\n");
          fprintf(stderr, "STACK PTR: 0x%08x\n", sp_addr);
          for (int i = 0; i < 16; i++) {
            fprintf(stderr, "[0x%08x] ", sp_addr + 32*i);
            for (int j = 0; j < 32; j += 4) {
              uint32_t addr = sp_addr + (32*i) + j;
              uint32_t data = UINT32_MAX;
              data = core->get_cluster()->getCacheModel()->backing_store.read_word(addr);
              fprintf(stderr, "(%3d) %08x ", i*32+j, data);
            }
            fprintf(stderr, "\n");
          }
        }
        // Dump the registerfile state.
        core->get_regfile(instr->get_core_thread_id())->dump();
        // Dump the scoreboard state.
        core->get_scoreboard(instr->get_core_thread_id())->dump();
        // Dump the instruction state.
        instr->dump();
        // TODO: Dump pipeline state.
        // this->dump();
        fprintf(stderr,"cycle %" PRIu64 "\n", rigel::CURR_CYCLE);
        assert(0 && "ea<CODEPAGE_HIGHWATER_MARK");
      } 
      // end debug dump for codeword access
      else if (
        // Only if enabled
        rigel::CMDLINE_CHECK_STACK_ACCESSES &&
        // TODO: Check to see if the location is in another core's stack.
        (ea > (0x0 - (rigel::THREADS_TOTAL*PER_THREAD_STACK_SIZE))) &&
         ((ea > (0xFFFFFFFF - (instr->get_global_thread_id()*PER_THREAD_STACK_SIZE))) ||
         (ea < (0x0 - ((1+instr->get_global_thread_id())*PER_THREAD_STACK_SIZE
        )))))
      {
        DEBUG_HEADER();
        fprintf(stderr, "[MC] Attempting to read a different core's stack!! "
                        "PC: 0x%08x addr: 0x%08x core: %d \n",
                instr->get_currPC(), ea, core->get_core_num_global());
        // Dump stack trace
        {
          uint32_t sp_addr = core->get_regfile(instr->get_core_thread_id())->read(29).data.i32;
          fprintf(stderr, "--------- STACK TRACE --------\n");
          fprintf(stderr, "STACK PTR: 0x%08x\n", sp_addr);
          for (int i = 0; i < 16; i++) {
            fprintf(stderr, "[0x%08x] ", sp_addr + 32*i);
            for (int j = 0; j < 32; j += 4) {
              uint32_t addr = sp_addr + (32*i) + j;
              uint32_t data = UINT32_MAX;
              data = core->get_cluster()->getCacheModel()->backing_store.read_word(addr);
              fprintf(stderr, "(%3d) %08x ", i*32+j, data);
            }
            fprintf(stderr, "\n");
          }
        }
        // Dump the registerfile state.
        core->get_regfile(instr->get_core_thread_id())->dump();
        // Dump the scoreboard state.
        core->get_scoreboard(instr->get_core_thread_id())->dump();
        // Dump the instruction state.
        instr->dump();
        // TODO: Dump pipeline state.
        // this->dump();
        assert(0);
      }

      // FIXME: Only allow a single load in flight at a time.
      if (core->GetLoadAccessPending(instr->get_core_thread_id())) {
         // cerr << " we are stalled here?" << core << endl;

        Profile::accum_cache_stats.load_stall_total++;
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return instr;
      }

      uint32_t memval = 0xdeadbeef;
      bool stage_stall = false;

      if ((ea & 0x3) != 0) {
        DEBUG_HEADER();
        fprintf(stderr, "Unaligned load access! PC: 0x%08x addr: 0x%08x core: %d\n",
          instr->get_currPC(), ea, core->get_core_num_global());
        // Dump stack trace
        {
          uint32_t sp_addr = core->get_regfile(instr->get_core_thread_id())->read(29).data.i32;
          fprintf(stderr, "--------- STACK TRACE --------\n");
          fprintf(stderr, "STACK PTR: 0x%08x\n", sp_addr);
          for (int i = 0; i < 16; i++) {
            fprintf(stderr, "[0x%08x] ", sp_addr + 32*i);
            for (int j = 0; j < 32; j += 4) {
              uint32_t addr = sp_addr + (32*i) + j;
              uint32_t data = UINT32_MAX;
              data = core->get_cluster()->getCacheModel()->backing_store.read_word(addr);
              fprintf(stderr, "(%3d) %08x ", i*32+j, data);
            }
            fprintf(stderr, "\n");
          }
        }
        // Dump the registerfile state.
        core->get_regfile(instr->get_core_thread_id())->dump();
        // Dump the scoreboard state.
        core->get_scoreboard(instr->get_core_thread_id())->dump();
        // Dump the instruction state.
        instr->dump();
        // TODO: Dump pipeline state.
        // this->dump();
        fprintf(stderr,"cycle %" PRIu64 "\n", rigel::CURR_CYCLE);
        assert(0 && "Unaligned load");
      }

      // Hit counters for profiling.
      bool l3d_hit, l2d_hit, l1d_hit;

     //XXX Neal... this stage should stall on reads. This read access code just
      //performs a cache request, while in the ccache_access stage we check on the
      //reply.
       MemoryAccessStallReason masr =
       core->get_cluster()->getCacheModel()->read_access(*instr,
          core->get_core_num_local(), ea, memval, stage_stall, l3d_hit, l2d_hit, l1d_hit,
          CoreInOrderLegacy::ClusterNum(core->get_core_num_global()),
          instr->get_local_thread_id());


      //handle the stall reason
      //if we win arbitration and make it out to the ccache, do not stall here
      // - we will eventually stall at the ccache_access stage waiting on a L1D
      // fill.
      //
      // we fix the stall condition code here: ->
      bool l2_scheduled = false;
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
          l2_scheduled = true;
          break;
        case L1DPending:
          instr->stats.cycles.l1d_pending++;
          // FIXME Neal - should this be here? stage_stall = false;
          break;
        case L1DMSHR:
          instr->stats.cycles.l1d_mshr++;
          break;
        case L2DArbitrate:
          instr->stats.cycles.l2d_arbitrate++;
          break;
        case L2DAccess:
          instr->stats.cycles.l2d_access++;
          l2_scheduled = true;
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

      // Update profiler if necessary.  Only count CC hits/misses on L1 misses.
      instr->stats.cache.set_l1d_hit(l1d_hit);
      instr->stats.cache.set_l2d_hit(l2d_hit); 
      instr->stats.cache.set_l3d_hit(l3d_hit);

      #ifdef DEBUG
      cerr << " [[LOAD ACCESS]] EA: " << HEX_COUT << ea;
      cerr << " V: " << HEX_COUT << memval;
      cerr << " stall: " << boolalpha << stall << endl;
      #endif

      //We have an L1 miss and have scheduled a cluster cache access
      //this instruction continues to the ccaccess stage where it is filled
      if (l2_scheduled) {
        // cerr << " access scheduled at the cluster cache " << core << endl;
        unstall(pipe);
        core->SetLoadAccessPending(instr->get_core_thread_id());

        goto done_memaccess;

      }

      if (stage_stall) {
        // cerr << " stalled load on core: " << core << endl;
        // FIXME: Until this clears, no other load can pass!
       //XXX Neal moved below core->SetLoadAccessPending(instr->get_core_thread_id());
        stall(pipe);

        goto done_memaccess;
      }

      //Do locality tracking for this access, both per-thread and aggregate (per-core)
      //This should only be done once per load-or-store, so we put it *after* we have
      //ascertained that stage_stall is false and the load was successful.
      if(rigel::ENABLE_LOCALITY_TRACKING)
      {
        core->aggregateLocalityTracker->addAccess(ea);
        core->perThreadLocalityTracker[instr->get_core_thread_id()]->addAccess(ea);
      }

      //Do TLB tracking for this access, both per-thread and aggregate (per-core),
      //data-only and unified TLBs.
      //This should only be done once per load-or-store, so we put it *after* we have
      //ascertained that stage_stall is false and the load was successful.
			//NOTE: Disabled UTLBs because they don't make much sense at the core level
			//(a UTLB would be on the critical path for fetch and L1D access, and would thus be
			//either too slow or too power-hungry)
      if(rigel::ENABLE_TLB_TRACKING)
      {
        for(size_t i = 0; i < core->num_tlbs; i++) {
          core->aggregateDTLB[i]->access(ea);
          //core->aggregateUTLB[i]->access(ea);
          if(rigel::THREADS_PER_CORE > 1) {
            core->perThreadDTLB[instr->get_core_thread_id()][i]->access(ea);
            //core->perThreadUTLB[instr->get_core_thread_id()][i]->access(ea);
          }
        }
      }

      // Memory address based sharing tracker, the preceding tracker is based on cache
      // hits, this one is based only on addresses touched. It implements a state machine
      // to follow addresses thru the various pseudo-states of accesses
      // Only enabled when set at the command line.
      if (rigel::CMDLINE_DUMP_SHARING_STATS)
      {
	int tid = instr->get_global_thread_id();
        int t   = rigel::event::global_tracker.GetCurrentTaskNum(tid);
        uint32_t pc = instr->get_currPC();
        #ifdef DEBUG_MEMTRACKER
        fprintf(stdout,"READ: core %d tid %d task %d addr 0x%08x pc 0x%08x\n",core,tid,t,ea,pc);
        #endif
        ProfileStat::MemRdTrack( ea, pc, t );
      }

      //We have and L1 hit, so set the access like we expect
      //core->SetLoadAccessPending(instr->get_core_thread_id());
      instr->setMemAccess();
      instr->setCCAccess();

      // For load-linked we need to register the access
      if (I_LDL == instr_type) {
        core->get_cluster()->getCacheModel()->load_link_request(*instr, instr->get_local_thread_id(),
          ea, sizeof(uint32_t)
        );
      }

      // We now have the value and no stall will occur.  We need to call
      // setCCAccess to forward the data on the bypass network and it prevents
      // the CC stage from becoming active.
      //
      // On a prefetch this is ignored
//      instr->set_result_RF(memval, instr->get_result_reg().addr);
     //XXX removed instr->setCCAccess();

      unstall(pipe);
      // For vector operations, since they are 128-bit (four word) aligned, we
      // can assume that once one is a hit, they all are a hit.
      if (instr->get_is_vec_op()) {
        uint32_t mv0, mv1, mv2, mv3;
        uint32_t addr0, addr1, addr2, addr3;
        if (((ea) & (0xF)) != 0x0) {
          DEBUG_HEADER();
          fprintf(stderr, "Unaligned vector load addr: 0x%08x\n", ea);
          assert(0);
        }
        addr0 = ea + 0x0;
        addr1 = ea + 0x4;
        addr2 = ea + 0x8;
        addr3 = ea + 0xc;
        // addr0 already is a hit so the rest must be
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
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg().addr, instr->get_result_reg().data.i32);
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg1().addr, instr->get_result_reg1().data.i32);
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg2().addr, instr->get_result_reg2().data.i32);
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg3().addr, instr->get_result_reg3().data.i32);
#if 0
DEBUG_HEADER();
fprintf(stderr, "VLDW [MC] core: %d PC %08x: ", core_num, instr->get_currPC());
fprintf(stderr, "r0(%d) ea 0x%08x d 0x%08x ", instr->get_result_reg().addr, addr0, mv0);
fprintf(stderr, "r1(%d) ea 0x%08x d 0x%08x ", instr->get_result_reg1().addr, addr1, mv1);
fprintf(stderr, "r2(%d) ea 0x%08x d 0x%08x ", instr->get_result_reg2().addr, addr2, mv2);
fprintf(stderr, "r3(%d) ea 0x%08x d 0x%08x\n", instr->get_result_reg3().addr, addr3, mv3);
#endif
        // "Upgrade" to a regular bypass register
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(instr->get_result_reg().addr);
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(instr->get_result_reg1().addr);
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(instr->get_result_reg2().addr);
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(instr->get_result_reg3().addr);
      } else {
        // "Upgrade" to a regular bypass register
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(instr->get_result_reg().addr);
        instr->set_result_RF(
          memval,
          instr->get_dreg().addr,
          ea
        );
        // Forward data on the bypass network
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg().addr, instr->get_result_reg().data.i32);
      }

      // Load access actually completed
      // FIXME: Account for prefetches separately
      Profile::accum_cache_stats.load_accesses++;
      // FIXME: Update scoreboard with values just calculated.  May want to fix this so
      // FIXME: that you make data available this cycle instead of forcing it to go
      // FIXME: through the RF in writeback

      goto done_memaccess;
    }
    case (I_SYSCALL):
    {
      struct syscall_struct syscall_data;

      // Pointer to syscall_struct
      RIGEL_ADDR_T addr = instr->get_sreg0().data.i32;
      // Read in the parameters and sanity check?
      //We get words 0, 2, 3, and 4 of the __suds_syscall_struct.
      //Word 1 is for the result.
      syscall_data.syscall_num = core->getBackingStore()->read_word(addr+0*sizeof(uint32_t));
      syscall_data.arg1.u = core->getBackingStore()->read_word(addr+2*sizeof(uint32_t));
      syscall_data.arg2.u = core->getBackingStore()->read_word(addr+3*sizeof(uint32_t));
      syscall_data.arg3.u = core->getBackingStore()->read_word(addr+4*sizeof(uint32_t));

#if 0
      std::cerr << "SYSCALL 0x" << hex << setw(2) << setfill('0') << syscall_data.syscall_num;
      std::cerr << " (" << setw(8) << setfill(' ') << SYSCALL_NAME_TABLE[syscall_data.syscall_num] << ")";
      std::cerr << " global corenum: " << dec << get_core_num_global();
      std::cerr << " { 0x" << hex << syscall_data.arg1 << ", 0x" << hex << syscall_data.arg2;
      std::cerr << ", 0x" << hex << syscall_data.arg3 << "} " << std::endl;
#endif
      // TODO Write a read memory memcpy() type function for the simulator
      /* TODO: Perform system call */
      // Syscall names are defined in syscall.h
      //


// NOTE: this is handled internally in the syscall class now
      core->system_calls->doSystemCall(syscall_data);
#if 0
      switch (syscall_data.syscall_num) {
        /* RIGEL SYSCALLS */
        case (SYSCALL_STRTOF): // strtof() {
          core->system_calls->syscall_strtof(syscall_data);
          break;
        }
        case (SYSCALL_FTOSTR): // strtof() {
          core->system_calls->syscall_ftostr(syscall_data);
          break;
        }
        case (SYSCALL_FILEMAP): // filemap() {
          core->system_calls->syscall_filemap(syscall_data);
          break;
        }
        case (SYSCALL_FILESLURP): // fileslurp() {
          core->system_calls->syscall_fileslurp(syscall_data);
          break;
        }
        case (SYSCALL_FILEDUMP): // filedump() {
          core->system_calls->syscall_filedump(syscall_data);
          break;
        }
        /* START UNIX SYSCALLS */
        case (SYSCALL_WRITE): // write() {
          core->system_calls->syscall_write(syscall_data);
          break;
        }
        case (SYSCALL_READ): // read() {
          core->system_calls->syscall_read(syscall_data);
          break;
        }
        case (SYSCALL_OPEN): // open() {
          core->system_calls->syscall_open(syscall_data);
          break;
        }
        case (SYSCALL_CLOSE): // close() {
          core->system_calls->syscall_close(syscall_data);
          break;
        }
        case (SYSCALL_LSEEK): // lseek()
          core->system_calls->syscall_lseek(syscall_data);
          break;
        case (SYSCALL_STARTTIMER):
          core->system_calls->syscall_starttimer(syscall_data);
          break;
        case (SYSCALL_GETTIMER):
          core->system_calls->syscall_gettimer(syscall_data);
          break;
        case (SYSCALL_STOPTIMER):
          core->system_calls->syscall_stoptimer(syscall_data);
          break;
        case (SYSCALL_CLEARTIMER):
          core->system_calls->syscall_cleartimer(syscall_data);
          break;
        case SYSCALL_SETMEMORY:
        case SYSCALL_COPYMEMORY:
        case SYSCALL_COMPAREMEMORY:
          assert(0 && "SYSCALL not yet implemented!");
        case SYSCALL_READF:
          core->system_calls->syscall_readf(syscall_data);
          break;
        case SYSCALL_READI:
          core->system_calls->syscall_readi(syscall_data);
          break;
        case SYSCALL_READUI:
          core->system_calls->syscall_readui(syscall_data);
          break;
        case SYSCALL_READB:
          core->system_calls->syscall_readb(syscall_data);
          break;
        case SYSCALL_READUB:
          core->system_calls->syscall_readub(syscall_data);
          break;
        case SYSCALL_WRITEF:
          core->system_calls->syscall_writef(syscall_data);
          break;
        case SYSCALL_WRITEI:
          core->system_calls->syscall_writei(syscall_data);
          break;
        case SYSCALL_WRITEUI:
          core->system_calls->syscall_writeui(syscall_data);
          break;
        case SYSCALL_WRITEB:
          core->system_calls->syscall_writeb(syscall_data);
          break;
        case SYSCALL_WRITEUB:
          core->system_calls->syscall_writeub(syscall_data);
          break;
        case SYSCALL_RANDOMF:
          core->system_calls->syscall_randomf(syscall_data);
          break;
        case SYSCALL_RANDOMI:
          core->system_calls->syscall_randomi(syscall_data);
          break;
        case SYSCALL_RANDOMU:
          core->system_calls->syscall_randomu(syscall_data);
          break;
        case SYSCALL_SRAND:
          core->system_calls->syscall_srand(syscall_data);
          break;
        case SYSCALL_PRINTSTRING:
          core->system_calls->syscall_printstring(syscall_data);
          break;
        //case (SYSCALL_UNLINK): // unlink()
        //case (SYSCALL_LINK): // link()
        //case (SYSCALL_STAT): // stat()
        //case (SYSCALL_FSTAT): // fstat()
        //case (SYSCALL_LSTAT): // lstat()
        //case (SYSCALL_CHMOD): // chmod()
        //case (SYSCALL_RENAME): // rename()
        //case (SYSCALL_MKSTEMP): //  mkstemp()
        //case (SYSCALL_FTRUNCATE): // ftruncate()
        default:
					fprintf(stderr, "Error, syscall %d not yet implemented\n", syscall_data.syscall_num);
          assert(0 && "SYSCALL not yet implemented!");
          break;
      }
#endif

      // TODO: Set the result value
      core->getBackingStore()->write_word(addr+1*sizeof(uint32_t),
            syscall_data.result.u);

      goto done_memaccess;
    }
     case (I_STW):
     case (I_STC):
     case (I_VSTW):
    {
      // Short circuit for finished stores
      if (instr->doneMemAccess()) { goto done_memaccess; }

      // FIXME:  If a load is in flight, hold back the store.  The better way to
      // do it would be to check the address of the pending load...
      if (core->GetLoadAccessPending(instr->get_core_thread_id())) {
        #ifdef __DEBUG_ADDR
        if (__DEBUG_ADDR == (instr->get_result_ea() & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
          DEBUG_HEADER(); fprintf(stderr, "[STAGE MEMWB] Store stalled on load! ");
                          fprintf(stderr, "addr: %08x pc: %08x core: %4d\n",
                            instr->get_result_ea(), instr->get_currPC(),
                              core->get_core_num_global());
        }
        #endif
        Profile::accum_cache_stats.load_stall_total++;
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return instr;
      }
      using namespace rigel::memory;

      uint32_t memval;
      bool stage_stall = false;
      uint32_t ea = instr->get_result_ea();

      if ((ea & 0x3) != 0) {
        DEBUG_HEADER();
        fprintf(stderr, "Unaligned store access! PC: 0x%08x addr: 0x%08x core: %d\n",
          instr->get_currPC(), ea, core->get_core_num_global());
        // Dump stack trace
        {
          uint32_t sp_addr = core->get_regfile(instr->get_core_thread_id())->read(29).data.i32;
          fprintf(stderr, "--------- STACK TRACE --------\n");
          fprintf(stderr, "STACK PTR: 0x%08x\n", sp_addr);
          for (int i = 0; i < 16; i++) {
            fprintf(stderr, "[0x%08x] ", sp_addr + 32*i);
            for (int j = 0; j < 32; j += 4) {
              uint32_t addr = sp_addr + (32*i) + j;
              uint32_t data = UINT32_MAX;
              data = core->get_cluster()->getCacheModel()->backing_store.read_word(addr);
              fprintf(stderr, "(%3d) %08x ", i*32+j, data);
            }
            fprintf(stderr, "\n");
          }
        }
        // Dump the registerfile state.
        core->get_regfile(instr->get_core_thread_id())->dump();
        // Dump the scoreboard state.
        core->get_scoreboard(instr->get_core_thread_id())->dump();
        // Dump the instruction state.
        instr->dump();
        // TODO: Dump pipeline state.
        // this->dump();
        assert(0);
      }
      if (ea < rigel::CODEPAGE_HIGHWATER_MARK) {
        DEBUG_HEADER();
        fprintf(stderr, "[MC] Attempting to write to a code word! "
                        "PC: 0x%08x addr: 0x%08x core: %d \n",
                instr->get_currPC(), ea, core->get_core_num_global());
        // Dump stack trace
        {
          uint32_t sp_addr = core->get_regfile(instr->get_core_thread_id())->read(29).data.i32;
          fprintf(stderr, "--------- STACK TRACE --------\n");
          fprintf(stderr, "STACK PTR: 0x%08x\n", sp_addr);
          for (int i = 0; i < 16; i++) {
            fprintf(stderr, "[0x%08x] ", sp_addr + 32*i);
            for (int j = 0; j < 32; j += 4) {
              uint32_t addr = sp_addr + (32*i) + j;
              uint32_t data = UINT32_MAX;
              data = core->get_cluster()->getCacheModel()->backing_store.read_word(addr);
              fprintf(stderr, "(%3d) %08x ", i*32+j, data);
            }
            fprintf(stderr, "\n");
          }
        }
        // Dump the registerfile state.
        core->get_regfile(instr->get_core_thread_id())->dump();
        // Dump the scoreboard state.
        core->get_scoreboard(instr->get_core_thread_id())->dump();
        // Dump the instruction state.
        instr->dump();
        assert(0);
      } else if (
        // Only if enabled
        rigel::CMDLINE_CHECK_STACK_ACCESSES &&
        // TODO: Check to see if the location is in another core's stack.
        (ea > (0x0 - (rigel::THREADS_TOTAL*PER_THREAD_STACK_SIZE))) &&
         ((ea > (0xFFFFFFFF - (instr->get_global_thread_id()*PER_THREAD_STACK_SIZE))) ||
         (ea < (0x0 - ((1+instr->get_global_thread_id())*PER_THREAD_STACK_SIZE
        )))))
      {
        DEBUG_HEADER();
        fprintf(stderr, "[MC] Attempting to write a different core's stack!! "
                        "Cycle: %" PRIu64 " PC: 0x%08x addr: 0x%08x core: %d \n",
                rigel::CURR_CYCLE, instr->get_currPC(), ea, core->get_core_num_global());
        // Dump the registerfile state.
        core->get_regfile(instr->get_core_thread_id())->dump();
        // Dump the scoreboard state.
        core->get_scoreboard(instr->get_core_thread_id())->dump();
        // Dump the instruction state.
        instr->dump();
        // TODO: Dump pipeline state.
        // this->dump();
        assert(0);
      }

      memval = instr->get_result_reg().data.i32;

      // For profiler.
      bool l1d_hit, l2d_hit, l3d_hit;
      MemoryAccessStallReason masr =
        core->get_cluster()->getCacheModel()->write_access(*instr, core->get_core_num_local(), ea, memval,
        stage_stall, l3d_hit, l2d_hit, l1d_hit, -1, instr->get_local_thread_id());

      switch(masr)
      {
        case NoStall:
          break;
        case L2DPending:
        // FIXME: We probably want to break out coherence stalls at some point.
        case L2DCoherenceStall:
          instr->stats.cycles.l2d_pending++;
          break;
        case L1DPending:
          instr->stats.cycles.l1d_pending++;
          break;
        case L1DMSHR:
          instr->stats.cycles.l1d_mshr++;
          break;;
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
      cerr << " [[STORE ACCESS]] EA: " << HEX_COUT << ea;
      cerr << " V: " << HEX_COUT << memval;
      cerr << " stall: " << boolalpha << stall << endl;
      #endif

      if (stage_stall) {
        Profile::accum_cache_stats.store_stall_total++;
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return rigel::NullInstr;
      }

      //Do locality tracking for this access, both per-thread and aggregate (per-core)
      //This should only be done once per load-or-store, so we put it *after* we have
      //ascertained that stage_stall is false and the store was successful.
      if(rigel::ENABLE_LOCALITY_TRACKING)
      {
        core->aggregateLocalityTracker->addAccess(ea);
        core->perThreadLocalityTracker[instr->get_core_thread_id()]->addAccess(ea);
      }

      // Memory address based sharing tracker, the preceding tracker is based on cache
      // hits, this one is based only on addresses touched. It implements a state machine
      // to follow addresses thru the various pseudo-states of accesses
      // Only enabled when set at the command line.
      if (rigel::CMDLINE_DUMP_SHARING_STATS)
      {
	int tid = instr->get_global_thread_id();
        int t = rigel::event::global_tracker.GetCurrentTaskNum(tid);
        uint32_t pc = instr->get_currPC();
        #ifdef DEBUG_MEMTRACKER
        fprintf(stdout,"WRITE: core %d tid %d task %d addr 0x%08x pc 0x%08x\n",core,tid,t,ea,pc);
        #endif
        ProfileStat::MemWrtTrack( ea, pc, t );
      }

      if (is_stalled(pipe)) { unstall(pipe); }

      if (instr->get_is_vec_op()) {
        uint32_t mv0, mv1, mv2, mv3;
        mv0 = instr->get_sreg0().data.i32;
        mv1 = instr->get_sreg1().data.i32;
        mv2 = instr->get_sreg2().data.i32;
        mv3 = instr->get_sreg3().data.i32;
        uint32_t addr0, addr1, addr2, addr3;
        if (((ea) & (0xF)) != 0x0) {
          DEBUG_HEADER();
          fprintf(stderr, "Unaligned vector store addr: 0x%08x\n", ea);
          assert(0);
        }
        addr0 = ea + 0x0;
        addr1 = ea + 0x4;
        addr2 = ea + 0x8;
        addr3 = ea + 0xc;
        // addr0 already is a hit so the rest must be
        core->get_cluster()->getCacheModel()->backing_store.write_word(addr0, mv0);
        core->get_cluster()->getCacheModel()->backing_store.write_word(addr1, mv1);
        core->get_cluster()->getCacheModel()->backing_store.write_word(addr2, mv2);
        core->get_cluster()->getCacheModel()->backing_store.write_word(addr3, mv3);
      }

      instr->setMemAccess();

      // Store actually completed
      Profile::accum_cache_stats.store_accesses++;
      goto done_memaccess;
    }
    // XXX: Stalling memory operations that do not return values go here.
    case (I_LINE_WB):
    case (I_LINE_FLUSH):
    {
      // Short circuit for finished stores
      if (instr->doneMemAccess()) { goto done_memaccess; }

      bool stage_stall = false;
      uint32_t ea = instr->get_result_ea();
      if (0 != (ea & 0x03)) {
        DEBUG_HEADER();
        fprintf(stderr, "Unaligned writeback memory access!");
        // Dump stack trace
        {
          uint32_t sp_addr = core->get_regfile(instr->get_core_thread_id())->read(29).data.i32;
          fprintf(stderr, "--------- STACK TRACE --------\n");
          fprintf(stderr, "STACK PTR: 0x%08x\n", sp_addr);
          for (int i = 0; i < 16; i++) {
            fprintf(stderr, "[0x%08x] ", sp_addr + 32*i);
            for (int j = 0; j < 32; j += 4) {
              uint32_t addr = sp_addr + (32*i) + j;
              uint32_t data = UINT32_MAX;
              data = core->get_cluster()->getCacheModel()->backing_store.read_word(addr);
              fprintf(stderr, "(%3d) %08x ", i*32+j, data);
            }
            fprintf(stderr, "\n");
          }
        }
        // Dump the registerfile state.
        core->get_regfile(instr->get_core_thread_id())->dump();
        // Dump the scoreboard state.
        core->get_scoreboard(instr->get_core_thread_id())->dump();
        // Dump the instruction state.
        instr->dump();
        // TODO: Dump pipeline state.
        // this->dump();
        assert(0);
      }

      // FIXME: Only allow a single load in flight at a time.
      if (core->GetLoadAccessPending(instr->get_core_thread_id())) {
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return instr;
      }

      // used twice below
      CacheAccess_t ca(
        ea,                          // address
        core->get_core_num_local(),  // core number (faked)
        instr->get_local_thread_id(),// thread id at cluster
        IC_MSG_ERROR,                // will fault if later accessed and not set valid
        instr                        // instruction correlated to this access
      );
      //core->get_cluster()->getCacheModel()->writeback_line(*instr, core->get_core_num_local(), ea, stage_stall);
      core->get_cluster()->getCacheModel()->writeback_line( ca, stage_stall );
      if (stage_stall) {
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return rigel::NullInstr;
      }
      // With the directory, the writeback is synonymous with the flush.
      if (!rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
        if (instr->get_type() == I_LINE_FLUSH) {
          // For flushes we want to do a writeback then clear the line from the
          // cache completely with an invalidate.
          // core->get_cluster()->getCacheModel()->invalidate_line(core->get_core_num_local(), ea, stage_stall);
          core->get_cluster()->getCacheModel()->invalidate_line( ca, stage_stall);
        }
      }


      if (is_stalled(pipe)) { unstall(pipe); }

      // Atomic access actually completed
      instr->setMemAccess();
      goto done_memaccess;
    }

    case (I_ATOMINC):
    case (I_ATOMDEC):
    case (I_ATOMXCHG):
    case (I_ATOMCAS):
    case (I_ATOMADDU):
    case (I_ATOMMAX):
    case (I_ATOMMIN):
    case (I_ATOMOR):
    case (I_ATOMAND):
    case (I_ATOMXOR):
    case (I_GLDW):
    case (I_GSTW):
    {
      // Short circuit for finished stores
      if (instr->doneMemAccess()) { goto done_memaccess; }

      // For GSTW and ATOM.XCHG
      uint32_t memval = instr->get_result_reg().data.i32;
      bool stage_stall = false;
      uint32_t ea = instr->get_result_ea();
      if((ea & 0x03) != 0)
      {
        DEBUG_HEADER();
        fprintf(stderr, "[MC] Unaligned global! "
                        "PC: 0x%08x addr: 0x%08x core: %d \n",
                        instr->get_currPC(), ea, core->get_core_num_global());
        // Dump stack trace
        {
          uint32_t sp_addr = core->get_regfile(instr->get_core_thread_id())->read(29).data.i32;
          fprintf(stderr, "--------- STACK TRACE --------\n");
          fprintf(stderr, "STACK PTR: 0x%08x\n", sp_addr);
          for (int i = 0; i < 16; i++) {
            fprintf(stderr, "[0x%08x] ", sp_addr + 32*i);
            for (int j = 0; j < 32; j += 4) {
              uint32_t addr = sp_addr + (32*i) + j;
              uint32_t data = UINT32_MAX;
              data = core->get_cluster()->getCacheModel()->backing_store.read_word(addr);
              fprintf(stderr, "(%3d) %08x ", i*32+j, data);
            }
            fprintf(stderr, "\n");
          }
        }
        // Dump the registerfile state.
        core->get_regfile(instr->get_core_thread_id())->dump();
        // Dump the scoreboard state.
        core->get_scoreboard(instr->get_core_thread_id())->dump();
        // Dump the instruction state.
        instr->dump();
        // TODO: Dump pipeline state.
        // this->dump();
        fprintf(stderr,"cycle %" PRIu64 "\n", rigel::CURR_CYCLE);
        assert(0 && "Unaligned global access");
      }
//DEBUG_HEADER();
//fprintf(stderr, "ATOMIC OP ea: 0x%08x memval: 0x%08x [[BEFORE ACCESS]]\n", ea, memval);

      // FIXME: Only allow a single load in flight at a time.
      if (core->GetLoadAccessPending(instr->get_core_thread_id())) {
        Profile::accum_cache_stats.atomic_stall_total++;
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return instr;
      }

      CacheAccess_t ca(
        ea,                          // address
        core->get_core_num_local(),  // core number (faked)
        instr->get_local_thread_id(),// thread id
        IC_MSG_NULL,                 // no interconnect message type,
        instr  // instruction correlated to this access
      );
      // FIXME: This will be done using a global cache message in the future
      MemoryAccessStallReason masr = core->get_cluster()->getCacheModel()->global_memory_access(
          ca,
          memval,
          stage_stall,
          CoreInOrderLegacy::ClusterNum( core->get_core_num_global() )
      );

      switch(masr)
  {
    case NoStall:
      break;
    case L2DPending:
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
    case StuckBehindGlobal:
      instr->stats.cycles.stuck_behind_global++;
      break;
    case L2DAccessBitInterlock:
      instr->stats.cycles.l2d_access_bit_interlock++;
      break;
    default:
      printf("masr: %d\n", masr);
      assert(0 && "Invalid MemoryAccessStallReason");
      break;
  }

      #ifdef DEBUG
      cerr << " [[ATOMIC/GLOBAL ACCESS]] EA: " << HEX_COUT << ea;
      cerr << " V: " << HEX_COUT << memval;
      cerr << " stall: " << boolalpha << stall << endl;
      #endif
//DEBUG_HEADER();
//fprintf(stderr, "ATOMIC OP ea: 0x%08x memval: 0x%08x [[AFTER ACCESS]]\n", ea, memval);

      if (stage_stall) {
        Profile::accum_cache_stats.atomic_stall_total++;
        stall(pipe);
        core->get_cluster()->getProfiler()->timing_stats.mc_stall++;
        return rigel::NullInstr;
      }
      if (is_stalled(pipe)) { unstall(pipe); }

      instr->set_result_RF(
        memval,
        instr->get_dreg().addr,
        ea
      );

      //////////////////////////////////////////////////////////////////////////
      // global and atomic hooking for sharing study
      if(rigel::CMDLINE_DUMP_SHARING_STATS) {
	int tid = instr->get_global_thread_id();
        int t = rigel::event::global_tracker.GetCurrentTaskNum(tid);
        int pc = instr->get_currPC();
        // addr = ea

        switch (instr_type) {

          case (I_ATOMINC):
          case (I_ATOMDEC):
          case (I_ATOMXCHG):
          case (I_ATOMCAS):
          case (I_ATOMADDU):
          case (I_ATOMMAX):
          case (I_ATOMMIN):
          case (I_ATOMOR):
          case (I_ATOMAND):
          case (I_ATOMXOR):
            // FIXME: counting these as Writes for now, but should track as a seperate
            // class of traffic?
            #ifdef DEBUG_MEMTRACKER
            fprintf(stdout,"WRITE/ATOMIC: core %d task %d addr 0x%08x pc 0x%\n",core_id,t,ea,pc);
            #endif
            ProfileStat::MemWrtTrack( ea, pc, t, false, true );
            break;

          case (I_GSTW):
            #ifdef DEBUG_MEMTRACKER
            fprintf(stdout,"WRITE(GLOBAL): core %d task %d addr 0x%08x pc 0x%\n",core_id,t,ea,pc);
            #endif
            ProfileStat::MemWrtTrack( ea, pc, t, true, false );
            break;

          case (I_GLDW):
            #ifdef DEBUG_MEMTRACKER
            fprintf(stdout,"READ(GLOBAL): core %d task %d addr 0x%08x \n",core_id,t,ea,pc);
            #endif
            ProfileStat::MemRdTrack( ea, pc, t , true, false );
            break;

          default:
            assert(0 && "unknown access type to track!");
        }

      }
      //////////////////////////////////////////////////////////////////////////

      // Need to clear reg and allow for regular bypass.  GSTW does not lock.
      if (instr->get_type() != I_GSTW) {
        core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg().addr, instr->get_result_reg().data.i32);
        core->get_scoreboard(instr->get_core_thread_id())->UpgradeMemAccess(instr->get_result_reg().addr);
      }
      // Atomic access actually completed
      Profile::accum_cache_stats.atomic_accesses++;
      instr->setMemAccess();
      goto done_memaccess;
    }
    default:
    {
      /* PASSTHROUGH */
      goto done_memaccess;
    }
  }

done_memaccess:

  core->get_cluster()->getProfiler()->timing_stats.mc_active++;
  return instr;
}

