
/// Chip class
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
Chip::Chip(rigel::ConstructionPayload cp) :
  ComponentBase(cp.parent,
                cp.component_name.empty() ? "Chip" : cp.component_name.c_str()),
  _memory_timing(cp.memory_timing),
  _backing_store(cp.backing_store),
  _chip_state(cp.chip_state)
{
  cp.parent = this;
	cp.component_name.clear();
  init_bm(cp);           // initialize broadcast manager (FIXME: make conditional)
  init_global_cache(cp); // initialize the global cache
  init_gnet(cp);         // initialize the global tile-gcache interconnect
  init_tiles(cp);        // finally, init tiles (depends on some above)
}

// chip destructor
Chip::~Chip() {
  for (int i = 0; i < rigel::NUM_GCACHE_BANKS; i++) {
    delete _global_cache[i];
  }
  delete   _gnet;
  delete   _bm;
  for(int i = 0; i < rigel::NUM_TILES; i++) {
    delete _tiles[i];
  }
}

// chip PerCycle()
int 
Chip::PerCycle() { 

  // Clock each of the global cache banks.
  // Clock G$ banks in round-robin priority order
  // so that banks 0,1,2 do not starve bank 3 for a given channel.
  using namespace rigel::cache;
  for (unsigned int i = 0; i < rigel::DRAM::CONTROLLERS; i++)
  {
    for (int j = (rigel::CURR_CYCLE % GCACHE_BANKS_PER_MC), count = 0; count < GCACHE_BANKS_PER_MC;
        count++, j = (j + 1) % GCACHE_BANKS_PER_MC)
    {
      _global_cache[i*rigel::cache::GCACHE_BANKS_PER_MC + j]->PerCycle();
    }
  }

  // Clock the global network.
  _gnet->PerCycle();
  // Clock each of the tiles.
  // Due to the way per-instruction profiling (particularly
  // InstrLegacy.stats.cycles.stall_accounted_for) is implemented, the cores
  // (Cluster::PerCycle()) must be the last thing to be clocked.
  for(int t = 0; t < rigel::NUM_TILES; t++) { 
    _tiles[t]->PerCycle(); 
  }

  // Clock each cluster, check for halted clusters.  Each cluster clocks
  // each of its own cores recursively.
  // check if all tiles(clusters,cores) are halted
  int halted_count = 0;
  for(int tile = 0; tile < rigel::NUM_TILES; tile++) {
    halted_count += _tiles[tile]->halted();
  }

  halted_ = halted_count;

  return halted();  

}

/// dump object state 
void 
Chip::Dump() { };

/// for printing intermittent status data
void 
Chip::Heartbeat() { 
  // t: tile
  for(int t = 0; t < rigel::NUM_TILES; t++) {
    _tiles[t]->Heartbeat();
		std::cerr << "\n";
  } // end tile
}

