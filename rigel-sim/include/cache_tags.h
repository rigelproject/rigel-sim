////////////////////////////////////////////////////////////////////////////////
// cache_tags.h
////////////////////////////////////////////////////////////////////////////////
//
//  The CacheTag class for tracking metadata for lines resident in the various
//  levels of the cache is defined and implemented here.
//
////////////////////////////////////////////////////////////////////////////////
#ifndef __CACHE_TAGS_H__
#define __CACHE_TAGS_H__

#include "define.h"
#include "sim.h"

/// CLASS NAME: CacheTag
/// 
/// The cache tags are used for tracking metadata related to cachelines presently
/// in any level of the cache.  Some of the fields are superfluous at some levels
/// of the cache, but it is easier and cleaner not to special case.
class CacheTag {
  public: /* METHODS */
    // Default constructor.
    CacheTag();
    // Call on every access to update LRU status.  We also set the accessed bit
    // to allow coherence probes to replace it/invalidate it.
    void touch() { last_accessed_cycle = rigel::CURR_CYCLE; _touch_count++; }
    // Number of times an operation is performed to the block
    uint64_t touch_count() { return _touch_count; }
    // Cycle this block was last touched/accessed
    uint64_t last_touch() { return last_accessed_cycle; }
    // Track the number of insertions to this line (proxy for evictions)
    uint64_t insert_count() { return _insert_count; }
    // Check for a hit on a per-word basis.
    bool hit(uint32_t addr, uint32_t addr_mask) const;
    // Only do a tag match and a valid check.  Per-word checks not done.
    bool tag_check(uint32_t addr) const;
    // Set/clear the prefetch bit signalling this was brought in on a prefetch.
    void set_hwprefetch() { _hwpf = true; }
    void clear_hwprefetch() { _hwpf = false; }
    bool is_hwprefetch() { return _hwpf; }
    //Bulk prefetch
    void set_bulkprefetch() { _bulkpf = true; }
    void clear_bulkprefetch() { _bulkpf = false; }
    bool is_bulkprefetch() { return _bulkpf; }
    
    // Insert a (partial/dirty) line into the cache.   Line must not be valid
    // initially.
    void insert(uint32_t addr, uint32_t dirty_mask, uint32_t valid_mask);
    // Locking a tag disallow any forward progress until the line can be taken by
    // the next higher-level cache's interconnect, e.g., the tile interconnect
    // if the line is being evicted from the cluster cache.  The line is
    // implicitly unlocked by the fill() call.
    void lock(uint32_t addr);
    void unlock() { _lock = false; }
    bool locked() const { return (_lock); }
    // Return the address of the line we are trying to evict from this way.
    uint32_t victim_addr() const { return _victim_addr; }
    // Set the victim that we are trying to currently evict.
    void set_victim_addr(uint32_t addr) { _victim_addr = addr; }
    // Invalidate word(s) matched by the bit_mask if present.  Otherwise, ignore.
    void invalidate(uint32_t addr, uint32_t inv_mask);
    // Make the words valid, i.e., call on a fill().
    void make_all_valid(uint32_t addr);
    // Make the words valid.
    void make_valid(uint32_t addr, uint32_t valid_mask);
    // Unconditional invalidate.
    void invalidate() { _valid_mask = 0x0; _tag = 0xdeadbeef; }
    // Dirty the line only if it exists.  Does combining of dirty bits.
    void set_dirty(uint32_t addr, uint32_t dirty_mask);
    // Clean the words in the line.
    void clear_dirty(uint32_t addr, uint32_t clean_mask);
    // Check if any masked words are dirty.
    bool dirty(uint32_t addr, uint32_t dirty_mask);
    // Check if any word in the line is valid.  Rather, check if the line is not
    // valid and can be used for a fill.
    bool valid() const { return ((0 != _valid_mask) && !_lock); }
    // Return the tag for the cache line.
    uint32_t get_tag_addr() const { return _tag; }
    // Public access to return the words that are valid on the line.
    uint32_t get_valid_mask() { return _valid_mask; }
    // Change the state of the line in the cache.
    void  set_coherence_state(coherence_state_t s) { cc_state = s; }
    // Retrieve the state of the line in the cache.
    coherence_state_t get_coherence_state() { return cc_state; }

    // BEGIN cache-touch profiling information.  Used only by CCache in
    // cache_model.cpp.  The functionality is the same, but use different data
    // structures for each level of the cache.
    // CCache data special case.
    void profile_ccache_core_access(int core_num);
    // ICache special case.  Should work for both unified and separate L2I
    // configurations.
    void profile_icache_core_access(int core_num);
    // Base method for handling cluster-level cache sharing.
    void _profile_core_access(int core_num, uint64_t &core_bitmask, uint64_t *histogram);
    // END cache-touch profiling information.

