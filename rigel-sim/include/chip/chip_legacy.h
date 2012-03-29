#ifndef __CHIP_LEGACY_H__
#define __CHIP_LEGACY_H__

#include "util/construction_payload.h"
#include "sim/component_base.h"
#include "sim.h"
#include "util/checkpointable.h"

// forward declarations
class TileBase;
class GlobalCache;
class BroadcastManager;
class GlobalNetworkNew;

class ChipLegacy : public ComponentBase, public rigelsim::Checkpointable {

  public:
  
    ChipLegacy(rigel::ConstructionPayload cp);

    virtual ~ChipLegacy();

    int PerCycle();

    void init_tiles(rigel::ConstructionPayload &cp);
    void init_bm(rigel::ConstructionPayload &cp);
    void init_gnet(rigel::ConstructionPayload &cp);
    void init_global_cache(rigel::ConstructionPayload &cp);

    //BroadcastManager *bm()                    { return _bm;           }
    //GlobalCache     **global_cache()          { return _global_cache; }
    //GlobalNetworkNew *gnet()                  { return _gnet;         }
    //rigel::MemoryTimingType *memory_timing() { return _memory_timing; }
    TileBase** tiles()                       { return _tiles;        }
    
    /// dump object state 
    void Dump();
    
    /// for printing intermittent status data
    void Heartbeat();
    
    /// called at end of simulation
    void EndSim();

    virtual void save_state() const;

    virtual void restore_state();

    /// return true if all children are halted
    // TODO, don't rely up "TILES", but more general
    bool halted() { return (halted_ == rigel::NUM_TILES); }

  private:

    TileBase                        **_tiles;
    GlobalCache                     **_global_cache;
    BroadcastManager                *_bm;
    rigel::GlobalNetworkType      *_gnet;
    rigel::MemoryTimingType       *_memory_timing;
    rigel::GlobalBackingStoreType *_backing_store;
    rigelsim::ChipState             *_chip_state;

    int halted_;
};

#endif