/// called at end of simulation
void 
Chip::EndSim() { 
  using namespace rigel;
  // Dump any remaining BCASTs.  Any remaining at the end of a successful run is bad times..
  // FIXME: move this to a cleanup sim function
  _bm->dump();

  for(int t = 0; t < NUM_TILES; t++) {
    _tiles[t]->EndSim();
  }

  /// FIXME: move this out of the chip to somewhere better?
  /// FIXME: create dependence on ZeroTracker
  if(TRACK_BIT_PATTERNS) {
    GLOBAL_ZERO_TRACKER_PTR->report(std::cout);
  }

  for(int i = 0; i < NUM_GCACHE_BANKS; i++) {
    _global_cache[i]->gather_end_of_simulation_stats();
  }

  if(ENABLE_LOCALITY_TRACKING) {
    _gnet->dump_locality();
    for (int i = 0; i < NUM_GCACHE_BANKS; i++) {
      _global_cache[i]->dump_locality();
    }
    _memory_timing->dump_locality();
  }

#if CLUSTER_TYPE == 1

  if(ENABLE_TLB_TRACKING) {
		for(size_t i = 0; i < _global_cache[0]->num_tlbs; i++) {
			uint64_t hits=0, misses=0;
      for(int j = 0; j < NUM_GCACHE_BANKS; j++) {
				hits += _global_cache[j]->tlbs[i]->getHits();
				misses += _global_cache[j]->tlbs[i]->getMisses();
			}
			const char *cfg = _global_cache[0]->tlbs[i]->getConfigString().c_str();
			fprintf(stderr, "GC, %s, %"PRIu64", %"PRIu64", %f\n", cfg, hits, misses, (((double)hits*100.0f)/(hits+misses)));
    }
		//Disabling per-core ITLBs and UTLBs because they add the majority of the sim time overhead
		//and give almost no useful information (tiny ITLBs give huge hit rates, and UTLBs don't really make sense
		//at the core level anyway since they're on the critical path for fetch and L1D access.
		for(size_t i = 0; i < _tiles[0]->getClusters()[0]->GetCore(0).num_tlbs; i++) {
			//uint64_t pcim=0, pcih=0;
			uint64_t pcdm=0, pcdh=0;
			//uint64_t pcum=0, pcuh=0;
			//uint64_t ptim=0, ptih=0;
			uint64_t ptdm=0, ptdh=0;
			//uint64_t ptum=0, ptuh=0;
			for(int j = 0; j < NUM_TILES; j++) {
				for(int k = 0; k < CLUSTERS_PER_TILE; k++) {
					for(int l = 0; l < CORES_PER_CLUSTER; l++) {
						if(THREADS_PER_CORE > 1) {
						  for(int m = 0; m < THREADS_PER_CORE; m++) {
				  			//ptim += _tiles[j]->getClusters()[k]->GetCore(l).perThreadITLB[m][i]->getMisses();
			  				//ptih += _tiles[j]->getClusters()[k]->GetCore(l).perThreadITLB[m][i]->getHits();
		  					ptdm += _tiles[j]->getClusters()[k]->GetCore(l).perThreadDTLB[m][i]->getMisses();
		  					ptdh += _tiles[j]->getClusters()[k]->GetCore(l).perThreadDTLB[m][i]->getHits();
                //ptum += _tiles[j]->getClusters()[k]->GetCore(l).perThreadUTLB[m][i]->getMisses();
                //ptuh += _tiles[j]->getClusters()[k]->GetCore(l).perThreadUTLB[m][i]->getHits();
						  }
						}
            //pcim += _tiles[j]->getClusters()[k]->GetCore(l).aggregateITLB[i]->getMisses();
            //pcih += _tiles[j]->getClusters()[k]->GetCore(l).aggregateITLB[i]->getHits();
            pcdm += _tiles[j]->getClusters()[k]->GetCore(l).aggregateDTLB[i]->getMisses();
            pcdh += _tiles[j]->getClusters()[k]->GetCore(l).aggregateDTLB[i]->getHits();
            //pcum += _tiles[j]->getClusters()[k]->GetCore(l).aggregateUTLB[i]->getMisses();
            //pcuh += _tiles[j]->getClusters()[k]->GetCore(l).aggregateUTLB[i]->getHits();
          }
        }
      }
      const char *cfg = _tiles[0]->getClusters()[0]->GetCore(0).aggregateITLB[i]->getConfigString().c_str();
			if(THREADS_PER_CORE > 1) {
				//fprintf(stderr, "Per-Thread I, %s, %"PRIu64", %"PRIu64", %f\n", cfg, ptim, ptih, (((double)ptih*100.0f)/(ptih+ptim)));
				fprintf(stderr, "Per-Thread D, %s, %"PRIu64", %"PRIu64", %f\n", cfg, ptdm, ptdh, (((double)ptdh*100.0f)/(ptdh+ptdm)));
				//fprintf(stderr, "Per-Thread U, %s, %"PRIu64", %"PRIu64", %f\n", cfg, ptum, ptuh, (((double)ptuh*100.0f)/(ptuh+ptum)));
			}
			//fprintf(stderr, "Per-Core I, %s, %"PRIu64", %"PRIu64", %f\n", cfg, pcim, pcih, (((double)pcih*100.0f)/(pcih+pcim))); 
	    fprintf(stderr, "Per-Core D, %s, %"PRIu64", %"PRIu64", %f\n", cfg, pcdm, pcdh, (((double)pcdh*100.0f)/(pcdh+pcdm)));
	    //fprintf(stderr, "Per-Core U, %s, %"PRIu64", %"PRIu64", %f\n", cfg, pcum, pcuh, (((double)pcuh*100.0f)/(pcuh+pcum)));
    }
    for(size_t i = 0; i < _tiles[0]->getClusters()[0]->getCacheModel()->L2.num_tlbs; i++) {
			uint64_t hits=0, misses=0;
			for(int j = 0; j < NUM_TILES; j++) {
				for(int k = 0; k < CLUSTERS_PER_TILE; k++) {
		      misses += _tiles[j]->getClusters()[k]->getCacheModel()->L2.tlbs[i]->getMisses();
					hits += _tiles[j]->getClusters()[k]->getCacheModel()->L2.tlbs[i]->getHits();
				}
			}
			const char *cfg = _tiles[0]->getClusters()[0]->getCacheModel()->L2.tlbs[i]->getConfigString().c_str();
			fprintf(stderr, "Per-Cluster, %s, %"PRIu64", %"PRIu64", %f\n", cfg, misses, hits, (((double)hits*100.0f)/(hits+misses)));
		}

  }
#endif //#if CLUSTER_TYPE == 1
  // End TLB TRACKING
  
}

