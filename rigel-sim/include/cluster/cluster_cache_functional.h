#ifndef __CLUSTER_CACHE_FUNCTIONAL_H__
#define __CLUSTER_CACHE_FUNCTIONAL_H__

#include "cluster/cluster_cache_base.h"
#include "util/util.h"

namespace rigel {
	class ConstructionPayload;
	class ComponentCount;
}

//class Packet;


class ClusterCacheFunctional : public ClusterCacheBase {

  public:

    /// constructor
    ClusterCacheFunctional(rigel::ConstructionPayload cp);

    /// component interface functions
    int  PerCycle();
    void EndSim();
    void Dump();
    void Heartbeat();

    /// cluster cache interface
    int sendRequest(PacketPtr ptr);
    int recvResponse(PacketPtr ptr);

    uint32_t doLocalAtomic(PacketPtr p, int tid);

  private:
    std::vector<clustercache::LDLSTC_entry_t> LinkTable;

};

#endif
