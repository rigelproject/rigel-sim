////////////////////////////////////////////////////////////////////////////////
// includes/caches/l2i.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the L2ICache class, used when separate L2 caches
//  are modelled.
//  This file is included from cache_model.h.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __CACHES__L2I_H__
#define __CACHES__L2I_H__

class ClusterLegacy;

/// L2ICache
/// Cluster-level instruction cache definition.  Enabled with
/// USE_L2I_CACHE_SEPARATE.  Otherwise, a unified approach is used and all
/// instructions are cached in the L2D (cluster) cache.
class L2ICache : public CacheBase< 
  L2I_WAYS, L2I_SETS, LINESIZE,
  L2I_OUTSTANDING_MISSES, CACHE_WRITE_THROUGH, L2I_EVICTION_BUFFER_SIZE
> 
{
  public:
    // Clock the cache.
    void PerCycle();
    // Fill the entire line associated with addr into the L2I.
    bool Fill(uint32_t addr);
    // Set the time in the future when the MSHR will be ready.
    void Schedule(int size, MissHandlingEntry<rigel::cache::LINESIZE> *MSHR);
    void SetNextLevel(L2Cache *nextlevel) { NextLevel = nextlevel;  }
    void ICacheProfilerTouch(uint32_t addr, int core);
  private:
    // We need to know what the next level of the hierarchy is so that we can
    // continually check to see if the line we requested last is available
    L2Cache *NextLevel;

    friend class L2Cache; //To let it peek into MSHRs to notify us when instr requests
    //get back (if instrs aren't cached at the L2D)
};

//FILL  : Used to allocate an entry in the cache
//INPUT : addr of data that needs to be added to cache
//OUTPUT: True if able to evict and add addr or addr already in cache
//        False if unable to evict and make room
inline bool 
L2ICache::Fill(uint32_t addr) 
{
  // Already in the cache
  if (IsValidLine(addr)) { 
    int set = get_set(addr);
    int way = get_way_line(addr);
    TagArray[set][way].make_all_valid(addr);
    return true; 
  }
  
  // Find an empty line.  Eviction occurs if none are available
  bool stall;
  int set = get_set(addr);
  int way = evict(addr, stall);
  if (stall) { return false; }
  
  #ifdef DEBUG_CACHES
  DEBUG_HEADER();
  fprintf(stderr, "L2I: addr 0x:%08x set: %d, way: %d [[FILLING]]\n", addr, set, way);
  #endif
  uint32_t dirty_mask = 0;
  uint32_t valid_mask = CACHE_ALL_WORD_MASK;
  TagArray[set][way].insert(addr, dirty_mask, valid_mask);

  return true;
}


#endif
