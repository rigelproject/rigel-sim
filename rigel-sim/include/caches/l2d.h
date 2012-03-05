////////////////////////////////////////////////////////////////////////////////
// includes/caches/l2d.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the definition of the L2D cache (cluster cache).  This
//  file is included from cache_model.h
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __CACHES__L2D_H__
#define __CACHES__L2D_H__

#include "prefetch.h"
#include "locality_tracker.h"
#include <cstdio>
#include <string>
#include "util/debug.h"
#include "caches/cache_base.h"
#include "sim.h"
#include "tlb.h"

class Profile;
class CacheModel;
class GlobalCache;
class TileInterconnectBase;
class HWPrefetcher;
class ClusterLegacy;

////////////////////////////////////////////////////////////////////////////////
namespace rigel {
  namespace cache {
    typedef struct nonatomic_ccentry {
      uint32_t addr;
      bool valid;
      int queue_num;

      void reset() {
        addr = 0xFFFFFFFF;
        valid = false;
        queue_num = -1;
      }
    } nonatomic_ccentry_t;
    // Each fifo_map_entry has the mapping held in the L2's list of MSHRs in
    // pendingMiss (real_pend_idx) and the index held by the MSHR's themselves
    // which is a virtual index (mshr_pend_idx) 
    struct fifo_map_entry_t { 
      int real_idx; // Index in the pendingMiss array
      int virt_idx; // Index held by the MHSR

      public:
        fifo_map_entry_t() { invalidate(); }

        void invalidate() {
          virt_idx = -1;
          real_idx = -1;
        }
    };
  };
};
////////////////////////////////////////////////////////////////////////////////

using namespace rigel::cache;

////////////////////////////////////////////////////////////////////////////////
// Class: L2Cache
////////////////////////////////////////////////////////////////////////////////
class L2Cache : public CacheBase< rigel::cache::L2D_WAYS, rigel::cache::L2D_SETS, rigel::cache::LINESIZE,
  rigel::cache::MAX_L2D_OUTSTANDING_MISSES, rigel::cache::CACHE_WRITE_THROUGH, rigel::cache::L2D_EVICTION_BUFFER_SIZE
