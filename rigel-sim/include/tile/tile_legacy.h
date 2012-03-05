////////////////////////////////////////////////////////////////////////////////
// tile_legacy.h
////////////////////////////////////////////////////////////////////////////////
//
//  Definition of the Tile class that holds multiple clusters.  It sits between
//  the global network above the Tile  and the clusters below it.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __TILE_LEGACY_H__
#define __TILE_LEGACY_H__

#include "sim.h"
#include "tilebase.h"

class BroadcastManager;
class TileInterconnectBase;
class GlobalNetworkBase;
class ClusterLegacy;
class ClusterBase;
class CommandLineArgs;

class TileLegacy: public TileBase
{
  public:

    /// constructor
    TileLegacy(rigel::ConstructionPayload cp);

	  /// destructor
    ~TileLegacy();

    /// PerCycle()
    int PerCycle();

    /// Dump()
    void Dump();

    /// returns count of halted sub-elements (clusters)
    int  halted();

    /// print heartbeat data
    void Heartbeat();

    /// pre-simulation initialization
    void PreSimInit();

    /// clean up / finish up at end of sim 
    void EndSim();

    //Checkpointing
    virtual void save_state() const;
    virtual void restore_state();

    /// accessors
    int                    getID()           const { return id; }
    TileInterconnectBase*  getInterconnect() const { return interconnect; }
    rigel::ClusterType** getClusters()     const { return (rigel::ClusterType**)clusters; }
    GlobalNetworkBase*     getGNet()         const { return gnet; }

  //FIXME these should be protected to modularize the simulator
  //protected:
    int id;

    ClusterLegacy **clusters;

    TileInterconnectBase *interconnect;

    GlobalNetworkBase *gnet;


};

#endif //#ifndef _TILE_H_


