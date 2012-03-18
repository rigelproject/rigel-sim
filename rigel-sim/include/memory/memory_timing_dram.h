
////////////////////////////////////////////////////////////////////////////////
// memory_timing_dram.h
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifndef __MEMORY_TIMING_DRAM_H__
#define __MEMORY_TIMING_DRAM_H__

#include "sim.h"
#include "caches_legacy/mshr_legacy.h"
#include "util/util.h"
#include "memory/memory_model_ideal.h"
#include <queue>
#include <stdint.h>
#include <vector>
#include <set>
#include "tlb.h"

class DRAMController;
class DRAMChannelModel;
class LocalityTracker;
//class TLB; //Forward declaration won't do; since we have a vector<TLB>, we need a destructor definition.

using namespace rigel::DRAM;


////////////////////////////////////////////////////////////////////////////////
// CLASS: MemoryTimingDRAM
////////////////////////////////////////////////////////////////////////////////
class MemoryTimingDRAM {
  public:
    DRAMController **Controllers;
    DRAMChannelModel **DRAMModelArray;

    // Default constructor not implemented.
    //MemoryModelGDDR4();
    // TODO: Implement destructor to properly free dynamically allocated memory/objects.

    // First parameter says whether or not DRAM controllers
    // should check each Schedule()d request to make sure it
    // does not have the same address as any currently-pending request.
    // This is a sensible check for most systems of interest, since G$
    // MSHRs should aggregate such duplicate requests into one and avoid
    // multiple Schedule() calls, but for the DRAM simulator and other cases
    // it makes sense to turn it off.
    MemoryTimingDRAM(bool _collisionChecking);
    // Clock the memory controller.
    void PerCycle();
    // Collect stats
    void CollectStats();
    // Try to schedule the request with the memory controller.
    bool Schedule(const std::set<uint32_t> addrs, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  CallbackInterface *requestingEntity, int requestIdentifier);
    // Check address bit mapping scheme for aliasing
    bool alias_check();
    // Latency is determined by DRAM model, so latency is variable
    bool CheckWaitOnMemSched() { return true; }
    // Dump current state of the memory controller for debugging purposes.
    void dump();
    // Dump locality info
    void dump_locality() const;

  private:
    FILE *FILE_memory_trace;
    LocalityTracker *aggregateLocalityTracker;

    uint64_t ***lines_serviced_buffer;
    uint64_t ***requests_serviced_buffer;
    FILE *FILE_load_balance_trace;
};


#endif
