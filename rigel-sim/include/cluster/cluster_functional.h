
#ifndef __CLUSTER_FUNCTIONAL_H__
#define __CLUSTER_FUNCTIONAL_H__

#include "sim.h" //For GlobalBackingStoreType
#include "cluster/cluster_base.h"
#include "core.h"
#include "cluster/cluster_cache.h"

class ClusterFunctional : public ClusterBase {

  public:

    ClusterFunctional(rigel::ConstructionPayload cp);

    ~ClusterFunctional();

    /// Component interface
    void Heartbeat();
    void Dump();
    void EndSim();
    int PerCycle();

    int halted() { return (halted_ == numcores); }

    virtual void save_state() const;
    virtual void restore_state();

    /// this cluster only

    /// replace with generic port connections
    InPortBase<Packet*>* get_inport() { return from_interconnect; }
    OutPortBase<Packet*>* get_outport() { return to_interconnect; }

  private:

    CoreFunctional** cores; /// array of cores

    ClusterCacheFunctional *ccache; /// cluster cache object

    int numcores;           /// cores in this cluster

    int halted_; /// halted cores (threads?)

    OutPortBase<Packet*>* to_interconnect;   ///< outport to the interconnect
    InPortBase<Packet*>*  from_interconnect; ///< inport from the interconnect

};

#endif 
