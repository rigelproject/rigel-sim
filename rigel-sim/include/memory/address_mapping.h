////////////////////////////////////////////////////////////////////////////////
// address_mapping.h
////////////////////////////////////////////////////////////////////////////////
//
// Contains the definition for the AddressMapping class, which is a centralized
// location for handling all mappings between addresses and DRAM locations, 
// cache banks and sets, etc.
// FIXME: It might not be a bad idea to move get_set() and get_way() from CacheBase
// over here as well, just so we have everything in one place.
////////////////////////////////////////////////////////////////////////////////

#ifndef __ADDRESS_MAPPING_H__
#define __ADDRESS_MAPPING_H__

#include <stdint.h>
#include <stdlib.h>

#include "sim.h"

class AddressMapping
{
  public:
    // There are 2 main non-trivial address mappings in RigelSim:
    // 1) Deciding the DRAM channel, rank, bank, and column for a given address
    // 2) Once the DRAM channel has been decided, decide which of the N global cache banks
    //    assigned to that channel will cache the address, and which set within that bank.
    // For now, as long as a single DRAM location and a single G$ location store a given address,
    // we don't have to worry about maintaining coherence amongst sets/banks in the global cache,
    // simplifying our implementation.  This is trivially accomplished by defining C++ functions
    // for each of these mappings, which by being deterministic are defined to be 1-to-1,
    // satisfying the above property.

    // An additional constraint imposed above the 1-to-1 mapping is that each
    // DRAM channel should handle a disjoint portion of the 32-bit address space,
    // and each DRAM channel should have a unique set of M global cache banks
    // used to cache its portion of the address space (and no addresses outside
    // it).  An additional locality optimization is that we map the DRAM
    // channel's address space to global cache banks in blocks of a few
    // contiguous cache lines (set by
    // rigel::cache::GCACHE_CONTIGUOUS_CACHE_LINES).  See sim.h for constraints
    // on GCACHE_CONTIGUOUS_CACHE_LINES.

    // The bit-order for mapping 1 (DRAM) is as follows, from MSBit to LSBit: Row
    // -> Rank -> Bank -> Controller/Channel -> Col That is, if a single core is
    // making unit-stride accesses through a large aligned chunk of memory, It
    // will stride through the columns of a single DRAM row, then across DRAM
    // channels in the system, Then across DRAM banks within a single rank, then
    // ranks within a channel, and then rows within a bank.  This ordering was
    // devised in order to provide decent load-balancing across the system
    // without a great deal of programmer effort.

    // Returns the DRAM column number for the given address
    inline static unsigned int GetCol(uint32_t addr) { 
      using namespace rigel::DRAM;
      using namespace rigel::cache;
      const uint32_t globalColNum = (addr/COLSIZE);
      const uint32_t colsPerBlock = (LINESIZE/COLSIZE)*GCACHE_CONTIGUOUS_LINES;
      const uint32_t colsPerWraparound = colsPerBlock*CONTROLLERS;
      uint32_t col = (globalColNum % colsPerBlock) + ((globalColNum / colsPerWraparound) * colsPerBlock);
      return (col % COLS);
    }
    // Returns the ID of the DRAM channel/controller
    // (0..rigel::DRAM::CONTROLLERS-1) which handles the given address

