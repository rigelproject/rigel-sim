
////////////////////////////////////////////////////////////////////////////////
// core_inorder_legacy.h
////////////////////////////////////////////////////////////////////////////////
// CoreInOrderLegacy class declarations
////////////////////////////////////////////////////////////////////////////////

#ifndef _CORE_INORDER_LEGACY_H_
#define _CORE_INORDER_LEGACY_H_

#include <list>
#include <fstream>

#include "sim.h"
#include "core_base.h"
#include "util/syscall.h"
#include "profile/profile.h"
#include "btb.h"

class UserInterfaceLegacy;
class ScoreBoard;
class ScoreBoardBypass;
class BranchPredictorBase;
class FetchStage;
class DecodeStage;
class ExecuteStage;
class MemoryStage;
class FPCompleteStage;
class CCacheAccessStage;
class WriteBackStage;
class StageBase;
class RegisterFileLegacy;
class SpRegisterFileLegacy;
class LocalityTracker;
class TLB;

// Stages
// FIXME: move this enum somewhere global
enum {
  FETCH_STAGE,
  DECODE_STAGE,
  EXECUTE_STAGE,
  MEM_ACCESS_STAGE,
  FP_COMPLETE_STAGE,
  CACHE_ACCESS_STAGE,
  WRITE_BACK_STAGE,
  NUM_STAGES
};
typedef enum {
  IF2DC,
  DC2EX,
  EX2MC,
  MC2FP,
  FP2CC,
  CC2WB,
  WB,
  NUM_STAGE_LATCHES
} latch_name_t;

////////////////////////////////////////////////////////////////////////////////
// struct fetch_packet_t
////////////////////////////////////////////////////////////////////////////////
// TODO: put these with fetch? (new stages headers?)
// Structure used to hold values from in-order fetch while an ICache miss is
// serviced since we cannot hit the branch predictor twice and not expect to
// screw things up
////////////////////////////////////////////////////////////////////////////////
struct FetchPacket {

  // member data
  uint32_t raw_instr; // the raw 32-bit encoded instruction word
  bool valid;         // is the fetch packet valid
  uint32_t gHist;     // 
  uint32_t next_pc;   // 
  uint32_t pred_pc;   // 

  // constructor
  FetchPacket() { clear(); }

  // clear fetch packet
  void clear() {
    gHist = 0;
    next_pc = 0;
    pred_pc = 0;
    raw_instr = 0;
    valid = false;
  }

};
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// struct pipeline_per_thread_state_t
////////////////////////////////////////////////////////////////////////////////
//encapsulate pipeline stage state shared ACROSS multiple pipeline stages
//data here is kept PER-THREAD (see other struct for PER-CORE state)
//data here is not "owned" by a particular pipeline stage
////////////////////////////////////////////////////////////////////////////////
struct PipelineThreadState {
  FetchPacket last_fetch;    	   // not sure if this is used anywhere
  uint32_t last_pc;                // 
  bool     _halt;                  // Set on first occurance of HLT
  bool     pending_load_access;    // True if load has passed MEM and missed in line buffer
  bool     last_pending_addr;      // Address of the last pending load access
  int      watchdog;               // 
  bool     vector_op_pending;      // 
  bool     nonblocking_atomics_enabled;
  struct {
    uint64_t predict_pc;          // 
    bool     restore_unaligned;   //
    uint32_t restore_pc;          // 
  } MT;

  ///////////////////////////////////////////
  // Inter-Stage Signals/Regs
  ///////////////////////////////////////////
  // Used for sending PC fixup information up to fetch from execute

  // EX2IF
  struct _EX2IF{
    _EX2IF() : FixupPC(0), FixupNeeded(0), MispredictPC(0) {}
    uint32_t FixupPC;
    bool     FixupNeeded;
    uint32_t MispredictPC; // Address of mispredicted branch from execute.
  } EX2IF;
  // IF2IF
  struct _IF2IF{
    _IF2IF() : NextPC(0), UnalignedPending(false), BTRHitPending(false),
               profiler_l1i_stall_pending(false), profiler_l2i_stall_pending(false)
    { }
    uint32_t NextPC;
    bool     UnalignedPending; // Set when a mispredicted branch fixup stalls to an unaligned address
    bool     BTRHitPending;    // Set when a BTR hit occured in a cycle.  Initially just for debug purposes.
    // profiler, FIXME, put this somewhere else
    // Profiler hooks to get IF misses correct.  Set on stall, unset on success.
    bool profiler_l1i_stall_pending;
    bool profiler_l2i_stall_pending;
  } IF2IF;
  ///////////////////////////////////////////
  // SpecInstr
  // List of currently speculative instrs
  struct {
    int ListLength; // FIXME: was const, too hard to initialize, should be const
    InstrSlot List[3*rigel::ISSUE_WIDTH]; // Three stages of speculation
    int       ListHead;
  } SpecInstr;

