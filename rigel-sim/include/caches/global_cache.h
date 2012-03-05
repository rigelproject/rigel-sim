////////////////////////////////////////////////////////////////////////////////
// includes/caches/global_cache.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the definition of the global cache.  This
//  file is included from cache_model.h
//
////////////////////////////////////////////////////////////////////////////////


#ifndef __CACHES__GLOBAL_CACHE_H__
#define __CACHES__GLOBAL_CACHE_H__

#include "sim.h"
#include "caches/cache_base.h"
#include <list>
#include <set>

class GlobalNetworkBase;
class Profile;
class CacheDirectory;
class CallbackInterface;
class LocalityTracker;
class TLB;
namespace rigel {
  class ConstructionPayload;
}
namespace directory {
  struct dir_of_timing_t;
}

template< int ways, int sets, int linesize, int MaxOutstandingMisses, int WBPolicy,
  int CACHE_EVICTION_BUFFER_SIZE > class CacheBase;

////////////////////////////////////////////////////////////////////////////////
//        class GlobalCache            (Definition)
//
////////////////////////////////////////////////////////////////////////////////
// Each instance of this class implements a single Rigel global cache bank.
// It has a reference to the global memory model, which includes the memory controller
// instances the global cache uses to fulfill misses,
// and a pointer to the global network, where it services requests from the cores

using namespace rigel::cache;
using namespace directory;

class GlobalCache : public CacheBase < GCACHE_WAYS, GCACHE_SETS, LINESIZE, GCACHE_OUTSTANDING_MISSES,
  CACHE_WRITE_THROUGH, GCACHE_EVICTION_BUFFER_SIZE >, public CallbackInterface
{
  public:
    //FIXME split this into channel ID and bank-within-channel ID
    unsigned int bankID;

    // constructor
    GlobalCache(rigel::ConstructionPayload cp);
    // Clock the cache.
    void PerCycle();
    // Insert the line at 'addr' into the cache with all bits valid.  If the
    // lock bit is set, the line is locked into the GCache, i.e., it cannot be
    // evicted, until the line is accessed.
    bool Fill(uint32_t addr, bool was_hwprefetch, bool was_bulkpf, bool dirty, bool nonspeculative);

    // Wait on a fill for a _variable_ latency
    //void pend(uint32_t addr, int latency, icmsg_type_t icmsg_type, int core_id,
    //          int ccache_pending_index, bool is_eviction, InstrLegacy *instr,
    //          net_prio_t priority);
    void pend( CacheAccess_t ca_in, int latency,
               int ccache_pending_index, bool is_eviction);

    // Attempt to evict a line from the global caches.  If we are unable to
    // evict, stall is set and the return value is invalid.  If the eviction
    // succeeded, stall is cleared and the return value is the which way within
    // the set for 'addr' that was freed.
    int evict(uint32_t addr, bool &stall);
    void setGlobalNetwork(GlobalNetworkBase *gnet) {GlobalNetwork = gnet;}
    GlobalNetworkBase *get_GlobalNetwork() const { return GlobalNetwork; }
    // Return the pointer to the cache directory.
    CacheDirectory *get_cache_directory() { return cache_directory; }
    // true if all MSHRs are full.  FIXME: As sort of a
    // kludge to remove a deadlock condition on Fill()'s with the MC MSHRs full,
    // Allowing evictions to get an extra rigel::cache::GLOBAL_CACHE_EVICTION_BUFFER_SIZE entries.
    bool mshr_full(bool is_eviction) const;
    // Dump locality information
    void dump_locality() const;
    // Dump TLB hit rate information
    void dump_tlb() const;
    // Gather stats at the end of the simulation
    void gather_end_of_simulation_stats() const;

		//NOTE These are public so that Chip can get at them to do stats aggregation in EndSim().
		//FIXME Encapsulate these better
		TLB **tlbs; //Array of pointer-to-TLBs; not array of TLBs because we want different constructor arguments for each.
		size_t num_tlbs;
  protected:
    // We specialize because of the complex mapping between addrs and G$ banks/sets.
    virtual int get_set(uint32_t addr) const;
    virtual void callback(int a, int b, uint32_t c) {
      PM_set_line_ready(a, c, rigel::CURR_CYCLE + b);
    }
  private: /* HELPERS */
    // Check to see if a request has WDT timeouts.
    void helper_check_msg_timers(const std::list<ICMsg>::iterator &rmsg_iter) const;
    // Handle broadcasts.  Return true if one was found.
    bool helper_handle_bcasts(const std::list<ICMsg>::iterator &rmsg_iter, std::list<ICMsg> &reply_buffer);
    // Do global cache hardware prefetching.
    void helper_handle_prefetches(const std::list<ICMsg>::iterator &rmsg_iter);
    // If a request is found, try to insert it into the ReplyBuffer and remove
    // the request from the RequestBuffer.  May need to retry.  Note that
    // rmsg_iter might be modified by the call.
    void helper_handle_gcache_request_hit(std::list<ICMsg>::iterator &rmsg_iter);
    // Test a message's directory permission to see if it can access the GCache.
    // If not, start a new directory request.
    bool helper_handle_directory_access(ICMsg &rmsg);
    // Handle collecting probe responses.  Return true if the message is
    // handled, false means we have to continue processing.
    bool helper_handle_probe_responses( dir_of_timing_t *);
    // Schedule request with the memory model, ACKing the MSHR if successful.
    // Single line
    bool scheduleWithGlobalMemory(uint32_t addr, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  int PendingMissEntryIndex);
    // Multiple lines
    bool scheduleWithGlobalMemory(std::set<uint32_t> addrs, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  int PendingMissEntryIndex);
    // Find the tag associated with the line and set the accessed bit associated
    // with it.
    void set_accessed_bits(uint32_t addr);
    // Handle requests coming in from one of the request queues.
    void helper_handle_new_requests(int myQueueNum);
    // Handle messages with already allocated MSHRs.
    void helper_handle_pending_requests(int myQueueNum);
    // Handle the responses generated on behalf of the HWcc implementation.
    void helper_handle_directory_responses(int myQueueNum);

  private: /* DATA */
    // Reference to memory timing model
    rigel::MemoryTimingType &MemoryTimingModel;
    // Ptr to global network
    GlobalNetworkBase *GlobalNetwork;
    // Restrict port utilization.
    int replies_this_cycle;
    int requests_this_cycle;
    // The directory for the global cache bank. Intitialized in the constructor.
    CacheDirectory *cache_directory;
    // The timing struct of pending requests for the overflow directory to
    // handle probe responses.  Initialized in the constructor.
    dir_of_timing_t *probe_dir_timer;
    //LocalityTracker ***perCoreLocalityTracker;
    LocalityTracker **perClusterLocalityTracker;
    LocalityTracker *aggregateLocalityTracker;

  // Network needs to check IsValid().
  friend class GlobalNetworkCrossBar;
  friend class GlobalNetworkBase;
  friend class L2Cache;
  // DRAMController needs to call callback().
  friend class DRAMController;
};

inline bool
GlobalCache::mshr_full(bool is_eviction) const{
  if (is_eviction) {
    return (validBits.getNumSetBits() == GCACHE_OUTSTANDING_MISSES);
  } else {
    return (validBits.getNumSetBits() >= (GCACHE_OUTSTANDING_MISSES-GCACHE_EVICTION_BUFFER_SIZE));
  }
}

#endif //#ifndef __GLOBAL_CACHE_H__
