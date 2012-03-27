#ifndef __CLUSTER_CACHE_FUNCTIONAL_H__
#define __CLUSTER_CACHE_FUNCTIONAL_H__

#include "cluster/cluster_cache_base.h"
#include "util/util.h"
#include <utility>
#include "util/fifo.h"
#include "port/port.h"


class ClusterCacheFunctional : public ClusterCacheBase {

  public:

    /// constructor
    ClusterCacheFunctional(
      rigel::ConstructionPayload cp,
      InPortBase<Packet*>* in,       ///< unused, the functional cache don't care!
      OutPortBase<Packet*>* out      ///< unused, the functional cache don't care!
    );

    /// component interface functions
    int  PerCycle();
    void EndSim();
    void Dump();
    void Heartbeat();

  protected:

    std::vector<clustercache::LDLSTC_entry_t> LinkTable;

  private: // we don't want these visible to inherited classes

    void doMemoryAccess(Packet* p);
    void doLocalAtomic(Packet* p);
    void doGlobalAtomic(Packet* p);
  
    port_status_t FunctionalMemoryRequest(Packet* p);

    /// for the functional model, we should use an UNSIZED queue to never run out of space
    fifo< std::pair<uint64_t,Packet*> > outpackets;

    int fixed_latency;

};

#endif
