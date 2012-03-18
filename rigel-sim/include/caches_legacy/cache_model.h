////////////////////////////////////////////////////////////////////////////////
// cache_model.h
////////////////////////////////////////////////////////////////////////////////
//
//  Parameters for the various levels of the cache.
//  Used by cluster_cache.cpp, core_caches.cpp, global_cache.cpp, and others
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _CACHE_MODEL_H_
#define _CACHE_MODEL_H_
#include "sim.h"
#include "profile/profile.h"
#include "arbiter.h"

// Includes CacheTags and MSHRs
#include "caches_legacy/cache_tags.h"
#include "mshr.h"
// Each of the caches is kept in a separate file, but they are all interrelated
// so we pull them in here.

// The includes below from includes/caches/ contain *NO* include files of their
// own.
using namespace rigel::cache;
#include "caches_legacy/cache_base.h"
#include "caches_legacy/l1d.h"
#include "caches_legacy/l1i.h"
#include "caches_legacy/l2i.h"
#include "caches_legacy/l2d.h"
#include "caches_legacy/global_cache.h"

// Forward declarations
class BroadcastManager;

namespace rigel {
  class ConstructionPayload;

  namespace cache {
    struct ldlstc_req_t {
      ldlstc_req_t() : addr(0), size(0), valid(false) {}
      // Address of the request to track.
      uint32_t addr;
      // The size in bytes of the region to track (to allow for configurable
      // LL/SC block sizes).
      int size;
      // Is the request valid?  We use an array of these so the valid bit is needed.
      bool valid;
    };
  }
}

/// CacheModel
/// 
/// Wrapper class for holding all of the caches that are associated with a
/// cluster.
class CacheModel 
{
  public: 
    // Default constructor.  Not implemented.
    CacheModel();
    // Useful constructor.
    CacheModel( rigel::ConstructionPayload cp,
                Profile *profile, ClusterLegacy *cl);
    // Destructor.  
    ~CacheModel() { 
      delete[] LoadLinkTable; 
      delete[] L1D;
      delete[] NoL1D;
      delete[] memory_retry_watchdog_count; 
      delete[] memory_retry_watchdog_count_instr;
    }
    // Called once at the start of each cycle
    void PerCycle();

    // arb early for port before reaching mem stage
    void ArbEarly(uint32_t addr, uint32_t core, int tid, RES_TYPE res_type );

    // XXX: Reads and writes are continually reexecuted until 
    //
    // The l{1,2}{i,2} boolean references are used for counting hits/misses in
    // the caches for profiling.  They are set by every access but the
    // instruction latches only the first value returned (it is idempodent).
    //
    // Return data from memory at 'addr' into 'data'.  'stall' is false when
    // value is available.
    MemoryAccessStallReason read_access(InstrLegacy &instr, int core, uint32_t addr, uint32_t &data, 
            bool &stall, bool &l3d_hit, bool &l2d_hit, bool &l1d_hit, int cluster, int tid);
    // Read an instruction for fetch.
    MemoryAccessStallReason read_access_instr(int core, uint32_t addr, uint32_t &data, 
            bool &stall, bool &l2i_hit, bool &l1i_hit, int tid);
    // Write 'data' to memory.  If store buffer is full (trivially true if the
    // buffer is sized at zero entries), returns true in 'stall'
    MemoryAccessStallReason write_access(InstrLegacy &instr, int core, uint32_t
      addr, const uint32_t &data, bool &stall, bool &l3d_hit, bool &l2d_hit, 
      bool &l1d_hit, int cluster, int tid);
    // prefetch_line() checks the CCache for a hit, if so it stops.  On a miss,
    // prefetch_line() sends a message to the G$ to get the data.  
    //
    // RETURN: true when MSHR allocated, false if not (stall core).
    bool prefetch_line(InstrLegacy &instr, int core, uint32_t addr, int tid);
    // Insert a memory barrier.  Block all requests from entering the cluster
    // cache controller and wait for writeback count to drop to zero and
    // pendingMisses to go to zero before releasing.
    //
    // RETURN: true when stalled, false when complete
    bool memory_barrier(InstrLegacy &instr, int local_core_id);

