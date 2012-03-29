
////////////////////////////////////////////////////////////////////////////////
// memory_timing_dram.h
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifndef __MEMORY_TIMING_DRAM_H__
#define __MEMORY_TIMING_DRAM_H__

#include "sim.h"
#include "caches_legacy/mshr_legacy.h"
#include "util/util.h"
#include "memory/memory_timing_base.h"
//#include "memory/memory_model_ideal.h"
#include <queue>
#include <stdint.h>
#include <vector>
#include <set>
#include "tlb.h"

class ComponentBase;
class DRAMController;
class DRAMChannelModel;
class LocalityTracker;
//class TLB; //Forward declaration won't do; since we have a vector<TLB>, we need a destructor definition.

using namespace rigel::DRAM;


////////////////////////////////////////////////////////////////////////////////
// CLASS: MemoryTimingDRAM
////////////////////////////////////////////////////////////////////////////////
class MemoryTimingDRAM : MemoryTimingBase {
  public:
    //FIXME Implement destructor to properly free dynamically allocated memory/objects.

    /// Second parameter says whether or not DRAM controllers
    /// should check each Schedule()d request to make sure it
    /// does not have the same address as any currently-pending request.
    /// This is a sensible check for most systems of interest, since G$
    /// MSHRs should aggregate such duplicate requests into one and avoid
    /// multiple Schedule() calls, but for the DRAM simulator and other cases
    /// it makes sense to turn it off.
    MemoryTimingDRAM(ComponentBase *parent, bool _collisionChecking);
    void CollectStats();
    /// Try to schedule a request with the memory controller.
    bool Schedule(const std::set<uint32_t> addrs, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  CallbackInterface *requestingEntity, int requestIdentifier);
    /// Check address bit mapping scheme for aliasing.
    /// Should not be used in normal simulations, only for sanity-checking new address mappings
    bool alias_check();
    /// Dump information on the spatial and temporal locality of memory requests seen at the DRAM controllers.
    void dump_locality() const;

    virtual int  PerCycle();
    virtual void EndSim() { } ///< FIXME Push stats dumps into here
    virtual void PreSimInit() { } ///< FIXME Push most of constructor into here
    /// Dump current state of the memory controller for debugging purposes.
    virtual void Dump();
    virtual void Heartbeat() { } ///< FIXME Implement this as a brief version of Dump()
  private:
    DRAMController **Controllers;
    DRAMChannelModel **DRAMModelArray;

    FILE *FILE_memory_trace;
    LocalityTracker *aggregateLocalityTracker;

    uint64_t ***lines_serviced_buffer;
    uint64_t ***requests_serviced_buffer;
    FILE *FILE_load_balance_trace;
};


#endif
