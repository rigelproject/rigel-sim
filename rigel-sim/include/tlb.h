#ifndef __TLB_H__
#define __TLB_H__

#include <stdint.h>            //For uint*_t
#include <cstddef>             //For size_t
#include <string>              //For std::string
#include <cstdio>              //For FILE *

//#include "caches/cache_base.h" //For CacheAccess_t
#include "util/dynamic_bitset.h"    //For DynamicBitset

typedef uint32_t addr_t;
#define ADDR_MAX UINT32_MAX;

enum TLBReplacementPolicy {
	LRU,
	MRU,
	LFU
};

enum TLBStatInterval {
	TLBSI_TASK = 0,
	TLBSI_INTERVAL,
	TLBSI_PROGRAM,
	TLBSI_MAX
};

class TLB {
	public:
		typedef struct {
			addr_t addr;
			union {
				uint64_t md64;
				struct {
					uint32_t md32_1;
					uint32_t md32_2;
				};
				struct {
					uint16_t md16_1;
					uint16_t md16_2;
					uint16_t md16_3;
					uint16_t md16_4;
				};
			};
		} tlb_entry;

		TLB(std::string _name, unsigned int _sets, unsigned int _ways, unsigned int _page_size_shift, TLBReplacementPolicy _repl);
		~TLB();
		void access(addr_t addr);
		size_t getHits() const { return hits; }
		size_t getMisses() const { return misses; }
		std::string getConfigString() const;
    void dump(FILE *stream) const;
	private:
		/*
		const unsigned int (TLB::*find_victim_way)(unsigned int) const;
		unsigned int find_victim_lru(unsigned int set) const;
		unsigned int find_victim_mru(unsigned int set) const;
		unsigned int find_victim_lfu(unsigned int set) const;
		const unsigned int (TLB::*update_way)(uint64_t, unsigned int, bool) const;
		unsigned int find_victim_lru(uint32_t set) const;
		unsigned int find_victim_mru(uint32_t set) const;
		unsigned int find_victim_lfu(uint32_t set) const;
		*/
        addr_t pagenum(addr_t addr) const;
		unsigned int set(addr_t addr) const;

		std::string getReplacementPolicyString() const;

        std::string name;
		unsigned int sets;
		unsigned int ways;
		unsigned int page_size_shift;
		TLBReplacementPolicy repl;
		tlb_entry *d; //Allocated as a 1D array, accessed as 2D
		DynamicBitset valid;
		size_t hits;
		size_t misses;
		size_t evict_dirty;
		size_t evict_clean;
};

#endif //#ifndef __TLB_H__
