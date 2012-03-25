#include "tile/tile_new.h"
#include "global_network.h"  // for GlobalNetworkBase
#include "interconnect.h"   // for TileInterconnectBroadcast, etc
#include "sim.h"            // for CLUSTERS_PER_TILE, etc
#include "cluster.h"        // for ClusterType (typedef'd to Cluster in sim.h)
#include "util/construction_payload.h"
#include "port/port.h"

/// constructor
TileNew::TileNew(
  rigel::ConstructionPayload cp
) :
  TileBase(cp.parent, 
           cp.component_name.empty() ? "TileNew" : cp.component_name.c_str()),
  numclusters(rigel::CLUSTERS_PER_TILE), // TODO: dynamically set
  _halted(0),
  //gnet(cp.global_network),
  from_gnet(NULL),
  to_gnet(NULL)
{

  cp.parent = this;
	cp.component_name.clear();

  // contruct ports with outside world
  // TODO: relocate construction?
  from_gnet = new InPortBase<Packet*>( PortName(name(), id(), "memside_in") );
  to_gnet   = new OutPortBase<Packet*>( PortName(name(), id(), "memside_out") );
  //< end contruction of ports

  // new interconnect
  interconnect = new TreeNetwork(cp, from_gnet, to_gnet);

  clusters = new rigel::ClusterType *[numclusters];
  for (int i = 0; i < numclusters; ++i) {
    // creates a new ClusterState for each new cluster
    cp.cluster_state = cp.chip_state->add_clusters();
    cp.component_index = numclusters * id() + i; // remove this, cluster ID assigned via ComponentCounter
    clusters[i] = new rigel::ClusterType(cp);
    // connect to tree network
    //interconnect->get_inport(i)->attach( clusters[i]->get_outport() );
    //clusters[i]->get_inport()->attach( interconnect->get_outport(i) );
  }

  // contruct ports with Clusters
  //< end contruction of ports

}

void
TileNew::Heartbeat() {
  for (int c = 0; c < rigel::CLUSTERS_PER_TILE; c++) {
    clusters[c]->Heartbeat();
  } // end cluster
}

/// called each cycle
/// update all owned components
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

  _halted = halted_clusters;

  return halted();

}

/// check if all internal components (clusters) are halted
int TileNew::halted() {
  return (_halted == numclusters);
}

/// save tile state
void TileNew::save_state() const {
  for(int i = 0; i < rigel::CLUSTERS_PER_TILE; i++)
    clusters[i]->save_state();
}

/// restore tile state
void TileNew::restore_state() { assert(0 && "Write me!"); }

/// post construction initialization to perform before simulation
void
TileNew::PreSimInit() {
  for(int cluster = 0; cluster < rigel::CLUSTERS_PER_TILE; cluster++) {
    getClusters()[cluster]->getProfiler()->start_sim();
  }
};

/// called at the end of the simulation
void
TileNew::EndSim() {
	for(int i = 0; i < numclusters; i++)
		clusters[i]->EndSim();
}
