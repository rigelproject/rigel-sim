////////////////////////////////////////////////////////////////////////////////
// l1i.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the definition of the core-level ICache.  This file is
//  included from cache_model.h.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __CACHES__L1I_H__
#define __CACHES__L1I_H__

class L2ICache;

/* CLASS NAME: L1ICache
 * 
 * DESCRIPTON
 * Core-level instruction cache.  Sometimes referred to as the instruction
 * buffer.
 *
 */
class L1ICache : public CacheBase< 
  L1I_WAYS, L1I_SETS, LINESIZE, 
  L1I_OUTSTANDING_MISSES, CACHE_WRITE_THROUGH, L1I_EVICTION_BUFFER_SIZE
> 
{
  public: /* ACCESSORS */
    // We need to know what the next level of the cache is to do handshaking.
    void SetNextLevel(L2Cache *nextlevel) { NextLevel = nextlevel;  }
    void SetNextLevelI(L2ICache *nextleveli) { NextLevelI = nextleveli;  }

  private: /* ACCESSORS */
    // Clock tick for L1I.
    void PerCycle();
    // Insert the line into the L1I with all words valid.  Generates no
    // eviction since L1I is read-only.
    bool Fill(uint32_t addr);

  private: /* DATA */
    // We need to know what the next level of the hierarchy is so that we can
    // continually check to see if the line we requested last is available
    L2Cache *NextLevel;
    // FIXME this is a hack.  L2ICache and L2Cache should inherit from a common
    // base class which implements the Schedule() interface (the only thing this
    // is actually used for)
    L2ICache *NextLevelI; 

  friend class CacheModel;
  friend class L2Cache; //To let it peek into MSHRs to notify us when instr requests
                        //get back (if instrs aren't cached at the L2)
};

//FILL  : Used to allocate an entry in the cache
//INPUT : addr of data that needs to be added to cache
//OUTPUT: True if able to evict and add addr or addr already in cache
//        False if unable to evict and make room
inline bool 
L1ICache::Fill(uint32_t addr) 
{
  // Already in the cache
  if (IsValidLine(addr)) 
  { 
    int set = get_set(addr);
    int way = get_way_line(addr);
    TagArray[set][way].make_all_valid(addr);
    return true; 
  }
	
  // Find an empty line.  Eviction occurs if none are available
  bool stall;
  int set = get_set(addr);
  // warning, CacheBase uses a random eviction unless we override evict() for L1I
  int way = evict(addr, stall);
	
  if (stall) { return false; }
	
#ifdef DEBUG_CACHES
DEBUG_HEADER();
fprintf(stderr, "L1I: addr 0x:%08x set: %d, way: %d [[FILLING]]\n", addr, set, way);
#endif

  uint32_t dirty_mask = 0;
  uint32_t valid_mask = CACHE_ALL_WORD_MASK;

  TagArray[set][way].insert(addr, dirty_mask, valid_mask);

  return true;
}

#endif
