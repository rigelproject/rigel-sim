#include "tlb.h"
#include <cfloat>              //For DBL_MAX
#include <cstddef>             //For size_t
#include <cstdio>              //For file*, fprintf, etc.
#include <cassert>             //For assert()
#include <string>              //For std::string
#include <inttypes.h>          //For PRIu64

#define D(x)
//#define D(x) x

namespace rigel {
  extern uint64_t CURR_CYCLE;
}

TLB::TLB(std::string _name, unsigned int _sets, unsigned int _ways, unsigned int _page_size_shift, TLBReplacementPolicy _repl) : 
         name(_name), sets(_sets), ways(_ways), page_size_shift(_page_size_shift), repl(_repl), valid(_sets*_ways),
         hits(0), misses(0), evict_dirty(0), evict_clean(0)
{
	d = new tlb_entry[sets*ways];
}

TLB::~TLB()
{
  delete[] d;
}

std::string TLB::getConfigString() const {
  char str[128];
  snprintf(str, 128, "%s, %u, %u, %zu",
           getReplacementPolicyString().c_str(), sets, ways, ((size_t)1 << page_size_shift));
  return std::string(str);
}

void TLB::access(addr_t addr) {
  addr_t page = pagenum(addr);
  size_t s = set(addr);
  size_t setStart = s*ways;
  int validEntry = valid.findNextSetInclusive(setStart);
  const int end = s*ways + ways;
  while(validEntry < end && validEntry != -1) {
    tlb_entry &e = d[validEntry];
		if(e.addr == page) { //Hit
			D(printf("HIT %08X\n", addr));
			hits++;
			switch(repl) {
				case LRU:
				case MRU:
					e.md64 = rigel::CURR_CYCLE;
          break;
        case LFU:
          e.md32_2++;
          break;
        default:
          assert(0 && "Unhandled TLB replacement policy");
			}
      return;
		}
    validEntry = valid.findNextSet(validEntry);
	}

  //Miss
  D(printf("MISS %08X -- ", addr));
  misses++;
  tlb_entry *victim = NULL;
  int emptyEntry = valid.findNextClearInclusive(setStart);
  if(emptyEntry < end && emptyEntry != -1) {
    D(printf("EMPTY\n"));
    victim = &(d[emptyEntry]);
    valid.set(emptyEntry);
  }
  else {
    tlb_entry *e = &(d[setStart]);
    tlb_entry *setEnd = e + ways;
    //Find victim
    switch(repl) {
      case LRU: {
        uint64_t min_cycle = UINT64_MAX;
        for(; e < setEnd; ++e) {
          if(e->md64 <= min_cycle) {
            min_cycle = e->md64;
            victim = e;
          }
        }
        break;
      }
      case MRU: {
        uint64_t max_cycle = UINT64_C(0);
        for(; e < setEnd; ++e) {
          if(e->md64 >= max_cycle) {
            max_cycle = e->md64;
            victim = e;
          }
        }
        break;
      }
      case LFU: {
        double min_frequency = DBL_MAX;
        for(; e < setEnd; ++e) {
          if(((double)(rigel::CURR_CYCLE - e->md32_1) / e->md32_2) <= min_frequency) {
            min_frequency = (double)(rigel::CURR_CYCLE - e->md32_1) / e->md32_2;
            victim = e;
          }
        }
        break;
      }
      default:
        assert(0 && "Unhandled TLB replacement policy");
    }
    D(printf("VICTIM %08X\n", victim->addr));
  }
  assert(victim != NULL && "Should have found a TLB eviction victim by now");
  //We have a victim to evict; install the new entry.
  victim->addr = page;
  switch(repl) {
    case LRU:
    case MRU:
      victim->md64 = rigel::CURR_CYCLE;
      break;
    case LFU:
      victim->md32_1 = (uint32_t) rigel::CURR_CYCLE;
      victim->md32_2 = 1;
      break;
    default:
      assert(0 && "Unhandled TLB replacement policy");
  }
}

unsigned int TLB::set(addr_t addr) const
{
	return pagenum(addr) % sets;
}

addr_t TLB::pagenum(addr_t addr) const
{
  return (addr >> page_size_shift);
}

std::string TLB::getReplacementPolicyString() const
{
  switch(repl) {
    case LRU:
      return std::string("LRU");
    case MRU:
      return std::string("MRU");
    case LFU:
      return std::string("LFU");
    default:
      assert(0 && "Unknown TLB Replacement Policy");
      break;
  }
  return std::string();
}

void TLB::dump(FILE *stream) const
{
  fprintf(stream, "TLB %s: %s, %"PRIu64" Accesses, %"PRIu64" Hits, %"PRIu64" Misses, %f%% hit rate\n",
              name.c_str(), getConfigString().c_str(), hits+misses, hits, misses, (((double)hits*100.0f)/(hits+misses)));
}