    // NOTE: The implementation of this function is intertwined with that of GetGCacheSet
    inline static unsigned int GetController(uint32_t addr) {
      using namespace rigel::DRAM;
      using namespace rigel::cache;
      uint32_t ctrl = (addr / (LINESIZE*GCACHE_CONTIGUOUS_LINES)) % CONTROLLERS;
      return ctrl;
    }
    // Returns the DRAM bank number corresponding to the given address
    inline static unsigned int GetBank(uint32_t addr) { 
      using namespace rigel::DRAM;
      using namespace rigel::cache;
      uint32_t globalColNum = addr/COLSIZE;
      uint32_t colsPerBlock = (LINESIZE/COLSIZE)*GCACHE_CONTIGUOUS_LINES;
      uint32_t blockNum = globalColNum / colsPerBlock;
      uint32_t ctrlBlockNum = (blockNum/CONTROLLERS);
      uint32_t ctrlColNum = ctrlBlockNum*colsPerBlock + (globalColNum % colsPerBlock);
      uint32_t bank = (ctrlColNum / COLS) % BANKS;
      return bank;
    }
    // Returns the DRAM rank number corresponding to the given address
    inline static unsigned int GetRank(uint32_t addr) {
      using namespace rigel::DRAM;
      using namespace rigel::cache;
      uint32_t globalColNum = addr/COLSIZE;
      uint32_t colsPerBlock = (LINESIZE/COLSIZE)*GCACHE_CONTIGUOUS_LINES;
      uint32_t blockNum = globalColNum / colsPerBlock;
      uint32_t ctrlBlockNum = (blockNum/CONTROLLERS);
      uint32_t ctrlColNum = ctrlBlockNum*colsPerBlock + (globalColNum % colsPerBlock);
      uint32_t rank = (ctrlColNum / (COLS*BANKS)) % RANKS;
      return rank;
    }
    // Returns the DRAM row number corresponding to the given address
    inline static unsigned int GetRow(uint32_t addr) {
      using namespace rigel::DRAM;
      using namespace rigel::cache;
      uint32_t globalColNum = addr/COLSIZE;
      uint32_t colsPerBlock = (LINESIZE/COLSIZE)*GCACHE_CONTIGUOUS_LINES;
      uint32_t blockNum = globalColNum / colsPerBlock;
      uint32_t ctrlBlockNum = (blockNum/CONTROLLERS);
      uint32_t ctrlColNum = ctrlBlockNum*colsPerBlock + (globalColNum % colsPerBlock);
      uint32_t row = (ctrlColNum / (COLS*BANKS*RANKS));
      return row;
    }
    // Returns the ID of the global cache bank which maps to the given address
    inline static unsigned int GetGCacheBank(uint32_t addr) 
    {
      using namespace rigel;
      using namespace rigel::DRAM;
      using namespace rigel::cache;
      return ((GetController(addr)*GCACHE_BANKS_PER_MC) + 
              ((addr / (LINESIZE * CONTROLLERS * GCACHE_CONTIGUOUS_LINES)) % GCACHE_BANKS_PER_MC));
    }

    ////////////////////////////////////////////////////////////////////////////////
    // GetGCacheSet(uint32_t addr, int numSets)
    ////////////////////////////////////////////////////////////////////////////////
    // takes addr, the address in question, and numSets, the number of sets *per* G$ bank
    //
    // returns the index of the global cache set to which the given address maps
    // for example, with GCACHE_CONTIGUOUS_LINES = 8 and LINESIZE = 32, we want to
    // return {addr[31:15],addr[11:10],addr[7:5]} % numSets.
    // with GCACHE_CONTIGUOUS_LINES = 8 and LINESIZE = 32, we want to return
    // {addr[31:15],addr[11],addr[7:5]} % numSets
    //
    // NOTE: This working is dependent on the current implementation of
    // GetController() (using the bits just above the DRAM column number)
    // If GetController() is modified, this needs to be modified too.
    // This implementation will grab the low bits just above the cache line offset, drop
    // the bits just above those used for selecting the cache bank within the tile,
    // then use 1 or more bits between these bank select bits and the bits which
    // choose the *tile* an address maps to.  Note that this means the
    // GetController() bits MINUS the low-order
    // rigel_log2(rigel::cache::MCS_PER_TILE) bits.  This function will only use
    // these "middle bits" if they exist for current configuration.  Finally, the
    // high-order bits of the set number will be those address bits above the
    // GetController() bits.  These 2 or 3 sets of bits are concatenated together,
    // modulo the number of sets in a global cache bank, to obtain the set number.
    inline static unsigned int GetGCacheSet(uint32_t addr, int numSets) {
      using namespace rigel;
      using namespace rigel::DRAM;
      using namespace rigel::cache;
      uint32_t cacheLineNumber = addr / LINESIZE;
      uint32_t blockNumber = cacheLineNumber / GCACHE_CONTIGUOUS_LINES;
      uint32_t lineWithinBlock = cacheLineNumber % GCACHE_CONTIGUOUS_LINES;
      uint32_t blockRoundNumber = blockNumber / (CONTROLLERS * GCACHE_BANKS_PER_MC);
      uint32_t set = ((GCACHE_CONTIGUOUS_LINES * blockRoundNumber) + lineWithinBlock) % numSets;
      return set;
    }
};

#endif //#ifndef __ADDRESS_MAPPING_H__