> 
{
  public: /* ACCESSORS */
    // L2 cache constructor.
    L2Cache(CacheModel *caches, TileInterconnectBase *ti, Profile *profile, int clusternum);
    // Allows the next level up to set the return time for the fill (FIXME: no
    // MSHR pointer passing!)
    void Schedule(int size, MissHandlingEntry<rigel::cache::LINESIZE> *MSHR);
    // No default constructor ** Not implemented **
    L2Cache();
    // destructor
    ~L2Cache() {
      delete[] GlobalMemOpOutstanding;
      delete[] GlobalMemOpCompleted;
      delete[] GlobalLoadForCoreAtomicOutstanding;
      delete[] WritebacksPending;      
      delete[] MemoryBarrierPending;   
      delete[] MemoryBarrierCompleted;     
    };
          // L2 needs its own evict to make sure that it removes the line from a lower
    // level cache if one is found
    int evict(uint32_t addr, bool &stall);
    // Debugging support
    void dump_pending_msgs() const;
    void dump_cache_state() const;
    void dump_locality() const;
    void dump_tlb() const;
  private: /* ACCESSORS */    
    // Clock tick for the cache controller.
    void PerCycle();

    // Wait on a fill for a _variable_ latency.  The msg_type field allows for
    // prefetch disambiguation.  The pending index only matters for
    // GCache.pend() because it needs to hold onto it for an L2_REQ -> GC.PEND
    // -> GC.REP -> L2_REP sequence
    //void pend(uint32_t addr, icmsg_type_t msg_type, int core_id,
    //  int ccache_pending_index, bool is_eviction, InstrLegacy *instr, net_prio_t priority, int tid);
    void pend(const CacheAccess_t ca, bool is_eviction);

    // pend() with latency unused by L2Cache.
    void pend(uint32_t addr, int latency, icmsg_type_t msg_type, int core_id,
      int ccache_pending_index, bool is_eviction, InstrLegacy *instr,  net_prio_t priority);

    // Insert the line.  Booleans signal whether a.) it was a prefetch from
    // hardware (and thus speculative) b.) a read/write. c.) should be kept
    // incoherent in the cache, i.e., will not generate coherence traffic.
    // Incoherent Fill() will get free read-to-write upgrades and does not
    // generate read releases.
    bool Fill(uint32_t addr, bool was_prefetch, bool read_not_write, bool incoherent, bool &squashed);
    // XXX Deprecated XXX [20090929]
    //bool Fill(uint32_t addr, bool was_prefetch, bool read_not_write);

    // Insert the real index (int pendingMiss) into the list of mappings
    int pendingMissFifoInsert(int index);
    // Called when an entry is returned to the cache and must be removed.  On
    // return, the mapping no longer exists
    void pendingMissFifoRemove(int index);
    // Returns the virtual index (the one in an MSHR) for a given physical
    // index in the FIFO.  Note that the physical index implies ordering so
    // index 0 is the head of the fifo and index N-1 is the tail.
    int pendingMissFifoGetEntry(int index);
    // Given the index from an MSHR->PendingIndex() return the index into
    // pendingMiss that holds that entry.
    int ConvPendingToReal(int index);
    int ConvRealToPending(int index);
    // Dump the state of the FIFO.
    void dumpPendingMissFIFO(FILE *stream);
    // Return true if there is a request to the line outstanding that will not
    // fill into the cluster cache, e.g., a global operation.
    bool IsPendingNoCCFill(uint32_t addr);
    void SetMemoryBarrierPending(   int tidx ) { MemoryBarrierPending[tidx]   = true;   }
    void ClearMemoryBarrierPending( int tidx ) { MemoryBarrierPending[tidx]   = false;  }
    bool GetMemoryBarrierPending(   int tidx ) { return MemoryBarrierPending[tidx];     }
    void SetMemoryBarrierCompleted( int tidx ) { MemoryBarrierCompleted[tidx] = true;   }
    void AckMemoryBarrierCompleted( int tidx ) { MemoryBarrierCompleted[tidx] = false;  }
    bool GetMemoryBarrierCompleted( int tidx ) { return MemoryBarrierCompleted[tidx];   }
    // Called by cache_model to add currently requesting core to CCache bitmask
    // for the line.  Can be a read or a write.
    void CCacheProfilerTouch(uint32_t addr, int tid, bool is_read_request); 
    void ICacheProfilerTouch(uint32_t addr, int tid); 

    // Handle requests coming down from the global cache to deal with coherence
    // actions.  Return true if the request was handled and the message can be
    // removed.  This call may generate a reply.  Returns false if probe is
    // ignored and should not be removed from network yet.
    bool helper_handle_coherence_probe(MissHandlingEntry<rigel::cache::LINESIZE> &mshr);
    // Handle invalidations originating from a broadcast operation.
    bool helper_handle_ccbcast_probe(MissHandlingEntry<rigel::cache::LINESIZE> &mshr);

    // Set the MSHR associated with the address to be a read-to-write coherence
    // upgrade.
    void set_read_to_write_upgrade_pending(uint32_t addr);
    // Set all pendingMiss entries to be owned by a certain cluster.
    // This is only used by the cluster cache to wake/sleep cores.
    void PM_set_owning_cluster(ClusterLegacy *owner);
    // Queuery and update routines for CCache HW prefetches.
    void inc_hwprefetch_count() { 
      assert(hwprefetch_count < MAX_L2D_OUTSTANDING_PREFETCHES && "Overflow!"); hwprefetch_count++; }
    void dec_hwprefetch_count() { assert(hwprefetch_count > 0 && "Underflow!"); hwprefetch_count--; }
    void reset_hwprefetch_count() { hwprefetch_count = 0; }
    bool hwprefetch_enabled() const { return hwprefetch_count < MAX_L2D_OUTSTANDING_PREFETCHES; }
    int get_hwprefetch_count() const { return hwprefetch_count; }
    // Return true if the address is valid and the line is kept incoherent.
    bool get_incoherent(const uint32_t addr) const;   
    
  public: /* DATA */
    // Pointer to the cache model associated with this cluster's L2
    CacheModel *Caches; 
    Profile *profiler;

    HWPrefetcher *hw_prefetcher;

    // Pointer to the tile-level interconnect that this cluster is attached to.
    TileInterconnectBase *TileInterconnect;
    // Free pool of uniq ids for virtual mappings
    bool fifo_map_virt_free_pool[MAX_L2D_OUTSTANDING_MISSES];

    // Table that holds the mappings for PendingIndex() values carried by MSHRs
    // and the actual entry in pendingMiss[].  fifo_occupancy always
    // points to the last entry (the tail) where insertions occur from pend()
    rigel::cache::fifo_map_entry_t fifo_miss_map[MAX_L2D_OUTSTANDING_MISSES];

    // all "Global" arrays here should be sized, at minimum, at one entry per
    // tid (to support the minimum required one access per thread at a time)
    // FIXME: support multiple outstanding accesses
    // Per-core bitmap of cores with global memory operations (and atomics)
    // outstanding.  Cleared when an operation completes.  When the access is
    // done, GlobalMemOpCompleted[tid] is set.
    bool *GlobalMemOpOutstanding; 
    bool *GlobalMemOpCompleted;   
    // This is needed for CacheModel::global_memory_access, in order to simulate
    // atomics being completed at the core.
    bool *GlobalLoadForCoreAtomicOutstanding;
    //TLBs
    //I'd prefer this stuff to be private, but I want a Cluster to be able to call
    //getHits() and getMisses().
    TLB **tlbs; //Array of pointer-to-TLBs; not array of TLBs because we want different constructor arguments for each.
    size_t num_tlbs;
    // true if all MSHRs are full or a barrier is pending.  FIXME: As sort of a
    // kludge to remove a deadlock condition on Fill()'s in a full network, I am
    // allowing evictions to get an extra MSHR entry.  This is actually how the
    // hardware sort of works now in the CCache since there is an atomic swap on
    // a the MSHR from  the fill from the GCache  and the eviction going back up
    // Add check for broad casts
    bool mshr_full(bool is_eviction, bool is_bcast = false);
    // DEPRECATED - Use version with is_eviction instead.
    bool mshr_full();
    // Attempt to lock an address for thread_num.  This is necessary to make
    // sure non-atomic CCache operations (uarch) appear atomic (arch) to SW.  It
    // avoids livelock.  If the lock is already held for the address/thread or
    // no thread holds the lock, return true.  Otherwise, return false.  Unlock
    // on success.
    bool nonatomic_ccache_op_lock(const int thread_num, const uint32_t addr);
    void nonatomic_ccache_op_unlock(const int thread_num, const uint32_t addr);
    void dump_nonatomic_state() const { printf("dump_nonatomic_state TODO?\n"); };

  private: /* DATA */
    // Which cluster does this cache belong to (global number)
    const int cluster_num;
    // WDT counter used to see if we exceed some number of cycles without
    // removing what is on the incoming link.
    int network_stall_WDT;
    // TRACKED per THREAD!!!
    // Track the number of outstanding writebacks from each core and one extra
    // for evictions from the cluster cache.
    int  *WritebacksPending;      //[rigel::THREADS_PER_CLUSTER+1];
    // Track whether a given core has a memory barrier in flight.
    bool *MemoryBarrierPending;   //[rigel::THREADS_PER_CLUSTER];
    bool *MemoryBarrierCompleted; //[rigel::THREADS_PER_CLUSTER];

    // The number of cycles a request is stuck at
    // helper_handle_coherence_probe().  Check against a WDT every call to
    // ensure we are not getting a request stuck without the access_bit being
    // set.
    int wdt_count_access_bit_probe_failures;
    // Sequence numbers used to keep ordering between G$/C$ pairs.
    seq_num_t *seq_num;
    // Number of outstanding hardware prefetches.
    int hwprefetch_count;
    // Locks for nonatomic CCache operations.
    rigel::cache::nonatomic_ccentry_t * nonatomic_tbl;
    // Locality tracking info (need to be constructed at runtime with calculated
    // DRAM system parameters);
    LocalityTracker **perCoreLocalityTracker;
    LocalityTracker *aggregateLocalityTracker;
    // Keep track of how many outstanding broadcast acks there are.
    int outstanding_bcast_ack;
    // Keep track of how many entries are in the FIFO (almost always the same
    // as the number of valid MSHRs, but sometimes not, like when you want to
    // "atomically" Remove() and Insert() something to put it on the back of
    // the FIFO
    int fifo_occupancy;
  private: /* ACCESSORS */
    // Check to see if messages have been in the network for too long.  Debug
    // only.
    bool network_wdt_check(MissHandlingEntry<LINESIZE> mshr);
    // Check to see if the CCache WDT has expired and if so, dump state of
    // memory system and exit pronto.
    void ccache_wdt_expired();
    // Handle an outstanding memory barrier if needed.
    void ccache_handle_membar();
    // Handle pending misses.  Iterate through all valid misses and schedule
    // them as needed.
    void ccache_handle_pending_misses();
    // Remove any requests from the tile interconnect that have already been
    // serviced by the global cache.
    void ccache_handle_tile_net_replies();
    // Handle any profiling stuff that needs to be done at the top of PerCycle().
    void ccache_handle_profiling();
    // Deal with any fill operations that were blocked on a previous cycle
    // *first* before attending to new requests that may tie up resources.
    void ccache_handle_deferred_fills();
    // Try to acquire a logical channel for the G$/C$ pair.  Returns false if
    // one could not be allocated.
    bool helper_acquire_seq_channel(int gcache_bank, int & seq_channel) const;
    // Check for valid pendingMiss invariants.  Abort on failure.
    void helper_check_pending_miss_invariants(const int idx);

  friend class CacheModel;
  friend class HWPrefetcher;
  friend class ClusterLegacy;
};
     
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline bool 
L2Cache::IsPendingNoCCFill(uint32_t addr) 
{
  addr &= rigel::cache::LINE_MASK;
  for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
    if (pendingMiss[i].check_valid(addr)) {
      icmsg_type_t msg_type = pendingMiss[i].GetICMsgType();
      if (!ICMsg::check_can_ccache_fill(msg_type)) return true;
    }
  }
  return false;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline int 
