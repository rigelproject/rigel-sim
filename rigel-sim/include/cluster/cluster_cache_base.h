#ifndef __CLUSTER_CACHE_BASE_H__
#define __CLUSTER_CACHE_BASE_H__

#include "sim/componentbase.h"
#include "sim.h"
#include "define.h"

#include "packet/packet.h"

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

  protected:

    /// 
    rigel::GlobalBackingStoreType *mem_backing_store; 

};


#endif