    // Inject a broadcast invalidate/updaet into the network.  Stalls until it
    // is acked by the global cache.  Updates are easy: We just update the value
    // when we get the ack.  Invalidates require sending messages to all of the
    // other caches.
    //
    // RETURN: true if stalled, false if completed (no stall)
    bool broadcast( const CacheAccess_t ca, uint32_t &data); 
    //bool broadcast(InstrLegacy &instr, int local_core_id, uint32_t addr, uint32_t &data,
    //        net_prio_t net_priority, int local_tid); 

    // Notify L2 of a load-linked request.  Takes size argument for defining how
    // large of a region to track for LL request (in bytes)
    void load_link_request(InstrLegacy &instr, int tid, uint32_t addr, int size);
    // Check if a previous load-linked request to the same range of addresses
    // succeeded.  Return status set in 'success'
    void store_conditional_check(InstrLegacy &instr, int tid, uint32_t addr, bool &success);
    // Nullify any pending LL requests if there is a hit for the cluster
    void ldlstc_probe(int tid, uint32_t addr);
    // Handle Global Memory accesses.  These block and do not enter the cluster cache.
    // The increment/exchanges happen at the GCache.  The L2 tracks what is
    // outstanding in GlobalMemOpOutstanding by setting it on insert and
    // clearing it on removal.  GlobalMemOpAck is set by the core
    //MemoryAccessStallReason global_memory_access(InstrLegacy &instr, int core, uint32_t addr, 
    //    uint32_t &data, bool &stall, int cluster, net_prio_t net_priority, int tid);
    MemoryAccessStallReason global_memory_access(
      const CacheAccess_t ca_in, uint32_t &data, bool &stall, int cluster);

    // Push all valid entries back into memory
    void writeback_all();
    // Invalidate all entries.  Entries are not written back to cache
    void invalidate_all();
    // Invalidate all local instruction caches and the L2I.  If the cluster
    // cache backs the L1I's, then all caches will be invalidated at the
    // cluster.
    void invalidate_instr_all();
    // Write a single line back to memory if it is valid.   This operation may
    // stall.  If the line is not valid, the flush does not occur.  FIXME: What
    // to do if there is a pending request for this line?  The instr is set for
    // use when the writeback is the result of an explicit instruction.
    // Otherwise, instr is just NULL_INSTR
    //void writeback_line(InstrLegacy &instr, int core, uint32_t addr, bool &stall);
    void writeback_line( const CacheAccess_t ca_in, bool &stall);

    // Invalidate a single line without writing it back.  'valid' is set if the
    // line was valid in the cache before invalidating.  With coherence it may
    // stall.
    //void invalidate_line(int core, int tid, uint32_t addr, bool & stall);
    void invalidate_line( const CacheAccess_t ca, bool & stall);
    // Remove line from all caches in the cluster.
    void EvictDown(uint32_t addr);
    // Get the L1 D$ ID this thread maps to
    unsigned int GetL1DID(int tid);
    
    bool helper_arbitrate_L2D_instr(uint32_t addr, int core, int tid, RES_TYPE type);

    ClusterLegacy* GetCluster() { return cluster; }

    void save_to_protobuf(rigelsim::ClusterState *cs) const;
    void restore_from_protobuf(rigelsim::ClusterState *cs);

  public: /* Data */
    // Maintain a per-core/per cluster table of pending
    // load-link/store-conditional requests.
    // MT Change:  maintain a per-thread/per cluster table of pending ldl/stc
    // requests
    ldlstc_req_t *LoadLinkTable;
    // Memory model for the entire chip.
    rigel::GlobalBackingStoreType &backing_store;
    L2Cache L2;
    Profile *profiler;
    // Broadcast manager for handling BCAST_* instructions since they are not
    // instantaneously available to all caches.
    BroadcastManager *broadcast_manager;
    // Private cluster number.  This number is unique w.r.t. other CacheModel's.
    int cluster_num;
    // Arbiter types are defined in rigel.h.
    rigel::ArbL2Type ArbL2; 
    rigel::ArbL2IType ArbL2I; 
    // Return path interconnect for bus-based clusters.
    rigel::ArbL2Type ArbL2Return; 

