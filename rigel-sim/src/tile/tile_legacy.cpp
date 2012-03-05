////////////////////////////////////////////////////////////////////////////////
// tile_legacy.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of Tile module, the hierarchical module in Rigel contains
//  parameterized number of clusters and a Tile Interconnect.  Connects at upper
//  level to Global Interconnect
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Tile() constructor
////////////////////////////////////////////////////////////////////////////////
// DESCRIPTON
// Tile constructor creates a paramaterized number of Cluster instances
// and a TileInterconnect instance
// PARAMETERS
// int tileID
// GlobalNetworkBase *global_network : pointer to global network connection
//
// MemoryModelType *mem, BroadcastManager *bm : passed to cluster instances as args
//
////////////////////////////////////////////////////////////////////////////////
#include "global_network.h"  // for GlobalNetworkBase
#include "interconnect.h"   // for TileInterconnectBroadcast, etc
#include "sim.h"            // for CLUSTERS_PER_TILE, etc
#include "tile/tile_legacy.h"           // for Tile, etc
#include "cluster.h"        // for ClusterType (typedef'd to Cluster in sim.h)
#include "core.h"      
#include "core/regfile_legacy.h"      
#include "profile/profile.h"
#include "util/construction_payload.h"

TileLegacy::TileLegacy(rigel::ConstructionPayload cp) :
  TileBase(cp.parent, cp.component_name.empty() ? "TileLegacy" : cp.component_name.c_str()),
	id(cp.component_index), gnet(cp.global_network)
{
	cp.parent = this;
  cp.component_name.clear();
	
  // FIXME TODO: make this settable elsewhere
  //interconnect = new TileInterconnectBroadcast(cp);
  interconnect = new TileInterconnectNew(cp);
  //interconnect = new TileInterconnectIdeal(cp);
  
  cp.tile_network = interconnect;

  clusters = new ClusterLegacy *[rigel::CLUSTERS_PER_TILE];
  for(int i = 0; i < rigel::CLUSTERS_PER_TILE; i++) {
    //NOTE: The following line creates a new ClusterState for each
    //new cluster.  Due to the way our protobuf state is currently
    //organized, we rely on the Tile constructors being called in the same
    //order between checkpoint save and restore.  To fix this, we could do
    //some combination of making Tiles be explicitly represented entities in
    //the protobuf with their own ID numbers, and/or putting ID numbers in the
    //Cluster-level protobuf state.
    cp.cluster_state = cp.chip_state->add_clusters();
    cp.component_index = (rigel::CLUSTERS_PER_TILE*id + i);
    clusters[i] = new ClusterLegacy(cp);
  }

  gnet->addTileInterconnect(id, interconnect);
}

TileLegacy::~TileLegacy() {
	delete interconnect;
	for(int i = 0; i < rigel::CLUSTERS_PER_TILE; i++) {
		delete clusters[i];
	}
	delete[] clusters;
}

//FIXME This is not modular and pokes into cluster/core/thread state; redo once generic interfaces for PC,
//retired instr count, etc. are implemented.
void
TileLegacy::Heartbeat() {

  for (int c = 0; c < rigel::CLUSTERS_PER_TILE; c++) {
		std::cerr << "\n--> Tile " << id << "\n";
    getClusters()[c]->Heartbeat();
  } // end cluster

};


////////////////////////////////////////////////////////////////////////////////
/// PerCycle()
/////////////////////////////////////////////////////////////////////////////////
/// Tile level PerCycle method for simulation
/// calls PerCycle for each submodule of Tile
/// PARAMETERS
///  none
////////////////////////////////////////////////////////////////////////////////
int 
TileLegacy::PerCycle()
{

  interconnect->PerCycle();

  //  randomize order clusters are stepped
  // (round-robin rotate priority for cycling)
  // should not be necessary if we are fair and handle interactions properly...
  int idx = rigel::CURR_CYCLE % rigel::CLUSTERS_PER_TILE; 
  int halted_clusters = 0;
  for (int offset = 0; offset < rigel::CLUSTERS_PER_TILE; offset++) {
    halted_clusters += getClusters()[idx]->PerCycle();
    idx = (idx + 1) % rigel::CLUSTERS_PER_TILE;
  }

  return halted();
}

int
TileLegacy::halted() {
    int halted_clusters = 0;
    for(int cluster = 0; cluster < rigel::CLUSTERS_PER_TILE; cluster++) {
      halted_clusters += getClusters()[cluster]->halted();
    }
    return (halted_clusters == rigel::CLUSTERS_PER_TILE);
}

void TileLegacy::EndSim() {

    // FIXME: Should have global vector<profiler> in the flavor of DEBUG_x, not poke down into every cluster.
    for(int cluster_id = 0; cluster_id <rigel::CLUSTERS_PER_TILE; cluster_id++){
      clusters[cluster_id]->EndSim();
      if(rigel::ENABLE_TLB_TRACKING)
      {
      #if 0
        clusters[cluster_id]->dump_tlb();
        for(size_t i = 0; i < num_tlbs; i++) {
          uint64_t _ptiHits, _ptdHits, _ptuHits, _ptiMisses, _ptdMisses, _ptuMisses;
          uint64_t _pciHits, _pcdHits, _pcuHits, _pciMisses, _pcdMisses, _pcuMisses;
          clusters[cluster_id]->get_total_tlb_hits_misses(i, _ptiHits,_ptiMisses,_ptdHits,_ptdMisses,_ptuHits,_ptuMisses,
                                                                       _pciHits,_pciMisses,_pcdHits,_pcdMisses,_pcuHits,_pcuMisses);
          ptiHits[i] += _ptiHits;
          ptiMisses[i] += _ptiMisses;
          ptdHits[i] += _ptdHits;
          ptdMisses[i] += _ptdMisses;
          ptuHits[i] += _ptuHits;
          ptuMisses[i] += _ptuMisses;
          pciHits[i] += _pciHits;
          pciMisses[i] += _pciMisses;
          pcdHits[i] += _pcdHits;
          pcdMisses[i] += _pcdMisses;
          pcuHits[i] += _pcuHits;
          pcuMisses[i] += _pcuMisses;
        }
      #endif
      }
    } // end for
}


void 
TileLegacy::Dump() {
  assert(0 && "unimplemented!");
}

void 
TileLegacy::PreSimInit() {
  for(int cluster = 0; cluster < rigel::CLUSTERS_PER_TILE; cluster++) {
    getClusters()[cluster]->getProfiler()->start_sim();
  }
};

void TileLegacy::save_state() const {
  for(int i = 0; i < rigel::CLUSTERS_PER_TILE; i++)
    getClusters()[i]->save_state();
}

void TileLegacy::restore_state() { assert(0 && "Write me!"); }
