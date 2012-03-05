////////////////////////////////////////////////////////////////////////////////
// cluster.cpp
////////////////////////////////////////////////////////////////////////////////
//
// 'cluster.cpp' Includes the pipeline models for all of the cores at the
// cluster.  It instantiates and iterates over its associated cores.
// It also owns the cluster cache.
//
////////////////////////////////////////////////////////////////////////////////

#include <ansidecl.h>                   // for inline
#include <stddef.h>                     // for NULL
#include <stdint.h>                     // for uint32_t
#include <bitset>                       // for bitset, bitset<>::reference
#include <fstream>
#include <iostream>
#include "cache_model.h"    // for CacheModel, etc
#include "caches/l2d.h"     // for L2Cache, etc
#include "cluster.h"        // for ClusterLegacy, etc
#include "core.h"           // for CoreInOrderLegacy, ::IF2DC, ::CC2WB, etc
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrCycleStats, InstrStats
#include "profile/profile.h"        // for Profile, InstrStat
#include "sim.h"            // for CORES_PER_CLUSTER, etc
#include "util/ui_legacy.h"             // for UserInterfaceLegacy
#include "util/construction_payload.h"

using namespace rigel;

////////////////////////////////////////////////////////////////////////////////
// ClusterLegacy() (constructor)
////////////////////////////////////////////////////////////////////////////////
//  Initialize the cluster model and the pipeline latches.   Recursively call
//  all of the constructors for the cores within the cluster.
//////////////////////////////////////////////////////////////////////////////////
ClusterLegacy::ClusterLegacy(ConstructionPayload cp) :
  ClusterBase(cp.change_name("ClusterLegacy")),
  id_num(cp.component_index),
  core_sleeping(0)
{
  cp.parent = this;
  cp.component_name.clear();

  // Allocate core array for this cluster.
  cores = new CoreInOrderLegacy *[rigel::CORES_PER_CLUSTER];

  // Allocate profiler for the cluster.  There is a global (static) profiler)
  // and a per-cluster profiler.  Data from the cluster profiles is merged on
  // exit.
  //profiler = new Profile(backing_store, id_num);

  // Allocate the cache model for the cluster.  The core- and cluster-level
  // caches are recursively initialized.
  // CacheModel reads the cluster id from the ConstructionPayloa
  caches = new CacheModel(cp, profiler, this);

  caches->L2.PM_set_owning_cluster(this);

  // Allocate the cores for the cluster.  Each core is initialized with a global
  // unique identifier.
  for (int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    cp.core_state = cp.cluster_state->add_cores();
    cp.component_index = id_num * rigel::CORES_PER_CLUSTER + i; // globally unique core ID
    cores[i] = new CoreInOrderLegacy(cp, this);
  }

  // Inititialize the pipeline latches for the cluster.
  // Now done inside the core.

  // Initialize the user interface for handling interactions on stdout/stdin
  // between the user and the simulation.
  ui = new UserInterfaceLegacy(cores, rigel::CMDLINE_INTERACTIVE_MODE);
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ClusterLegacy Destructor
////////////////////////////////////////////////////////////////////////////////
ClusterLegacy::~ClusterLegacy() {
  for (int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    if (cores[i]) { delete cores[i]; cores[i] = NULL; }
  }

  delete [] cores;
  cores = NULL;
  /* Kill system */
  if(caches) { delete caches; caches = NULL; }
  if(ui) { delete ui; ui = NULL; }
  //if(profiler) { delete profiler; profiler = NULL; }

}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SleepCore(int i)
////////////////////////////////////////////////////////////////////////////////
// puts core i (local core number, not global) to sleep.
void ClusterLegacy::SleepCore(int i)
{
  //TODO I'm pretty sure it's OK to call SleepCore() when the core is already asleep,
  // but not positive.
  //if(core_sleeping[i]) //Already asleep!
  //{
  //  printf("Cycle %"PRIu64": Error: Trying to sleep cluster %d core %d, already asleep!\n",
  //    rigel::CURR_CYCLE, GetClusterID(), i);
  //  assert(0 && "Trying to sleep sleeping core!");
  //}
  //printf("Cycle %"PRIu64": Cluster %d: %d, go to sleep!\n", rigel::CURR_CYCLE, GetClusterID(), i);
  core_sleeping.set(i);
}

////////////////////////////////////////////////////////////////////////////////
// WakeCore(int i)
////////////////////////////////////////////////////////////////////////////////
// wakes up core i (local core number, not global)
void ClusterLegacy::WakeCore(int i)
{
  //TODO I'm pretty sure it's OK to call SleepCore() when the core is already asleep,
  // but not positive.
  //if(!core_sleeping[i]) //Already awake!
  //{
  //printf("Cycle %"PRIu64": Error: Trying to wake cluster %d core %d, already awake!\n",
  //  rigel::CURR_CYCLE, GetClusterID(), i);
  //assert(0 && "Trying to wake woken core!");
  //}
  //printf("Cycle %"PRIu64": Cluster %d: %d, wake up!\n", rigel::CURR_CYCLE, GetClusterID(), i);
  core_sleeping.reset(i);
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// halted()
////////////////////////////////////////////////////////////////////////////////
// returns 1 if
int
ClusterLegacy::halted() {
  // Check if all cores are halted.
  int halt = CORES_PER_CLUSTER;
  for (int k = 0; k < CORES_PER_CLUSTER; k++) {
    halt -= cores[k]->is_halted();
  }
  if (halt == 0) { return 1; /* Halts */ }
  return 0;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ClusterLegacy::PerCycle()
////////////////////////////////////////////////////////////////////////////////
// steps through each core in the cluster
// step through each stage of the pipeline and update the latches
//
// returns 1 if halted, 0 if not halted
////////////////////////////////////////////////////////////////////////////////
int
ClusterLegacy::PerCycle() {

//   core_sleeping.flip();
//   if(core_sleeping.none())
//   {
//     printf("Cycle %"PRIu64": Cluster %d: All sleeping!\n", rigel::CURR_CYCLE, GetClusterID());
//     core_sleeping.flip();
//     return 0;
//   }
//   core_sleeping.flip();

  // formerly in Tile
  ClockClusterCache();

  // FIXME: move profiler stuff out of here
  for (int i = 0; i < CORES_PER_CLUSTER; i++) {
    // Add the total number of slots that are halted
    profiler->timing_stats.halt_stall +=  (cores[i]->is_halted() * rigel::ISSUE_WIDTH);
  }

  // early exit if halted
  if (halted()) return 1;

  /* Update per-cycle profile information */
  profiler->PerCycle();

  // step each core in this cluster
  for (int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    // Skip if this core is sleeping
    // TODO: Should we still do profiling in this case, or if not, how do we do fixup when
    // it wakes up?
    if(core_sleeping[i] && rigel::SLEEPY_CORES)
    {
      continue;
    }

    // SIM_SLEEP: Simulator hack allows for fast simulation. Turned on/off by writing
    // to the SPRF.  Write zero, clears it.  Non-zero sets it.
    // SIM_SLEEP turns off all cores but core 0
    if (rigel::SPRF_LOCK_CORES && !(i == 0 && id_num == 0)) {
      break; // exit early for sleeping cores
    }

    // dump the core being traced/watched
    if(cores[i]->get_core_num_global() == rigel::INTERACTIVE_CORENUM) {
      cores[i]->PrintCoreDisplay(ui);
    }

    //TODO: what does this collect, why here?
    ProfileCycles(i);

    // Update for each stage: FETCH, DECODE, EXECUTE, MEM_ACCESS, FP_COMPLETE, CC_ACCESS
    cores[i]->PerCycle();

    // MORE Profiling stuff
    // move this to CC? move to seperate func? leave here?
    //It doesn't matter if these instructions are null or not.
    //We just won't count null instructions at the end.
    for (int j = 0; j < rigel::ISSUE_WIDTH; j++) {
      cores[i]->nlatches[IF2DC][j]->stats.cycles.fetch++;
    }

    if (halted()) { return 1; }

    //TODO: what does this collect, why here?
    ProfileStuckBehind(i);

    // clock the latches to update state
    cores[i]->UpdateLatches();

  }
  return 0; // Not halted
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ProfileCycles()
////////////////////////////////////////////////////////////////////////////////
// helper function for profiling in cluster.step
// FIXME: this may be busted for MT!
inline void
ClusterLegacy::ProfileCycles(int i) {
  //Profiling
  //It doesn't matter if these instructions are null or not.
  //We just won't count null instructions at the end.

  //int tid = cores[i]->get_thread_id_core();

  //
  for (int j = 0; j < rigel::ISSUE_WIDTH; j++) {
    cores[i]->latches[IF2DC][j]->stats.cycles.decode++;
    cores[i]->latches[DC2EX][j]->stats.cycles.execute++;
    cores[i]->latches[EX2MC][j]->stats.cycles.mem++;
    cores[i]->latches[MC2FP][j]->stats.cycles.fp++;
    cores[i]->latches[FP2CC][j]->stats.cycles.cc++;
    cores[i]->latches[CC2WB][j]->stats.cycles.wb++;
  }
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// ProfileStuckBehind()
////////////////////////////////////////////////////////////////////////////////
// helper function for profiling in cluster.step
inline void
ClusterLegacy::ProfileStuckBehind(int i) {
  //For per-instruction profiling
  //At this point, all instructions which themselves stalled have had the appropriate
  //counters incremented.
  //Now, we increment instr->stats.cycles.stuck_behind_other_instr for all instructions
  //which did not stall themselves, //but are stuck behind another instruction which did stall.
  //To accomplish this, we increment the counter for *all* instructions in the core,
  //then decrement them for all the latches we clock forward.
  //Once we short-circuit out of the latch-clocking code, the remaining instructions
  //(earlier in the pipeline) will still have their stuck_behind_other_instr counter incremented.

  // VIM does not like it when you leave these uncommented out since GCC gives a
  // warning.
  //int tid = cores[i]->get_thread_id_core();
  // pointer to core state
  //pipeline_per_thread_state_t *ts = cores[i]->get_thread_state(tid);

  for(int k = 0; k < rigel::ISSUE_WIDTH; k++)
  {
    cores[i]->nlatches[IF2DC][k]->stats.cycles.stuck_behind_other_instr++;
    cores[i]->nlatches[DC2EX][k]->stats.cycles.stuck_behind_other_instr++;
    cores[i]->nlatches[EX2MC][k]->stats.cycles.stuck_behind_other_instr++;
    cores[i]->nlatches[MC2FP][k]->stats.cycles.stuck_behind_other_instr++;
    cores[i]->nlatches[FP2CC][k]->stats.cycles.stuck_behind_other_instr++;
    //For now, these will always be decremented since WB never stalls.  In the future, it might.
    cores[i]->nlatches[CC2WB][k]->stats.cycles.stuck_behind_other_instr++;
  }
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// cluster_sync()
////////////////////////////////////////////////////////////////////////////////
// add core/thread to given barrier
// return 0 failure, 1 success
////////////////////////////////////////////////////////////////////////////////
int ClusterLegacy::cluster_sync(int c){

  // check if a barrier member has still not left yet
  for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
    if( exit_barrier[i] == 1 ) {
      return 0;
    }
  }

  // if we can enter, do it
  enter_barrier[c] = 1;

  return 1;

};
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// get_sync_status()
////////////////////////////////////////////////////////////////////////////////
// returns 0 if still waiting
// returns 1 if barrier is passed
int ClusterLegacy::get_sync_status(int c) {           // returns

  // barrier ready to leave
  if(barrier_status == READY) {
    exit_barrier[c] = 0; // this core is leaving
    return 1;
  }

  // check barrier for completion (all members entered)
  for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
    if( enter_barrier[i] == 0 ) {
      return 0;
    }
  }

  // set barrier as completed now that all cores have entered
  barrier_status =  READY;

  // clear entry bits, set exit bits
  for(int i=0; i<rigel::THREADS_PER_CORE; i++) {
    enter_barrier[i] = 0;
    exit_barrier[i]  = 1;
  }

  exit_barrier[c] = 0; // this core is leaving

  return 1;
};
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// dump_locality()
////////////////////////////////////////////////////////////////////////////////
//dumps C$, per-core, and per-thread locality info
void ClusterLegacy::dump_locality()
{
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++)
    cores[i]->dump_locality();
  caches->L2.dump_locality();
}

std::string ClusterLegacy::getTLBConfigStream(size_t i) const
{
  return cores[0]->aggregateITLB[i]->getConfigString();
}

size_t ClusterLegacy::getNumCoreTLBs() const
{
  return cores[0]->num_tlbs;
}

void ClusterLegacy::get_total_tlb_hits_misses(size_t tlb_num, uint64_t &ptiHits, uint64_t &ptiMisses,
                                        uint64_t &ptdHits, uint64_t &ptdMisses,
                                        uint64_t &ptuHits, uint64_t &ptuMisses,
                                        uint64_t &pciHits, uint64_t &pciMisses,
                                        uint64_t &pcdHits, uint64_t &pcdMisses,
                                        uint64_t &pcuHits, uint64_t &pcuMisses) const
{
  ptiHits = 0; ptiMisses = 0;
  ptdHits = 0; ptdMisses = 0;
  ptuHits = 0; ptuMisses = 0;
  pciHits = 0; pciMisses = 0;
  pcdHits = 0; pcdMisses = 0;
  pcuHits = 0; pcuMisses = 0;
  ptiHits = 0; ptiMisses = 0;
  for(int j = 0; j < rigel::CORES_PER_CLUSTER; j++) {
    CoreInOrderLegacy &c = *(cores[j]);
    if(rigel::THREADS_PER_CORE > 1) {
      for(int k = 0; k < rigel::THREADS_PER_CORE; k++) {
        ptiHits += c.perThreadITLB[k][tlb_num]->getHits();
        ptiMisses += c.perThreadITLB[k][tlb_num]->getMisses();
        ptdHits += c.perThreadDTLB[k][tlb_num]->getHits();
        ptdMisses += c.perThreadDTLB[k][tlb_num]->getMisses();
        ptuHits += c.perThreadUTLB[k][tlb_num]->getHits();
        ptuMisses += c.perThreadUTLB[k][tlb_num]->getMisses();
      }
    }
    pciHits += c.aggregateITLB[tlb_num]->getHits();
    pciMisses += c.aggregateITLB[tlb_num]->getMisses();
    pcdHits += c.aggregateDTLB[tlb_num]->getHits();
    pcdMisses += c.aggregateDTLB[tlb_num]->getMisses();
    pcuHits += c.aggregateUTLB[tlb_num]->getHits();
    pcuMisses += c.aggregateUTLB[tlb_num]->getMisses();
  }
  if(rigel::THREADS_PER_CORE <= 1) {
    ptiHits = pciHits;
    ptiMisses = pciMisses;
    ptdHits = pcdHits;
    ptdMisses = pcdMisses;
    ptuHits = pcuHits;
    ptuMisses = pcuMisses;
  }    
}

////////////////////////////////////////////////////////////////////////////////
// dump_tlb()
////////////////////////////////////////////////////////////////////////////////
//dumps C$, per-core, and per-thread TLB info
void ClusterLegacy::dump_tlb()
{
  //Skip this for now to decrease verbosity
  //for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++)
  //  cores[i]->dump_tlb();
  caches->L2.dump_tlb();
  //Aggregate
  for(size_t i = 0; i < cores[0]->num_tlbs; i++) {
    uint64_t ptiHits, ptdHits, ptuHits, ptiMisses, ptdMisses, ptuMisses;
    uint64_t pciHits, pcdHits, pcuHits, pciMisses, pcdMisses, pcuMisses;
    get_total_tlb_hits_misses(i, ptiHits,ptiMisses,ptdHits,ptdMisses,ptuHits,ptuMisses,
                              pciHits,pciMisses,pcdHits,pcdMisses,pcuHits,pcuMisses);
    const char *cfg = cores[0]->aggregateITLB[i]->getConfigString().c_str();
    if(rigel::THREADS_PER_CORE > 1) {
      fprintf(stderr, "Cluster %d Total Per-thread ITLB %s %"PRIu64" hits, %"PRIu64" misses, %f%% hit rate\n", id_num, cfg, ptiHits, ptiMisses, (((double)ptiHits*100.0f)/(ptiHits+ptiMisses)));
      fprintf(stderr, "Cluster %d Total Per-thread DTLB %s %"PRIu64" hits, %"PRIu64" misses, %f%% hit rate\n", id_num, cfg, ptdHits, ptdMisses, (((double)ptdHits*100.0f)/(ptdHits+ptdMisses)));
      fprintf(stderr, "Cluster %d Total Per-thread UTLB %s %"PRIu64" hits, %"PRIu64" misses, %f%% hit rate\n", id_num, cfg, ptuHits, ptuMisses, (((double)ptuHits*100.0f)/(ptuHits+ptuMisses)));
    }
    fprintf(stderr, "Cluster %d Total Per-core ITLB %s %"PRIu64" hits, %"PRIu64" misses, %f%% hit rate\n", id_num, cfg, pciHits, pciMisses, (((double)pciHits*100.0f)/(pciHits+pciMisses)));
    fprintf(stderr, "Cluster %d Total Per-core DTLB %s %"PRIu64" hits, %"PRIu64" misses, %f%% hit rate\n", id_num, cfg, pcdHits, pcdMisses, (((double)pcdHits*100.0f)/(pcdHits+pcdMisses)));
    fprintf(stderr, "Cluster %d Total Per-core UTLB %s %"PRIu64" hits, %"PRIu64" misses, %f%% hit rate\n", id_num, cfg, pcuHits, pcuMisses, (((double)pcuHits*100.0f)/(pcuHits+pcuMisses)));
  }
}

////////////////////////////////////////////////////////////////////////////////

void 
ClusterLegacy::Dump() {
  assert(0 && "unimplemented");
}

///////////////////////////////////////////////////////////////////////////////
/// Heartbeat
///////////////////////////////////////////////////////////////////////////////
/// Legacy Cluster's heartbeat routine
void
ClusterLegacy::Heartbeat() {

  int c = _cluster_id;
	std::cerr << "Cluster " << c;
  //std::std::cerr << " (#" << (id*rigel::CLUSTERS_PER_TILE) << ": ";
  fprintf(stderr,"\n");

  // p: core
  for (int p = 0; p < rigel::CORES_PER_CLUSTER; p++) {
    std::cerr << "core" << p << ":";
    std::cerr << " 0x" << std::hex << std::setw(8) << std::setfill('0');
    std::cerr << GetCorePC(p) << "(pc)";
    // s: thread
    for (int s = 0; s < rigel::THREADS_PER_CORE; s++){
      // FIXME: implement generic lookup functions instead of nasty math all over
      //int thread = ((p + (c + (id * rigel::CLUSTERS_PER_TILE) * rigel::CORES_PER_CLUSTER))
      //             * rigel::THREADS_PER_CORE) + s;
      int thread = s;
         fprintf(stderr," t%d:(%d)",s,thread);
         fprintf(stderr, " (insnum:%012" PRIx64 ")",
           InstrLegacy::LAST_INSTR_NUMBER[thread]);

      fprintf(stderr, " hlt:%1d",
        GetCore(p).is_thread_halted(s) );
      // FIXME: This is not modular, remake with generic interface
      fprintf(stderr,
        " lastpc:0x%08x", GetCore(p).PipeThreadState[s].last_pc);
    } // end thread
    fprintf(stderr,"\n");
    fprintf(stderr,"Cycle %"PRIu64"\n", rigel::CURR_CYCLE);
  } // end core

}

//////////////////////////////////////////////////////////////////////////////
/// EndSim()
/// called at the end of simulation for any final cleanup
//////////////////////////////////////////////////////////////////////////////
void
ClusterLegacy::EndSim() {
  profiler->end_sim();
  profiler->accumulate_stats();
  if(rigel::TRACK_BIT_PATTERNS) {
    for(int core = 0; core < rigel::CORES_PER_CLUSTER; core++) {
      for(int thread = 0; thread < rigel::THREADS_PER_CORE; thread++) {
        GetCore(core).get_regfile(thread)->zt.report(std::cout);
      }
    }
  }
  //FIXME pull this out
  if(rigel::ENABLE_LOCALITY_TRACKING) {
    dump_locality();
  }
  if (rigel::DUMP_REGISTER_FILE)
  {
    char out_file[100];
		std::ofstream reg_dump_file;
    int cluster_id = id_num; // must be a global ID here
    std::cerr << "Cluster " << cluster_id << ", ";
    for (int core_id = 0; core_id < rigel::CORES_PER_CLUSTER; core_id++) {
      std::cerr << "Core " << core_id << " Register File\n\n";
      // FIXME: don't hardcode to thread 0!
      GetCore(core_id).get_regfile(0)->dump_reg_file(std::cerr);
      for(int thread_id=0; thread_id<rigel::THREADS_PER_CORE; thread_id++) {
        sprintf(out_file,"%s/rf.%d.%d.%d.dump",rigel::DUMPFILE_PATH.c_str(),cluster_id,core_id,thread_id);
        reg_dump_file.open(out_file);
        GetCore(core_id).get_regfile(thread_id)->dump_reg_file(reg_dump_file);
        reg_dump_file.close();
      }
    }
  }
}

// Methods that were removed from cluster.h to break include file dependences.
void 
ClusterLegacy::ClockClusterCache() { 
  caches->PerCycle(); 
}
uint32_t 
ClusterLegacy::GetCorePC(int c) { 
  return cores[c]->GetCurPC(); 
}

void ClusterLegacy::save_state() const {
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++)
    GetCore(i).save_state(); 
}

void ClusterLegacy::restore_state() {
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++)
    GetCore(i).restore_state(); 
}
