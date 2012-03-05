
////////////////////////////////////////////////////////////////////////////////
// memory_model.h
////////////////////////////////////////////////////////////////////////////////
//
//  Contains the definition of classes related to the memory model, which tracks
//  the memory state, and the memory controller, which schedules requests to the
//  DRAM model.
////////////////////////////////////////////////////////////////////////////////

#ifndef __MEMORY_MODEL_IDEAL_H__
#define __MEMORY_MODEL_IDEAL_H__

#include "sim.h"
#include "mshr.h"
#include "util/util.h"
#include <queue>
#include <set>
#include <vector>
#include <list>

class DRAMController;
class DRAMChannelModel;
class LocalityTracker;

using namespace rigel::DRAM;
////////////////////////////////////////////////////////////////////////////////
// CLASS: IdealizedMemoryModel
////////////////////////////////////////////////////////////////////////////////
// This class tracks pending memory requests and allows the simulation to return
// the response given a fixed latency and/or bandwidth for the memory
// controller.  It avoids using the DRAM model.
class IdealizedDRAMModel
{
   public:
    IdealizedDRAMModel();
    // Add a request to the request queue.  Returns false if bandwidth is
    // exceeded.  The G$ will have to try again later.  This replaces the call
    // to DRAMController->Schedule() in MemoryModelGDDR4->Scedule().
    bool Schedule(const std::set<uint32_t> addrs, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  CallbackInterface *requestingEntity, int requestIdentifier);
    // Clock the idealized memory model.
    void PerCycle();

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
    std::vector< std::list<PendingRequest> > request_q;
    // Configuration information for the memory model.
    struct {
      uint32_t max_bandwidth;
      uint32_t max_latency;
    } config;
};


#endif