    // ACC bit set on first touch.
    void set_accessed_bit_read() { accessed_bit_read = true; }
    void set_accessed_bit_write() { accessed_bit_write = true; }
    // ACC bit cleared on non-speculative Fill()
    void clear_accessed_bit_read() { accessed_bit_read = false; }
    void clear_accessed_bit_write() { accessed_bit_write = false; }
    // Clear both accessed bits.  Used in Gcache, but not in Ccache.
    void clear_accessed_bit() { accessed_bit_read = false; accessed_bit_write = false; }
    // ACC bit checked before responding to probe requests.
    bool get_accessed_bit() const { return accessed_bit_read && accessed_bit_write; }
    // Set both accessed bits.  Used in GCache, but not in CCache.
    void set_accessed_bit() { accessed_bit_read = true; accessed_bit_write = true; }
    // Dump the state of the cache tag to stderr.
    void dump() const;
    // We need to track whether a pending eviction is dirty or not for coherence
    bool get_victim_dirty() const { return dirty_victim; }
    void set_victim_clean() { dirty_victim = false; }
    void set_victim_dirty() { dirty_victim = true; }
    // Manipulate coherence tracking for the line.
    void clear_incoherent() { incoherent = false; }
    void set_incoherent() { incoherent = true; }
    bool get_incoherent() const { 
      if (!rigel::CMDLINE_ENABLE_HYBRID_COHERENCE && rigel::ENABLE_EXPERIMENTAL_DIRECTORY) { return false; }
      return incoherent; 
    }
   
  private: /* METHODS */

  private: /* DATA */
    // Used for tag matches.
    const uint32_t TAG_MATCH_MASK;
    // Used to clear MSb of any per-word variable.
    const uint32_t WORD_BITMASK;
    // Core data values.
    uint32_t _valid_mask;
    uint32_t _dirty_mask;
    uint32_t _tag;
    uint32_t _victim_addr;
    // Bit that is set when a HW prefetch brought this line into the cache.  May
    // want to add another bit for SW prefetching as well.  Bit is cleared when
    // the data is used non-speculatively.
    bool _hwpf;
    bool _bulkpf;
    bool _lock;
    // The rest of these are just 
    uint64_t last_accessed_cycle;
    uint64_t _insert_count;
    uint64_t _touch_count;
    // Begin cache-touch profiling information.  Used only by CCache.
    uint64_t ccache_core_access_bitmask;
    uint64_t icache_core_access_bitmask;
    // State of the line w.r.t. cache coherence
    coherence_state_t cc_state;
    // Bit that is set on a (non-speculative) fill and cleared when requesting
    // core accesses the line.
    bool accessed_bit_read;
    bool accessed_bit_write;
    // Set when the eviction victim is dirty.
    bool dirty_victim;
    // Set when the line is kept incoherent.  Used by HybridDirectory
    bool incoherent;
};
// end class CacheTag 
////////////////////////////////////////////////////////////////////////////////

/*********************** BEGIN CacheTag definitions **************************/

inline
CacheTag::CacheTag() : 
  TAG_MATCH_MASK(~(rigel::cache::LINESIZE-1)),
  WORD_BITMASK((1UL << (rigel::cache::LINESIZE/4) ) - 1),
   _valid_mask(0), _dirty_mask(0), _tag(0xdeadbeef), _victim_addr(0xdeadbeef),
   _hwpf(false), _bulkpf(false), _lock(false), last_accessed_cycle(0),
   _insert_count(0), _touch_count(0), ccache_core_access_bitmask(0),
   icache_core_access_bitmask(0), cc_state(CC_STATE_I),
   // By default accessed bit is set to all it to be replaced/invalidated.  Only
   // non-speculative fills will clear it.
   accessed_bit_read(true), accessed_bit_write(true),  dirty_victim(false),
   incoherent(false)
{ 
  // NADA
}

inline bool 
CacheTag::hit(uint32_t addr, uint32_t addr_mask) const
{
  addr_mask &= WORD_BITMASK;
  // Calculate the word offset for the address.
  bool word_hit = ((_valid_mask & addr_mask) == addr_mask);
  // Check for tag hit
  bool tag_hit = ((addr & TAG_MATCH_MASK) == _tag);
  return (tag_hit && word_hit);
}

inline bool 
CacheTag::tag_check(uint32_t addr)  const
{
  bool tag_hit = ((addr & TAG_MATCH_MASK) == _tag);
  bool valid = (0 != _valid_mask);
  return (valid && tag_hit);
}

inline void 
CacheTag::insert(uint32_t addr, uint32_t dirty_mask, uint32_t valid_mask) 
{
  _insert_count++;
  // Masks are per-word state bits.
  _valid_mask = (valid_mask & WORD_BITMASK);
  _dirty_mask = (dirty_mask & WORD_BITMASK);
  _tag = addr & TAG_MATCH_MASK;
  unlock();
  touch();
  // Default is to disable prefetch bits.
  clear_hwprefetch();
  clear_bulkprefetch();
  // On a fill, we reset the access bitmask for profiling.  Only used by the
  // cluster cache currently.
  ccache_core_access_bitmask = 0;
  icache_core_access_bitmask = 0;
  set_victim_clean();
  // By default, everything is kept coherent.
  clear_incoherent();
}

