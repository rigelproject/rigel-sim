#ifndef __TILE_NEW_H__
#define __TILE_NEW_H__

#include "sim.h"
#include "tile/tilebase.h"

// forward declarations
class BroadcastManager;
class TileInterconnectBase;
class GlobalNetworkBase;
class ClusterLegacy;
class CommandLineArgs;
class TreeNetwork;

class Packet;
template <class T> class InPortBase;
template <class T> class OutPortBase;

class TileNew: public TileBase {

  public:
    // constructor
    TileNew(rigel::ConstructionPayload cp);

    // PerCycle()
    int PerCycle();

    void Dump()      { assert(0&&"unimplemented"); }
    void Heartbeat();// { printf("%s unimplemented\n",__func__ ); }
    void EndSim();
    void PreSimInit();

    int  halted();

    //Checkpointing
    virtual void save_state() const;
    virtual void restore_state();

    // accessors
    //TileInterconnectBase*  getInterconnect() const { return interconnect; }
    rigel::ClusterType** getClusters()     const { return (rigel::ClusterType**)clusters; }
    //GlobalNetworkBase*     getGNet()         const { return gnet; }

  protected:

    int numclusters;
    int halted_;

    rigel::ClusterType **clusters;
    //TileInterconnectBase *interconnect;
    //GlobalNetworkBase    *gnet;

    TreeNetwork *interconnect;

    InPortBase<Packet*>*  from_gnet;
    OutPortBase<Packet*>* to_gnet;

    std::vector< InPortBase<Packet*>* >  cluster_ins;
    std::vector< OutPortBase<Packet*>* > cluster_outs;

};

#endif