  // Set pending_sync when a SYNC hits execute.  The syn_instr_num must retire
  // before the SYNC can continue.  SYNC also triggers a mispredict to ensure
  // everything is clean after it leaves execute.  REQ set at execute by the
  // SYNC, ACK set at writeback by the previous instruction.  ACK cleared by
  // SYNC at execute.
  bool pending_sync_req;
  bool pending_sync_ack;
  // Each thread maintains a separate BTB for now.
  BranchTargetBufferBase *btb;

  // for pipeline front-end arbitration thread switching
  // request for issue a new fetch, true when the thread is ready, false when handling an L1I miss
  bool req_to_fetch;
  // active when this thread has been selected for fetch but not yet able due to pipe stall/blocked
  // this prevents the thread_switch logic from switching, once a thread has been selected it must
  // perform a fetch to prevent starvation live-lock
  bool waiting_to_fetch;

};
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// struct pipeline_per_core_state_t
////////////////////////////////////////////////////////////////////////////////
// encapsulate per-core shared state that is NOT per-thread unique
// data here is not "owned" by a particular pipeline stage
typedef struct {

  // The complement of ALU operations this bad boy has:
  int funit_available_count[FU_COUNT];

  // table for holding thread scheduling info for the core
  // FIXME: this would ideally be a class and initialize itself (TODO)
  int* schedule_table;

  // network priority
  net_prio_t net_priority;

} pipeline_per_core_state_t;
////////////////////////////////////////////////////////////////////////////////

class ClusterLegacy;
namespace rigel {
    class ConstructionPayload;
}

////////////////////////////////////////////////////////////////////////////////
// CLASS NAME: CoreInOrderLegacy
//
////////////////////////////////////////////////////////////////////////////////
class CoreInOrderLegacy : public CoreBase {

  ////////////////////////////////////////////////////////////////////////////
  // Public Methods
  ////////////////////////////////////////////////////////////////////////////
  public:


    /// default Constructor -- TODO, is this used?
    //CoreInOrderLegacy();

    /// standard constructor
    CoreInOrderLegacy(rigel::ConstructionPayload cp, ClusterLegacy *_cluster);

    /// destructor
    ~CoreInOrderLegacy();

    /// Dump
    void Dump();

    /// Heartbeat
    void Heartbeat();

    /// EndSim
    void EndSim();

    InstrSlot regfile_read(InstrSlot instr, int pipe);

    int  PerCycle();           // step the core through one cycle of execution
    void UpdateLatches();  // update the latches at the end of a cycle

    ////////////////////////////////////////////////////
    // thread accessors
    ////////////////////////////////////////////////////
    void set_mt_restore_pc(uint32_t pc, int tid);
    //uint32_t get_mt_restore_pc() {
    //  return PipeThreadState[fetch_thread_id].mt_restore_pc;
    //}
    void thread_switch(); // initiate a thread switch
    void thread_flush( int tid );
    void thread_restore(int tid);

    int get_fetch_thread_id_core() const {
      return fetch_thread_id;
    }
    int get_fetch_thread_id_cluster() const { // cluster thread ID
      return fetch_thread_id + ( get_core_num_local() * rigel::THREADS_PER_CORE );
    }
    int get_fetch_thread_id_global() const { // chip thread ID
      return ( (core_num * rigel::THREADS_PER_CORE) + fetch_thread_id );
    }

    // pick the next thread to schedule
    int schedule_next_thread();

    // accessors for Core and Thread level state structures
    PipelineThreadState * get_thread_state(int tid) {
      assert((tid < rigel::THREADS_PER_CORE && tid >= 0) && "Out of bounds tid");
      return &PipeThreadState[tid];
    }

    // ONLY USE THIS (get_fetch_thread_state) FROM FETCH!!
    PipelineThreadState * get_fetch_thread_state() {
      return &PipeThreadState[fetch_thread_id];
    }
    pipeline_per_core_state_t * get_core_state() {
      return &PipeCoreState;
    }

