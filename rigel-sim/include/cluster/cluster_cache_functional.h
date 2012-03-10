#ifndef __CLUSTER_CACHE_FUNCTIONAL_H__
#define __CLUSTER_CACHE_FUNCTIONAL_H__

#include "cluster/cluster_cache_base.h"
#include "util/util.h"


// forward declarations
template<class T> class InPortBase;
template<class T> class OutPortBase;
//class Packet;

namespace rigel {
	class ConstructionPayload;
	class ComponentCount;
}


class ClusterCacheFunctional : public ClusterCacheBase {

  public:

    /// constructor
    ClusterCacheFunctional(rigel::ConstructionPayload cp);

    /// component interface functions
    int  PerCycle();
    void EndSim();
    void Dump();
    void Heartbeat();


    /// REMOVE ME: FIXME TODO HACK -- replace this with proper generic
    //connections
    InPortBase<Packet*>*  getInPort(int p)   { return ins[p]; }
    OutPortBase<Packet*>* getOutPort(int p)  { return outs[p]; }

  private:

    void doMemoryAccess(PacketPtr p);
    void doLocalAtomic(PacketPtr p);
    void doGlobalAtomic(PacketPtr p);
  
    void FunctionalMemoryRequest(Packet* p);

    std::vector<clustercache::LDLSTC_entry_t> LinkTable;

    std::vector< InPortBase<Packet*>* > ins;
    std::vector< OutPortBase<Packet*>* > outs;

};

#endif