inline void
CacheTag::lock(uint32_t addr) 
{
  _tag = addr & TAG_MATCH_MASK;
  _valid_mask = 0;
  _lock = true;
}

inline void 
CacheTag::invalidate(uint32_t addr, uint32_t inv_mask) 
{
  // Make the line invalid w.r.t. the coherence protocol.
  set_coherence_state(CC_STATE_I);

  inv_mask &= WORD_BITMASK;
  if (!tag_check(addr)) { return; }
  _valid_mask = (_valid_mask & (~inv_mask)) & WORD_BITMASK;
}


inline void 
CacheTag::make_all_valid(uint32_t addr) 
{
  if (!tag_check(addr)) { return; }
  _valid_mask = WORD_BITMASK;
}

inline void 
CacheTag::make_valid(uint32_t addr, uint32_t valid_mask) 
{
  valid_mask &= WORD_BITMASK;
  if (!tag_check(addr)) { return; }
  _valid_mask |= valid_mask ;
}

inline void 
CacheTag::clear_dirty(uint32_t addr, uint32_t clean_mask) 
{ 
  clean_mask &= WORD_BITMASK;
  if (!tag_check(addr)) { return; }
  _dirty_mask = (_dirty_mask & (~clean_mask)) & WORD_BITMASK;
}


inline bool 
CacheTag::dirty(uint32_t addr, uint32_t dirty_mask)
{
  if (!tag_check(addr)) { return false; }
  return (0 != (_dirty_mask & dirty_mask));
}

inline void 
CacheTag::profile_icache_core_access(int tid) 
{
  using namespace rigel::profiler;
  _profile_core_access( tid, 
                        this->icache_core_access_bitmask,
                        icache_access_histogram);
}

inline void 
CacheTag::profile_ccache_core_access(int tid) 
{
  using namespace rigel::profiler;
  _profile_core_access( tid, 
                        this->ccache_core_access_bitmask,
                        ccache_access_histogram);
}

inline void 
CacheTag::set_dirty(uint32_t addr, uint32_t dirty_mask) 
{ 
  dirty_mask &= WORD_BITMASK;
  if (!tag_check(addr)) { return; }
  _dirty_mask = (_dirty_mask | dirty_mask) & WORD_BITMASK; 
  touch();
}

inline void 
CacheTag::dump() const
{
  fprintf(stderr, "tag: %08x ", _tag);
  fprintf(stderr, "victim_addr: %08x ", _victim_addr);
  fprintf(stderr, "lock: %01x ", _lock);
  fprintf(stderr, "hwprefetch: %01x ", _hwpf);
  fprintf(stderr, "bulkpf: %01x ", _bulkpf);
  fprintf(stderr, "accessed (R): %01x ", accessed_bit_read);
  fprintf(stderr, "accessed (W): %01x ", accessed_bit_write);
  fprintf(stderr, "dirty: %02x ", _dirty_mask);
  fprintf(stderr, "valid: %02x ", _valid_mask);
  fprintf(stderr, "incoherent: %02x ", incoherent);
}
/************************* END CacheTag definitions ***************************/

////////////////////////////////////////////////////////////////////////////////
/// _profile_core_access()
/// 
/// Pull out cache profiling logic into a single function and call it from each
/// of the separate L2I/L2D functions inside of CacheTag.  
/// 
/// While  a cache line is present in the cache, any core that touches it adds
/// itself to the bitmask.  Each new core that accesses the line moves the line
/// to the next bin in the histogram.  The reason for updating the histograms
/// here is that we want to track lines that never get evicted (there can be
/// quite a few).  It also consolidates all of the tracking logic to this one
/// function.
/// 
/// Updated to use threads instead of cores.   We could probably make this more
/// efficient by using std::bitset<MAX_THREADS_PER_CLUSTER> instead of a
/// uint64_t.  Resolves bug 180.
////////////////////////////////////////////////////////////////////////////////
inline void
CacheTag::_profile_core_access(int tid, uint64_t &thread_bitmask, uint64_t *histogram) 
{
  // Namespace contains histogram data.
  uint32_t ltidnum = tid % rigel::THREADS_PER_CLUSTER;
  // Check if core already accessed it
  if (thread_bitmask & (0x01UL << ltidnum)) return;
  // Count the current number of accessors
  int access_count = 0;
  for (int i = 0; i < rigel::THREADS_PER_CLUSTER; i++) {
    if (0 != (thread_bitmask & (0x01UL << i))) { access_count++; }
  }
  assert(access_count <= rigel::THREADS_PER_CLUSTER
    && "Cannot have more accessors than cores in a cluster.");

  // Atomically remove it 
  if (access_count > 0) histogram[access_count]--;
  histogram[access_count+1]++;
  // Add the core to the bitmask.
  thread_bitmask = thread_bitmask | (0x01UL << ltidnum);
}

#endif

    
