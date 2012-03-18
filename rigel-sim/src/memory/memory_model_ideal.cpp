////////////////////////////////////////////////////////////////////////////////
// memory_model_ideal.cpp
////////////////////////////////////////////////////////////////////////////////
//

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include <list>                         // for list
#include <set>                          // for set, etc
#include <vector>                       // for vector
#include "memory/address_mapping.h"  // for AddressMapping
#include "memory/dram.h"           // for CONTROLLERS
#include "memory/memory_model_ideal.h"
#include "caches_legacy/mshr_legacy.h"           // for MissHandlingEntry
#include "sim.h"            // for CURR_CYCLE, etc
#include "util/util.h"           // for CallbackInterface

#if 0
#define DEBUG_PRINT_MC_TRACE(c, x) do { \
  DEBUG_HEADER(); \
  fprintf(stderr, "MC_TRACE: ctrl: %d %s\n", c, x); \
} while (0);
#else
#define DEBUG_PRINT_MC_TRACE(c, x)
#endif

// constructor
IdealizedDRAMModel::IdealizedDRAMModel()
{
  config.max_bandwidth = 100000;
  config.max_latency = rigel::CMDLINE_IDEALIZED_DRAM_LATENCY;
  request_q.resize(rigel::DRAM::CONTROLLERS);
}

/// IdealizedDRAMModel::PerCycl()
///
/// Clock the ideal DRAM model and schedule any memory requests that come due
/// this cycle.
void
IdealizedDRAMModel::PerCycle()
{
  for (size_t ctrl = 0; ctrl < request_q.size(); ctrl++)
  {
    if (request_q[ctrl].size())
    {
      PendingRequest pReq = request_q[ctrl].front();
      // If the top request for this controller is not ready, go to the next
      // controller.
      if (pReq.ready_cycle > rigel::CURR_CYCLE) { continue; }
      // Schedule the request with the L3 cache and remove.
      pReq.req->callback(pReq.request_id, 0, pReq.addr);
      request_q[ctrl].pop_front();
    }
  }
}

/// IdealizedDRAMModel::Schedule()
/// Try to add request from G$ to the idealized memory model.  If we are limiting
/// requests per cycle, we could reject after N, which would put the limit on the
/// request side.  If we wanted to do DRAM reply side bandwidth limiting, we need
/// to handle it in PerCycle().
bool
IdealizedDRAMModel::Schedule(
  const std::set<uint32_t> addrs,
  int size,
  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
  CallbackInterface *requestingEntity,
  int requestIdentifier)
{
	std::set<uint32_t>::iterator iter;
  int offset = 0;
  for (iter = addrs.begin(); iter != addrs.end(); ++iter)
  {
    uint64_t ready_cycle = rigel::CURR_CYCLE + config.max_latency + offset++;
#if 0
    PendingRequest req = {
      // We assume only one address for now.
      *iter,
      requestIdentifier,
      requestingEntity,
      ready_cycle
    };
#endif
    PendingRequest req;
    req.addr = *iter;
    req.request_id = requestIdentifier;
    req.req = requestingEntity;
    req.ready_cycle = ready_cycle;
    //DEBUG_HEADER(); fprintf(stderr, "inserting addr: %08x id: %d\n", *iter, requestIdentifier);
    int ctrl = AddressMapping::GetController(*(addrs.begin()));
    request_q[ctrl].push_back(req);
  }

  return true;
}

