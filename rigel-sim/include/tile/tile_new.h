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

class TileNew: public TileBase {

  public:
    // constructor
    TileNew(rigel::ConstructionPayload cp);

    // PerCycle()
    int PerCycle();

    void Dump()      { assert(0&&"unimplemented"); }
    void Heartbeat() { printf("%s unimplemented\n",__func__ ); }
    void EndSim();
    void PreSimInit();

    int  halted();

    //Checkpointing
    virtual void save_state() const;
    virtual void restore_state();

    // accessors
    TileInterconnectBase*  getInterconnect() const { return interconnect; }
    rigel::ClusterType** getClusters()     const { return (rigel::ClusterType**)clusters; }
    GlobalNetworkBase*     getGNet()         const { return gnet; }

  protected:

    int numclusters;
    int halted_;

    rigel::ClusterType **clusters;
    TileInterconnectBase *interconnect;
    GlobalNetworkBase    *gnet;

};

#endif
