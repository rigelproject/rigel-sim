////////////////////////////////////////////////////////////////////////////////
// l1d.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the definition of the L1DCache and the NoL1DCache class.
//  This file is included from cache_model.h.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __CACHES__L1D_H__
#define __CACHES__L1D_H__

// Forward declaration of L2Cache for SetNextLevel()
class L2Cache;

////////////////////////////////////////////////////////////////////////////////
/// L1DCache
/// Core-level data cache.  Usually referred to internally as a line buffer.
////////////////////////////////////////////////////////////////////////////////
class L1DCache : public CacheBase<
  L1D_WAYS, L1D_SETS, LINESIZE,
  L1D_OUTSTANDING_MISSES, CACHE_WRITE_THROUGH, L1D_EVICTION_BUFFER_SIZE
>
{
  public: /* ACCESSORS */
    //Need to initialize members in case memory is not 0'ed out.
    L1DCache() :
      CacheBase< L1D_WAYS, L1D_SETS, LINESIZE, L1D_OUTSTANDING_MISSES,
                 CACHE_WRITE_THROUGH, L1D_EVICTION_BUFFER_SIZE>::CacheBase(),
      temporary_access_pending(false), temporary_invalidate_pending(false),
      temporary_access_set(-1), temporary_access_way(-1) {}
    void PerCycle();
    void SetNextLevel(L2Cache *nextlevel) { NextLevel = nextlevel;  }
    // The Fill completed is only temporary, it will be evicted as soon as the
    // core completes.
    void set_temporary_pending(uint32_t addr) {
      temporary_access_pending = true;
      temporary_invalidate_pending = false;
      temporary_access_set = get_set(addr);
      temporary_access_way = get_way_line(addr);
      assert(-1 != temporary_access_way && "Invalid way!");
    }
    bool get_temporary_access_pending() const { return temporary_invalidate_pending; }
    // The core has accessed the temporary line, it will be invalidated by
    // PerCycle soon.
    void set_temporary_access_complete(uint32_t addr) {
      if (!temporary_access_pending) return;
      if (get_set(addr) == temporary_access_set && get_way_line(addr) == temporary_access_way) {
        temporary_invalidate_pending = true;
      }
    }
    // Insert the address into the cache with all words valid.  May generate an
    // eviction.
    bool Fill(uint32_t addr);

  private: /* DATA */
    // We need to know what the next level of the hierarchy is so that we can
    // continually check to see if the line we requested last is available
    L2Cache *NextLevel;
    // Allow the line to be in the L1 until it is first accessed.  This allows
    // for coherence actions to evict a line immidiately at the L2, but for the
    // core to make forward progress. This is a little bit fudged for writes
    // although it could be implemented by sending data to the cluster cache on
    // write requests.
    bool temporary_access_pending;
    // Set when the core has accessed the line.  PerCycle() should invalidate it
    // when found.
    bool temporary_invalidate_pending;
    int temporary_access_set;
    int temporary_access_way;

  friend class CacheModel;
};

////////////////////////////////////////////////////////////////////////////////
/// NoL1DCache
/// Stub for handling a core with no first-level cache, i.e., all requests go to
/// the cluster cache.
////////////////////////////////////////////////////////////////////////////////
class NoL1DCache
{

  public: /* ACCESSORS */
    // Default constructor.
    NoL1DCache();
    // Clock the cache.
    void PerCycle();

    // Select an entry and set valid.
    //void pend(uint32_t addr, int latency, icmsg_type_t ic_t, int core_id, int
    //  ccache_pending_index, bool is_eviction, InstrLegacy *instr,  net_prio_t priority);
    void pend(const CacheAccess_t ca, int latency, int ccache_pending_index, bool is_eviction);

    // Check to see if we can provide the value to the core.
    bool IsReady( uint32_t addr );
    // Is the word available already?
    bool IsValidWord( uint32_t addr );
    // Reset all of the valid/ready counts for the core.
    void clear(uint32_t addr);