    // flush on branch mispredict (and a few other things)
    void flush( int tid);

    virtual void save_state() const;
    virtual void restore_state();

    ////////////////////////////////////////////////////
    // ACCESSORS
    ////////////////////////////////////////////////////
    // pointer accessors for class hierarchy/interaction
    ClusterLegacy *get_cluster() {
      return cluster;
    }
    // Register File accessors
    //RegisterFileLegacy * get_regfile() const { return regfiles[fetch_thread_id]; }
    RegisterFileLegacy * get_regfile(int tid) const;
    SpRegisterFileLegacy * get_sprf(int tid) const;
    RegisterFileLegacy * get_accumulator_regfile(int tid) const;
    // prints interactive display for user interface
    void PrintCoreDisplay(UserInterfaceLegacy*);

    ////////////////////////////////////////////////////
    // core/cluster ID accessors
    ////////////////////////////////////////////////////
    // cluster-level core ID
    int get_core_num_local() const {
      return (core_num % rigel::CORES_PER_CLUSTER);
    }
    static int ClusterNum(int c) {
      return (c / rigel::CORES_PER_CLUSTER);
    }
    static int LocalCoreNum(int c) {
      return (c % rigel::CORES_PER_CLUSTER);
    }
    int get_core_num_global() const {
      return core_num;
    }

    bool is_thread_halted(int tid) const {
      return PipeThreadState[tid]._halt;
    }

    bool is_halted() const {
      for(int i=0; i < rigel::THREADS_PER_CORE; ++i) {
        if(!PipeThreadState[i]._halt) {
          return false;
        }
      }
      return true;
    }

    void halt(int tid) {
      PipeThreadState[tid]._halt = true;
    }

    bool is_fetch_stalled() const;

    uint32_t GetCurPC(int tid = -1) const {
      if(tid == -1)
        tid = fetch_thread_id;
      return  PipeThreadState[fetch_thread_id].last_pc;
    }

    void set_last_pc( uint32_t p, int tid ) {
      PipeThreadState[tid].last_pc = p;
    }

    ///// ???
    bool GetLoadAccessPending(int tid) const {
      // If the last address is the same, we are waiting on the same address so
      // we let it go forward
    //  if (last_pending_addr == ea) return false;
      return PipeThreadState[tid].pending_load_access;
    }
    void SetLoadAccessPending(int tid) {
      assert( PipeThreadState[tid].pending_load_access == false
        && "Trying to perform a load while one is pending!");
       PipeThreadState[tid].pending_load_access = true;
    }
    void ClearLoadAccessPending(int tid) {
      assert( PipeThreadState[tid].pending_load_access == true
        && "Trying to clear load pending while none are pending!");
      PipeThreadState[tid].pending_load_access = false;
    }
    ///// ???

    void decrement_watchdog(int tid) {
      PipeThreadState[tid].watchdog--;
    }

    void force_thread_swap() {
      forceSwap = true;
    }

    int get_watchdog(int tid) const {
      return PipeThreadState[tid].watchdog;
    }

    void set_watchdog(int watchdog, int tid) {
      PipeThreadState[tid].watchdog = watchdog;
    }

    void set_vector_op_pending(bool vector_op_pending, int tid) {
      PipeThreadState[tid].vector_op_pending = vector_op_pending;
    }

    bool get_vector_op_pending(int tid) const {
      return PipeThreadState[tid].vector_op_pending;
    }

    net_prio_t get_net_priority() const {
      return PipeCoreState.net_priority; }

    ScoreBoard * get_scoreboard(int tid) const {
      return scoreboards[tid];
    }

    ScoreBoard * get_sp_scoreboard(int tid) const {
      return sp_sbs[tid];
    }

    ScoreBoard * get_ac_scoreboard(int tid) const {
      return ac_sbs[tid];
    }

    BranchPredictorBase * get_bp() const {
      assert(0 && "bp code deprecated, check code paths for proper operation");
      return bp;
    }

    // speculative instrs
    void AddSpecInstr(InstrSlot instr);
    void RemoveSpecInstr(InstrSlot instr);
    void FlushSpecInstrList(int tid);
    void FlushInstr(InstrSlot instr);

