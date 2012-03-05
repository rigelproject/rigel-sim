#include "tile/tile_new.h"
#include "global_network.h"  // for GlobalNetworkBase
#include "interconnect.h"   // for TileInterconnectBroadcast, etc
#include "sim.h"            // for CLUSTERS_PER_TILE, etc
#include "cluster.h"        // for ClusterType (typedef'd to Cluster in sim.h)
#include "util/construction_payload.h"

/// constructor
TileNew::TileNew(
  rigel::ConstructionPayload cp
) :
  TileBase(cp.parent, 
           cp.component_name.empty() ? "TileNew" : cp.component_name.c_str()),
  numclusters(rigel::CLUSTERS_PER_TILE), // TODO: dynamically set
  halted_(0),
  gnet(cp.global_network)
{

  cp.parent = this;
	cp.component_name.clear();
  // FIXME TODO: make this settable elsewhere
  interconnect = new TileInterconnectBroadcast(cp);
  //interconnect = new TileInterconnectNew(cp);
  //interconnect = new TileInterconnectIdeal(cp);
  gnet->addTileInterconnect(cp.component_index, interconnect);
  
  cp.tile_network = interconnect;

  clusters = new rigel::ClusterType *[numclusters];
  for (int i = 0; i < numclusters; ++i) {
    //NOTE: The following line creates a new ClusterState for each
    //new cluster.  Due to the way our protobuf state is currently
    //organized, we rely on the Tile constructors being called in the same
    //order between checkpoint save and restore.  To fix this, we could do
    //some combination of making Tiles be explicitly represented entities in
    //the protobuf with their own ID numbers, and/or putting ID numbers in the
    //Cluster-level protobuf state.
    cp.cluster_state = cp.chip_state->add_clusters();
    cp.component_index = numclusters * id() + i; // remove this, cluster ID assigned via ComponentCounter
    clusters[i] = new rigel::ClusterType(cp);
  }

}

int
TileNew::PerCycle() {

  if( halted() ) {
    return halted();
  }

  interconnect->PerCycle();

  int idx = rigel::CURR_CYCLE % rigel::CLUSTERS_PER_TILE; 

  int halted_clusters = 0;
  for (int offset = 0; offset < rigel::CLUSTERS_PER_TILE; offset++) {
    halted_clusters += clusters[idx]->PerCycle();
    idx = (idx + 1) % rigel::CLUSTERS_PER_TILE;
  }

  halted_ = halted_clusters;

  return halted();

}

/// check if all internal components (clusters) are halted
int TileNew::halted() {
  return (halted_ == numclusters);
}

void TileNew::save_state() const {
  for(int i = 0; i < rigel::CLUSTERS_PER_TILE; i++)
    clusters[i]->save_state();
}

void TileNew::restore_state() { assert(0 && "Write me!"); }
////////////////////////////////////////////////////////////////////////////////

void
TileNew::EndSim() {
}