  public: /* DATA */
    bool ready[rigel::ISSUE_WIDTH];
    bool valid[rigel::ISSUE_WIDTH];
    uint32_t addrs[rigel::ISSUE_WIDTH];
    int32_t count[rigel::ISSUE_WIDTH];
};

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////
// FIXME: comment
inline
NoL1DCache::NoL1DCache()
{
  int j;
  for (j=0;j<rigel::ISSUE_WIDTH;j++)
  {
    ready[j] = false ;
    valid[j] = false;
    addrs[j]  = 0xdeadbeef;
    count[j] = 0;
  }
};

////////////////////////////////////////////////////////////////////////////////
// PerCycle()
////////////////////////////////////////////////////////////////////////////////
// FIXME: comment
inline void
NoL1DCache::PerCycle()
{
  int j;

  #ifdef DEBUG_CACHES
    cerr << "PerCycle() NoL1D: " << endl;
  #endif

  // update status of outstanding
  for (j = 0; j < rigel::ISSUE_WIDTH; j++)
  {
    if( count[j] > 0 && valid[j] )
    {
      assert( ready[j] == false && "Ready Data Not Accepted");
      count[j]--;
      if( count[j]==0 ) {
        ready[j] = true;
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
// pend()
////////////////////////////////////////////////////////////////////////////////
// FIXME: comment
inline void
//NoL1DCache::pend(uint32_t addr, int latency, icmsg_type_t ic_t, int core_id, int
//  ccache_pending_index, bool is_eviction, InstrLegacy *instr,  net_prio_t priority)
NoL1DCache::pend(
  const CacheAccess_t ca, int latency, int ccache_pending_index, bool is_eviction )
{
  int j;

  #ifdef DEBUG_CACHES
    cerr << "Pend() NoL1D: 0x" << hex << ca.get_addr() << endl;
  #endif

  for( j = 0; j < rigel::ISSUE_WIDTH; j++)
  {
    if( !valid[j] )
    {
      valid[j] = true;
      ready[j] = false;
      addrs[j] = ca.get_addr();
      count[j] = latency;
      return;
    }
    assert(0 && "NoL1D::pend() - too many outstanding accesses!" );
  }
};

////////////////////////////////////////////////////////////////////////////////
// IsReady()
////////////////////////////////////////////////////////////////////////////////
// FIXME: comment
inline bool
NoL1DCache::IsReady( uint32_t addr )
{
  int j;

  for ( j = 0; j < rigel::ISSUE_WIDTH; j++)
  {
    if( valid[j] && ready[j] && addrs[j]==addr )
    {
      #ifdef DEBUG_CACHES
        cerr << "IsReady() NoL1D: 0x" << hex << addr << endl;
      #endif
      return true;
    }
  }
  return false;
};

////////////////////////////////////////////////////////////////////////////////
// IsValidWord()
////////////////////////////////////////////////////////////////////////////////
// FIXME: comment
inline bool
NoL1DCache::IsValidWord( uint32_t addr )
{
  int j;

  for(j = 0; j < rigel::ISSUE_WIDTH; j++)
  {
    if( valid[j] && addrs[j]==addr )
    {
      #ifdef DEBUG_CACHES
        cerr << "IsValid() NoL1D: 0x" << hex << addr << endl;
      #endif
      return true;
    }
  }
  return false;
};

////////////////////////////////////////////////////////////////////////////////
// clear()
////////////////////////////////////////////////////////////////////////////////
// FIXME: comment
inline void
NoL1DCache::clear(uint32_t addr)
{
  int j;

  #ifdef DEBUG_CACHES
    cerr << "Clear() NoL1D: 0x" << hex << addr << endl;
  #endif

  for(j = 0; j < rigel::ISSUE_WIDTH; j++)
  {
    if( addrs[j] == addr )
    {
      valid[j] = false;
      ready[j] = false;
      count[j] = -1;
      return;
    }
  }
  assert(0 && "NoL1D: Invalid Path");
}

#endif



