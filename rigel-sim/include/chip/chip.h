#ifndef __CHIP_H__
#define __CHIP_H__

#include "util/construction_payload.h"
#include "sim/componentbase.h"
#include "sim.h"
#include "util/checkpointable.h"

#include "interconnect.h"

/// a newer, more modular chip class
/// named ChipTiled to distinguish from ChipLegacy
/// (also, assumes the classic tiled Rigel top-level architecture)

// forward declarations
class TileBase;
class GlobalCache;
class BroadcastManager;
class GlobalNetworkNew;

class ChipTiled : public ComponentBase, public rigelsim::Checkpointable {

  public:
 
    // constructor 
    ChipTiled(rigel::ConstructionPayload cp);

    virtual ~ChipTiled();


    TileBase** tiles() { return _tiles; }
   
    /// Component interface 
    /// required interface 
    int PerCycle();

    /// dump object state 
    void Dump();
    
    /// for printing intermittent status data
    void Heartbeat();
    
    /// called at end of simulation
    void EndSim();

    /// return true if all children are halted
    // TODO, don't rely up "TILES", but more general
    bool halted() { return ( _halted == rigel::NUM_TILES); }

    // for checkpoint save, recovery
    virtual void save_state() const;
    virtual void restore_state();

  private:

    void init_tiles(rigel::ConstructionPayload &cp);
    void init_gnet(rigel::ConstructionPayload &cp);
    void init_global_cache(rigel::ConstructionPayload &cp);

    int _halted;

    TileBase                        **_tiles;
    //GlobalCache                     **_global_cache;
    CrossBar                        *_gnet;
    rigel::MemoryTimingType         *_memory_timing;
    rigel::GlobalBackingStoreType   *_backing_store;

    rigelsim::ChipState             *_chip_state;

};

#endif
