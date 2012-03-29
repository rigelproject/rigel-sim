#ifndef __CLUSTER_CACHE_BASE_H__
#define __CLUSTER_CACHE_BASE_H__

#include "sim/component_base.h"
#include "sim.h"
#include "define.h"

// forward declarations
template<class T> class InPortBase;
template<class T> class OutPortBase;
class Packet;

namespace rigel {
	class ConstructionPayload;
	class ComponentCount;
}

extern rigel::ComponentCount ClusterCacheBaseCount;

namespace clustercache {
  struct LDLSTC_entry_t {
    uint32_t addr;
    int size;
    bool valid;
  };
}

/// handle memory requests for the cluster
/// this includes the 
class ClusterCacheBase : public ComponentBase {

  public:

    /// constructor
    ClusterCacheBase(rigel::ConstructionPayload cp);

    /// destructor
    virtual  ~ClusterCacheBase() {}

    // required Component interface
    virtual int  PerCycle()  = 0;
    virtual void EndSim()    = 0;
    virtual void Dump()      = 0;
    virtual void Heartbeat() = 0;

    // extended required interface 
    //virtual int sendRequest(PacketPtr ptr) = 0;
    //virtual int recvResponse(PacketPtr ptr) = 0;

    /// REMOVE ME: FIXME TODO HACK -- replace this with proper generic
    //connections
    InPortBase<Packet*>*  getCoreSideInPort(int p)  { return coreside_ins[p];  }
    OutPortBase<Packet*>* getCoreSideOutPort(int p) { return coreside_outs[p]; }

  protected:

    int numcores;

    std::vector< InPortBase<Packet*>* > coreside_ins;
    std::vector< OutPortBase<Packet*>* > coreside_outs;

    /// 
    rigel::GlobalBackingStoreType *mem_backing_store; 

};


#endif