    // Arrays for core-level caches.
    L1DCache *L1D; //L1D[rigel::L1DS_PER_CLUSTER];
    L1ICache L1I[rigel::CORES_PER_CLUSTER];
    // Only used if there are no L1 (core-level) data caches.
    NoL1DCache *NoL1D; //NoL1D[rigel::L1DS_PER_CORE * rigel::CORES_PER_CLUSTER];

    // L2 ICache is only used if set at the command line.  Otherwise the cache
    // model will back instruction fetch into the L2D (Cluster Cache).
    L2ICache L2I;


  private: 
    // Helper function for writes with CMDLINE_FREE_WRITES on.  Returns true
    // when call should go to UpdateMemory and false when it should be a write
    // stall.
    MemoryAccessStallReason
    helper_L2D_free_write(InstrLegacy &instr, int core, uint32_t addr,
      net_prio_t net_priority, MemoryAccessStallReason &masr, bool &stall, 
      const uint32_t &data, int tid);
    // Check to see if the current address violates the maximum codw word
    // boundary set by the loader.
    void helper_check_code_highwater_mark(uint32_t addr);
    // Handle arbitration for a read/write request.  Returns true on no stall.
    bool helper_arbitrate_L2D_access(uint32_t addr, int core, int tid, RES_TYPE type);
    bool helper_arbitrate_L2D_return(uint32_t addr, int core, int tid, RES_TYPE type);
    // Handle returns from the write_access method
    MemoryAccessStallReason helper_return_write_stall(bool &stall, MemoryAccessStallReason masr);
    MemoryAccessStallReason helper_return_read_stall(bool &stall, MemoryAccessStallReason masr);
    MemoryAccessStallReason helper_return_instr_stall(bool &stall, MemoryAccessStallReason masr);
    MemoryAccessStallReason helper_return_write_success(bool &stall, 
      MemoryAccessStallReason masr, uint32_t const &data, uint32_t addr, 
      InstrLegacy &instr, int core, int tid);
    MemoryAccessStallReason helper_return_read_success(bool &stall, 
      MemoryAccessStallReason masr, uint32_t &data, uint32_t addr, 
      InstrLegacy &instr, int core, int tid);
    // used to have a instr param too but that is meaningless for IFetch
    // removed it instead of always passing null
    MemoryAccessStallReason helper_return_instr_success(bool &stall, 
      MemoryAccessStallReason masr, uint32_t &data, uint32_t addr, 
      int core, int tid);
    // Retry count watchdog for memory operations.  The count is reset when a
    // memory operation completes and incremented on every attempt.  If the
    // count reaches a predefined maximum value, the simulator aborts.
    int *memory_retry_watchdog_count; //[rigel::L1DS_PER_CLUSTER];
    int *memory_retry_watchdog_count_instr; //[rigel::L1DS_PER_CLUSTER];
    // Bitvector of threads that have won arbitration (thus signaled the CCache)
    // but have yet to have the value returned to them.  It is set after winning
    // arbitration and cleared once the access succeeds. The goal is to require
    // only one L2 arbitration per access.
    bool *thread_arb_complete;
    // Keep a pointer to the cluster associated with this cache model around.
    ClusterLegacy *cluster;
}; 

inline MemoryAccessStallReason 
CacheModel::helper_return_write_stall(bool &stall, MemoryAccessStallReason masr) 
{ 
  stall = true; 
  return masr; 
}

inline MemoryAccessStallReason 
CacheModel::helper_return_read_stall(bool &stall, MemoryAccessStallReason masr) 
{ 
  stall = true; 
  return masr; 
}

inline MemoryAccessStallReason 
CacheModel::helper_return_instr_stall(bool &stall, MemoryAccessStallReason masr) 
{ 
  stall = true; 
  return masr; 
}

inline MemoryAccessStallReason 
CacheModel::helper_return_instr_success(bool &stall,
    MemoryAccessStallReason masr, uint32_t &data, uint32_t addr, 
    int core, int tid)
{
  // Reset WDT on successful read.
  // We succeeded, set the accessed bit.
  L2.set_accessed_bit_read(addr);
  L1I[core].set_accessed_bit_read(addr);
  memory_retry_watchdog_count_instr[core] = 0;
  // We succeeded, set the accessed bit.

  //DEBUG_READ_ACCESS("Success.");

  stall = false;
  return masr;
}

#endif