L2Cache::pendingMissFifoInsert(int index) {
  assert(index >= 0 && index < MAX_L2D_OUTSTANDING_MISSES 
    && "Invalid index!  Overrunning the table!");

  int uniq_id;
  for (uniq_id = 0; uniq_id < MAX_L2D_OUTSTANDING_MISSES; uniq_id++) {
    if (fifo_map_virt_free_pool[uniq_id] == false) {
      // Allocate the virt index
      fifo_map_virt_free_pool[uniq_id] = true;
      break;
    }
  }
  assert(uniq_id < MAX_L2D_OUTSTANDING_MISSES && "Invalid unique identifier");

  // Set up the mapping between the real index that was allocated by the
  // cache controller and passed into this function and the virtual mapping
  // just allocated from the free pool

  // NOTE: A long time ago, we tried to find the slot to insert into with:
  // size_t tail = validBits.getNumSetBits();
  // This usually works, but not when you want to "atomically" remove and insert an
  // MSHR (to put it in the back of the line, e.g., when a Fill() fails).
  // We can either iterate through the array until we find a virt_idx of -1, or keep
  // a counter fifo_occupancy that gets incremented and decremented as Insert()s
  // and Remove()s are performed.  Let's go with the latter for now.
  size_t tail = fifo_occupancy;
  fifo_miss_map[tail].virt_idx = uniq_id;
  fifo_miss_map[tail].real_idx = index;
  fifo_occupancy++;

  // This is what mshr->pendingIndex() will return
  return uniq_id;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline void 
L2Cache::pendingMissFifoRemove(int index) {
  assert(index >= 0 && "Invalid index!  Underflow on table!");
  assert(index < MAX_L2D_OUTSTANDING_MISSES && "Invalid index!  Overrunning the table!");
  assert(fifo_map_virt_free_pool[index] == true 
    && "Trying to return an entry that was never allocated!");

  //fprintf(stderr, "Cycle 0x%08"PRIX64": Removing virtual index %d from FIFO ", rigel::CURR_CYCLE, index);
  //rigel_dump_backtrace();

  // Return Unique ID to the free list
  fifo_map_virt_free_pool[index] = false;
  int ret = -1;
  for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
    if (fifo_miss_map[i].virt_idx == index) {
      ret = i;
      break;
    }
  }
  
  if(ret == -1) {
    fprintf(stderr, "Error: C$ Pending Miss FIFO Entry at index %d not found in mapping table\n", index);
    dumpPendingMissFIFO(stderr);
    exit(1);
  }

  //fprintf(stderr, "at index %d\n", ret);
  // Move all of the subsequent entries up one
  for ( ; ret < MAX_L2D_OUTSTANDING_MISSES-1; ret++) {
    fifo_miss_map[ret] = fifo_miss_map[ret+1];
  }
  fifo_miss_map[MAX_L2D_OUTSTANDING_MISSES-1].invalidate();
  fifo_occupancy--;

  //dumpPendingMissFIFO(stderr);
  //fprintf(stderr, "-----------------------\n");
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline int 
L2Cache::pendingMissFifoGetEntry(int index) {
  assert(index >= 0 && "Invalid index!  Underrunning the table!");
  assert(index < MAX_L2D_OUTSTANDING_MISSES && "Invalid index!  Overrunning the table!");
  
  // If we are past the end of the list, we return -1 signalling end of list
  if (index >= MAX_L2D_OUTSTANDING_MISSES) return -1;

  return fifo_miss_map[index].virt_idx;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline int 
L2Cache::ConvPendingToReal(int index) {
  using namespace rigel::cache;
  assert(index >= 0  && "Invalid index!  Underrunning the table!");
  assert(index < MAX_L2D_OUTSTANDING_MISSES && "Invalid index!  Overrunning the table!");

  for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
    if (fifo_miss_map[i].virt_idx == index) return fifo_miss_map[i].real_idx;
  }
  rigel::GLOBAL_debug_sim.dump_all();
  fprintf(stderr, "Cycle 0x%08"PRIX64": Error in cluster cache %d: Could not find virtual index %d in FIFO\n",
          rigel::CURR_CYCLE, cluster_num, index);
  dumpPendingMissFIFO(stderr);
  assert(0 && "Did not find index in fifo_miss_map!");
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline int 
L2Cache::ConvRealToPending(int index) 
{
  using namespace rigel::cache;
  assert(index >= 0  && "Invalid index!  Underrunning the table!");
  assert(index < MAX_L2D_OUTSTANDING_MISSES && "Invalid index!  Overrunning the table!");

  for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
    if (fifo_miss_map[i].real_idx == index) return fifo_miss_map[i].virt_idx;
  }
  rigel::GLOBAL_debug_sim.dump_all();
  fprintf(stderr, "Cluster: %d\n", cluster_num);
  dumpPendingMissFIFO(stderr);
  assert(0 && "Did not find index in fifo_miss_map!");
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline void
L2Cache::dumpPendingMissFIFO(FILE *stream)
{
    fprintf(stream, "Dumping Pending Miss FIFO:\n");
    for(int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
        fprintf(stream, "%d: Real %d, Virtual %d\n", i, fifo_miss_map[i].real_idx, fifo_miss_map[i].virt_idx);
    }
}

////////////////////////////////////////////////////////////////////////////////
// L2Cache Constructor
////////////////////////////////////////////////////////////////////////////////
inline
L2Cache::L2Cache(CacheModel *caches, TileInterconnectBase *ti, Profile *profile, int clusternum) : 
  CacheBase< L2D_WAYS, L2D_SETS, LINESIZE, 
    MAX_L2D_OUTSTANDING_MISSES, CACHE_WRITE_THROUGH, L2D_EVICTION_BUFFER_SIZE >::CacheBase(),
  Caches(caches), profiler(profile), TileInterconnect(ti),
  cluster_num(clusternum), network_stall_WDT(0), wdt_count_access_bit_probe_failures(0), 
  hwprefetch_count(0), outstanding_bcast_ack(0)
{
  using namespace rigel::cache;

  GlobalMemOpOutstanding = new bool[rigel::THREADS_PER_CLUSTER];
  GlobalMemOpCompleted   = new bool[rigel::THREADS_PER_CLUSTER];
  GlobalLoadForCoreAtomicOutstanding = new bool[rigel::THREADS_PER_CLUSTER];

  WritebacksPending      = new int[rigel::THREADS_PER_CLUSTER+1];
  MemoryBarrierPending   = new bool[rigel::THREADS_PER_CLUSTER];
  MemoryBarrierCompleted = new bool[rigel::THREADS_PER_CLUSTER];

    //XXX Neal added prefetching hook
  hw_prefetcher = new HWPrefetcher(this /*what about pf type?*/);

  // Initialize the mappings.  As entries are removed, the table is shifted
  for (int i = 0; i < MAX_L2D_OUTSTANDING_MISSES; i++) {
    fifo_miss_map[i].invalidate();
    fifo_map_virt_free_pool[i] = false;
  }
  // Allocate the interlock for nonatomic accesses at the cluster cache.
  nonatomic_tbl = new nonatomic_ccentry_t [rigel::THREADS_PER_CLUSTER];
  assert (rigel::THREADS_PER_CLUSTER >= rigel::CORES_PER_CLUSTER);
  for (int i = 0; i < rigel::THREADS_PER_CLUSTER; i++) {
    nonatomic_tbl[i].reset();
  }

  // Initially there are no outstanding Global Operations
  // FIXME: this loop should be over an orthogonal constant,
  // OUTSTANDING_BLAH_PER_CLUSTER or something, not just one per thread
  for (int i = 0; i < rigel::THREADS_PER_CLUSTER; i++) {
    GlobalMemOpOutstanding[i] = false;
    GlobalMemOpCompleted[i]   = false;
    GlobalLoadForCoreAtomicOutstanding[i] = false;
    WritebacksPending[i]      = 0;
    MemoryBarrierPending[i]   = false;
    MemoryBarrierCompleted[i] = false;
  }

  // For cache evictions
  // FIXME: this should be an orthogonal constant OUTSTANDING_BLAH_PER_CLUSTER or something
  WritebacksPending[rigel::THREADS_PER_CLUSTER] = 0;

  MSHR_COUNT = MAX_L2D_OUTSTANDING_MISSES;
  fifo_occupancy = 0;
  // Initialize the per-cluster/bank counters.
  seq_num = new seq_num_t [rigel::NUM_GCACHE_BANKS];
  // Construct LocalityTrackers
  if(rigel::ENABLE_LOCALITY_TRACKING)
  {
    //Need 1 extra perCore tracker for eviction traffic
    perCoreLocalityTracker = new LocalityTracker *[rigel::CORES_PER_CLUSTER+1];
    for(int i = 0; i < rigel::CORES_PER_CLUSTER+1; i++)
    {
      char name[128];
      snprintf(name, 128, "Cluster %i Core %i C$ Misses", cluster_num, i);
      std::string cppname(name);
      perCoreLocalityTracker[i] = new LocalityTracker(cppname, 8, LT_POLICY_RR,
        rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
        rigel::DRAM::BANKS, rigel::DRAM::ROWS);
    }
    char name[128];
    snprintf(name, 128, "Cluster %i Aggregate C$ Misses", cluster_num);
    std::string cppname(name);
    aggregateLocalityTracker = new LocalityTracker(cppname, 8, LT_POLICY_RR,
      rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
      rigel::DRAM::BANKS, rigel::DRAM::ROWS);
  }
  //Construct TLBs
  if(rigel::ENABLE_TLB_TRACKING)
  {
    //num_tlbs = 4*3*17*2;
		//num_tlbs = (6+6+5+5+4+4+3+3+2+2+1)*9;
    num_tlbs = 11*16;
    tlbs = new TLB *[num_tlbs];
    TLB **walker = tlbs;

    char name[128];
    snprintf(name, 128, "C$ %i", cluster_num);
    for(unsigned int i = 0; i <= 0; i += 1) {
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
////////////////////////////////////////////////////////////////////////////////
inline bool 
L2Cache::mshr_full(bool is_eviction, bool is_bcast) {
  size_t numSet = validBits.getNumSetBits();
  // Reserve capacity for handing broadcasts.
  const int BCAST_COUNT = 6;
  if (is_bcast && outstanding_bcast_ack > BCAST_COUNT) { return true; }
  else if (is_bcast) {
    if (numSet < (MAX_L2D_OUTSTANDING_MISSES-L2D_EVICTION_BUFFER_SIZE)) { return false; }
    return true;
  }

  // If the request at the tail is valid, we cannot accept new requests
  if (is_eviction) {
    return (numSet >= (MAX_L2D_OUTSTANDING_MISSES-BCAST_COUNT));
  } else {
    return (numSet >= (MAX_L2D_OUTSTANDING_MISSES-(L2D_EVICTION_BUFFER_SIZE+BCAST_COUNT)));
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline bool 
L2Cache::helper_acquire_seq_channel(int gcache_bank, int & seq_channel) const
{
  return seq_num[gcache_bank].get_unused_channel(seq_channel);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
inline bool 
L2Cache::get_incoherent(const uint32_t addr) const
{
  int set = get_set(addr);
  for (int w = 0; w < numWays; w++) {
    if (TagArray[set][w].valid()) {
      if (addr == TagArray[set][w].get_tag_addr()) {
        return TagArray[set][w].get_incoherent();
      }
    }
  }
  return false;
}
#endif