/// allocate and initialize tiles
void 
Chip::init_tiles(rigel::ConstructionPayload &cp) {
  _tiles = new TileBase *[rigel::NUM_TILES];

  for(int i = 0; i < rigel::NUM_TILES; i++) {
    cp.component_index = i;
    _tiles[i] = new rigel::TileType(cp);
  }
}

/// global cache initialization helper
void 
Chip::init_global_cache(rigel::ConstructionPayload &cp) {
  //FIXME Let # of G$ banks be set at command line.
  _global_cache = new GlobalCache *[rigel::NUM_GCACHE_BANKS];

  GLOBAL_CACHE_PTR = new GlobalCache *[rigel::NUM_GCACHE_BANKS];
  for (int i = 0; i < rigel::NUM_GCACHE_BANKS; i++) {
    cp.component_index = i;
    _global_cache[i] = new GlobalCache(cp);
    // Used for profiling
    GLOBAL_CACHE_PTR[i] = _global_cache[i];
  }
}

/// global network initialization helper
/// Instantiate global network, initialize w/ pointers to G$ banks
/// FIXME: make this a typedef or some other switch that is not based on  comments 
/// FIXME: really make this an abstract type with a factory?
void 
Chip::init_gnet(rigel::ConstructionPayload &cp) {
  _gnet = new rigel::GlobalNetworkType(cp);
  cp.global_network = _gnet; //Will be used by init_tiles() later
  // Give G$ banks pointers to global network
  // TODO: this should be attaching/connecting the two "components"
  for (int i = 0; i < rigel::NUM_GCACHE_BANKS; i++) {
    _global_cache[i]->setGlobalNetwork(_gnet);
  }
}

void 
Chip::init_bm(rigel::ConstructionPayload &cp) {
  _bm = new BroadcastManager(cp);
  cp.broadcast_manager = _bm;
}

void Chip::save_state() const {
  //for(int i = 0; i < rigel::NUM_GCACHE_BANKS; i++) {
  //  _global_cache[i]->save_state();
  //}
  //_bm->save_state();
  for(int i = 0; i < rigel::NUM_TILES; i++)
    _tiles[i]->save_state();
}

void Chip::restore_state() {
  //for(int i = 0; i < rigel::NUM_GCACHE_BANKS; i++) {
  //  _global_cache[i]->restore_state();
  //}
  //_bm->restore_state();
  for(int i = 0; i < rigel::NUM_TILES; i++)
    _tiles[i]->restore_state();
}
