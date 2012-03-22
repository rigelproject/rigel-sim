
/// ChipTiled class
/// holds on-chip components

#include "sim.h"
#include "chip/chip.h"
#include "tile.h"
#include "broadcast_manager.h"  // for BroadcastManager
#include "memory/dram.h"           // for CONTROLLERS, RANKS, ROWS, etc
#include "caches_legacy/global_cache.h"  // for GlobalCache, etc
#include "global_network.h"
#include "memory_timing.h"      //For MemoryTimingType implementation
#include "util/value_tracker.h"
#include "caches_legacy/cache_model.h" //For poking into L2s for TLB aggregation :(
#include "util/construction_payload.h"

/// chip constructor
ChipTiled::ChipTiled(rigel::ConstructionPayload cp) :
  ComponentBase(cp.parent,
                cp.component_name.empty() ? "ChipTiled" : cp.component_name.c_str()),
  _halted(0),
  _tiles(NULL),
  //_global_cache(NULL),
  _gnet(NULL),
  _memory_timing(cp.memory_timing),
  _backing_store(cp.backing_store),
  _chip_state(cp.chip_state)
{
  cp.parent = this;
	cp.component_name.clear();

  GLOBAL_CACHE_PTR = NULL; // in case we never init

  init_global_cache(cp); // initialize the global cache
  init_gnet(cp);         // initialize the global tile-gcache interconnect
  init_tiles(cp);        // finally, init tiles (depends on some above)
}

/// chip destructor
/// free dynamic memory
ChipTiled::~ChipTiled() {
  if (_gnet) {
    delete   _gnet;
  }
  for(int i = 0; i < rigel::NUM_TILES; i++) {
    delete _tiles[i];
  }
}

/// chip PerCycle()
/// called each cycle, update each owned component
int 
ChipTiled::PerCycle() { 

  // Clock each of the global cache banks.
  // Clock G$ banks in round-robin priority order
  // so that banks 0,1,2 do not starve bank 3 for a given channel.
  using namespace rigel::cache;
  // if (_global_cache) {
  //   for (unsigned int i = 0; i < rigel::DRAM::CONTROLLERS; i++)
  //   {
  //     for (int j = (rigel::CURR_CYCLE % GCACHE_BANKS_PER_MC), count = 0; count < GCACHE_BANKS_PER_MC;
  //         count++, j = (j + 1) % GCACHE_BANKS_PER_MC)
  //     {
  //       _global_cache[i*rigel::cache::GCACHE_BANKS_PER_MC + j]->PerCycle();
  //     }
  //   }
  // }

  // Clock the global network.
  if (_gnet) {
    _gnet->PerCycle();
  }

  // Clock each of the tiles.
  for(int t = 0; t < rigel::NUM_TILES; t++) { 
    _tiles[t]->PerCycle(); 
  }

  // Clock each cluster, check for halted clusters.  
  // Each cluster clocks each of its own cores recursively.
  // check if all tiles(clusters,cores) are halted
  int halted_count = 0;
  for(int tile = 0; tile < rigel::NUM_TILES; tile++) {
    halted_count += _tiles[tile]->halted();
  }

  _halted = halted_count;

  return halted();  

}

/// dump object state 
void 
ChipTiled::Dump() { };

/// for printing intermittent status data
void 
ChipTiled::Heartbeat() { 
  for(int t = 0; t < rigel::NUM_TILES; t++) {
    _tiles[t]->Heartbeat();
		std::cerr << "\n";
  } 
}

/// called at end of simulation
void 
ChipTiled::EndSim() { 

  using namespace rigel;

  for(int t = 0; t < NUM_TILES; t++) {
    _tiles[t]->EndSim();
  }

  // if (_global_cache) {
  //   for(int i = 0; i < NUM_GCACHE_BANKS; i++) {
  //     _global_cache[i]->gather_end_of_simulation_stats();
  //   }
  // }

}

/// allocate and initialize tiles
void 
ChipTiled::init_tiles(rigel::ConstructionPayload &cp) {

  _tiles = new TileBase *[rigel::NUM_TILES];

  for (int i = 0; i < rigel::NUM_TILES; i++) {
    cp.component_index = i;
    _tiles[i] = new rigel::TileType(cp);
  }

}

/// global cache initialization helper
//FIXME Let # of G$ banks be set at command line.
void 
ChipTiled::init_global_cache(rigel::ConstructionPayload &cp) {
  // empty
}

/// global network initialization helper
/// Instantiate, initialize global network
void 
ChipTiled::init_gnet(rigel::ConstructionPayload &cp) {
  _gnet = new CrossBar(cp);
}

void ChipTiled::save_state() const {
}

void ChipTiled::restore_state() {
}