    // accessors (comment me)
    void set_last_fetch_instr_num(uint64_t num) {
      last_fetch_instr_num = num;
    }
    void set_last_commit_cycle(uint64_t cycle) {
      last_commit_cycle = cycle;
    }
    uint64_t get_last_fetch_instr_num() const {
      return last_fetch_instr_num;
    }
    uint64_t get_last_commit_cycle() const {
      return last_commit_cycle;
    }

    // Methods for accessing network priority state.  The higher the number, the
    // higher the priority of the flow.  The piority takes effect at EX.
    void set_net_priority(net_prio_t level) { PipeCoreState.net_priority = level; }

    //Dump locality information for all threads and aggregate access stream
    void dump_locality() const;

    //Dump TLB information for all threads and aggregate access stream
    void dump_tlb() const;

    // end Public Methods
    ////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////
  // Public Data Members
  ////////////////////////////////////////////////////////////////////////////
  public:

    ///////////////////////////////////////////
    /* PIPELINE STATE */
    ///////////////////////////////////////////
    PipelineThreadState *PipeThreadState;
    pipeline_per_core_state_t    PipeCoreState;

    ///////////////////////////////////////////
    /* INTER STAGE LATCHES */
    ///////////////////////////////////////////
    InstrSlot latches[NUM_STAGES][rigel::ISSUE_WIDTH];
    InstrSlot nlatches[NUM_STAGES][rigel::ISSUE_WIDTH];
    ///////////////////////////////////////////

    static const InstrSlot NullInstr;

    //ScoreBoard *sb; // deprecated

		std::ofstream *reg_trace_file;  // Array of output files for register trace.  One per thread per core
    //temp value to count how many cycles fetch has stalled, for per-instr profiling.
    int fetch_stall_counter[rigel::ISSUE_WIDTH];
    //need to keep track of which PC you're trying to fetch per cycle.
    uint32_t fetch_stall_pc[rigel::ISSUE_WIDTH];

    StageBase * stages[NUM_STAGES];

    //////////////////////////////////////////
    // Core States
    FetchStage        *fetch_stage;
    DecodeStage       *decode_stage;
    ExecuteStage      *execute_stage;
    MemoryStage       *mem_stage;
    FPCompleteStage   *fpaccess_stage;
    CCacheAccessStage *ccache_stage;
    WriteBackStage    *writeback_stage;
    //////////////////////////////////////////

    // FIXME: why is this a class?
    // (see syscall.h and syscalls.cpp for more details)
    Syscall *system_calls;

    LocalityTracker **perThreadLocalityTracker;
    LocalityTracker *aggregateLocalityTracker;

    size_t num_tlbs;
    TLB ***perThreadITLB;
    TLB **aggregateITLB;
    TLB ***perThreadDTLB;
    TLB **aggregateDTLB;
    TLB ***perThreadUTLB;
    TLB **aggregateUTLB;

    bool forceSwap;
    std::list<InstrSlot> memoryOps;
  ////////////////////////////////////////////////////////////////////////////
  // Protected Data Members
  ////////////////////////////////////////////////////////////////////////////
  protected:

    /// globally unique core number
    int core_num;

    /// pointer to containing cluster for this core
    ClusterLegacy *cluster;

    // Register Files
    RegisterFileLegacy   **regfiles; // hold one RF per thread
    SpRegisterFileLegacy **sprfs;
    RegisterFileLegacy **accfiles; // Stores the accumulator

    // Simulator internal last instruction number.  Monotonically increasing.
    // Property is checked at writeback.
    uint64_t last_fetch_instr_num;
    // Cycle of last committed instruction
    uint64_t last_commit_cycle;

  ////////////////////////////////////////////////////////////////////////////
  // Private Methods
  ////////////////////////////////////////////////////////////////////////////
  // member functions that get or set class data
  private:

    void print_stage(const char *stage_name, latch_name_t);

  // end Private Methods
  ////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////
  // Private Data
  ////////////////////////////////////////////////////////////////////////////
  private:

    // scoreboards
    ScoreBoard **scoreboards;
    ScoreBoard **sp_sbs;
    ScoreBoard **ac_sbs;
    // branch predictor
    BranchPredictorBase *bp;

    // BEGIN MultiThreading
    int fetch_thread_id; // current thread for MT in FETCH (always 0 for single threaded)

  // end Private Data
  ////////////////////////////////////////////////////////////////////////////

};
#endif


