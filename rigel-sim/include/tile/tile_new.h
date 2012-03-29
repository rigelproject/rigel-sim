#ifndef __TILE_NEW_H__
#define __TILE_NEW_H__

#include "sim.h"
#include "tile/tile_base.h"

// forward declarations
class TreeNetwork;

class Packet;
template <class T> class InPortBase;
template <class T> class OutPortBase;

class TileNew: public TileBase {

  public:

    // constructor
    TileNew(rigel::ConstructionPayload cp);

    /// required component interface

    int PerCycle();

    void Dump()      { printf("%s: unimplemented", __func__); }
    void Heartbeat();
    void EndSim();
    void PreSimInit();

    int  halted();

    //Checkpointing
    virtual void save_state() const;
    virtual void restore_state();

    // accessors
    rigel::ClusterType** getClusters()     const { return (rigel::ClusterType**)clusters; }

    InPortBase<Packet*>* get_inport()   { return from_gnet; }
    OutPortBase<Packet*>* get_outport() { return to_gnet;   }

  protected:

    int numclusters;
    int _halted;

    rigel::ClusterType **clusters;

    TreeNetwork *interconnect;

    /// memory side ports
    InPortBase<Packet*>*  from_gnet;
    OutPortBase<Packet*>* to_gnet;

};

#endif
