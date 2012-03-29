#ifndef __MEMORY_MODEL_IDEAL_H__
#define __MEMORY_MODEL_IDEAL_H__

#include "memory/memory_timing_base.h"
#include "sim/component_base.h"
#include "sim.h"
#include "caches_legacy/mshr_legacy.h"
#include "util/util.h"
#include <queue>
#include <set>
#include <vector>
#include <deque>

class DRAMController;
class DRAMChannelModel;
class LocalityTracker;

using namespace rigel::DRAM;
////////////////////////////////////////////////////////////////////////////////
// CLASS: MemoryTimingIdeal
////////////////////////////////////////////////////////////////////////////////
// This class tracks pending memory requests and allows the simulation to return
// the response given a fixed latency and/or bandwidth for the memory
// controller.  It avoids using the DRAM model.
class MemoryTimingIdeal : MemoryTimingBase
{
   public:
    MemoryTimingIdeal(ComponentBase *parent);
    // Add a request to the request queue.  Returns false if bandwidth is
    // exceeded.  The G$ will have to try again later.  This replaces the call
    // to DRAMController->Schedule() in MemoryModelGDDR4->Scedule().
    virtual bool Schedule(const std::set<uint32_t> addrs, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  CallbackInterface *requestingEntity, int requestIdentifier);

    virtual int  PerCycle();
    virtual void EndSim() { } ///< FIXME Push stats dumps into here
    virtual void PreSimInit() { } ///< FIXME Push most of constructor into here
    virtual void Dump() { } ///< FIXME Implement me
    virtual void Heartbeat() { } ///< FIXME Implement this as a brief version of Dump()

  private:
    // Structure for tracking pending requests to the idealized DRAM system.
    struct PendingRequest {
      uint32_t addr;
      int request_id;
      class CallbackInterface *req;
      // Calculated at insertion.  First cycle the request will be ready.
      uint64_t ready_cycle;
    };
    // Queue of pending requests.  Scanned each PerCycle to look for
    // scheduleable entries.  One queue per DRAM controller.
    std::vector< std::deque<PendingRequest> > request_q;
    // Configuration information for the memory model.
    struct {
      uint32_t max_bandwidth;
      uint32_t max_latency;
    } config;
};


#endif
