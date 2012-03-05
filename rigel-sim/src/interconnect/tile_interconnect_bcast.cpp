#include "define.h"         // for icmsg_type_t
#include "interconnect.h"   // for TileInterconnectBroadcast, etc
#include "seqnum.h"         // for seq_num_t
#include "sim.h"            // for LINESIZE
#include "util/construction_payload.h"

using namespace rigel::interconnect;
using namespace rigel;

// Helper function to generate the cluster -> L1 mapping.
//static size_t GET_L1_ROUTER_ID(int cluster) { 
//  return (((cluster % CLUSTERS_PER_TILE)+(CLUSTERS_PER_L1_ROUTER-1))/CLUSTERS_PER_L1_ROUTER);
//}
//static size_t GET_NUM_L1_ROUTERS() { 
//  return ((CLUSTERS_PER_TILE)+(CLUSTERS_PER_L1_ROUTER-1)/CLUSTERS_PER_L1_ROUTER);
//}

// Constructor.  Parameter is the tile ID number.
TileInterconnectBroadcast::TileInterconnectBroadcast(rigel::ConstructionPayload cp) : 
  TileInterconnectNew(cp)
{
	cp.parent = this;
	cp.component_name.clear();
}

// Inject the packet.  Called from CCache model.  Returns false if unable to
// send the message this cycle.
bool 
TileInterconnectBroadcast::ClusterInject(
  int cluster_number,
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr,
  icmsg_type_t msg_type, 
  seq_num_t seq_num, 
  int seq_channel) 
{
  return TileInterconnectNew::ClusterInject(cluster_number,
    mshr, msg_type, seq_num, seq_channel);
}


// Remove a message from the network (pull from net)
bool 
TileInterconnectBroadcast::ClusterRemove(
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr)
{
  return TileInterconnectNew::ClusterRemove(cluster_num, mshr);
}

// See if a message is available.
bool 
TileInterconnectBroadcast::ClusterRemovePeek(
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr)
{
  return TileInterconnectNew::ClusterRemovePeek(cluster_num, mshr);
}

// Initialize the interconnect.
void 
TileInterconnectBroadcast::init()
{
}

// Clock the interconnect.
int 
TileInterconnectBroadcast::PerCycle()
{
  return TileInterconnectNew::PerCycle();
}

// Dump the interconnect state.
void
TileInterconnectBroadcast::dump()
{
  TileInterconnectNew::dump();
}
