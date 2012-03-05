////////////////////////////////////////////////////////////////////////////////
// global_network_ideal.cpp
////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>                     // for NULL
#include <list>
#include <string>                       // for string
#include <vector>                       // for vector, vector<>::iterator
#include "memory/address_mapping.h"  // for AddressMapping
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim, etc
#include "define.h"         // for ::IC_MSG_BCAST_INV_NOTIFY, etc
#include "memory/dram.h"           // for CONTROLLERS
#include "global_network.h"  // for GlobalNetworkIdeal
#include "icmsg.h"          // for ICMsg
#include "interconnect.h"   // for ChannelName, etc
#include "sim.h"            // for NUM_TILES, etc
#include "util/util.h"           // for SimTimer

void 
GlobalNetworkIdeal::init()
{
  using namespace rigel;
  // Add the network to the global debugging rig.
  GLOBAL_debug_sim.gnet.push_back(this);
  // Pointers to the tile interconnects.
  tiles = new TileInterconnectBase *[NUM_TILES];
  // Initialize all to NULL as a safety feature.  If we attempt to
  // double-initialize later, we will know.
  for (int i = 0; i < NUM_TILES; i++) { tiles[i] = NULL; }
  // Initialize the buffers used by the GCache.
  RequestBuffers.resize(DRAM::CONTROLLERS);
  for(std::vector<std::vector<std::list<ICMsg> > >::iterator it = RequestBuffers.begin();
      it != RequestBuffers.end(); ++it)
  {
    it->resize(VCN_LENGTH);
  }
  ReplyBuffers.resize(DRAM::CONTROLLERS);
  for(std::vector<std::vector<std::list<ICMsg> > >::iterator it = ReplyBuffers.begin();
      it != ReplyBuffers.end(); ++it)
  {
    it->resize(VCN_LENGTH);
  }
}

void
GlobalNetworkIdeal::helper_VCPerCycleRequestsToGCache(ChannelName vcn)
{
  using namespace rigel;
  for(int i = 0; i < NUM_TILES; i++)
  {
    std::list<ICMsg> from_buf = tiles[i]->RequestBuffers[vcn];
    std::list<ICMsg>::iterator req_msg = from_buf.begin();
    while (from_buf.end() != req_msg) {
      int dram_ctrl = AddressMapping::GetController(req_msg->get_addr());
      DEBUG_network("GNET REQ ", *req_msg);
      // Timer for network occupancy start at inject, stop here
      req_msg->timer_network_occupancy.StopTimer();
      // Timer for memory occupancy starts here, stops at GCache reply
      req_msg->timer_memory_wait.StartTimer();
      // Handle hit/miss profiling for request.
      helper_profile_gcache_stats(*req_msg);
      //Locality tracking, etc. NOTE: must be done before erase()
      insert_into_request_buffer(*req_msg);
      //Perform the switcheroo
      RequestBuffers[dram_ctrl][vcn].push_back(*req_msg);
      req_msg = from_buf.erase(req_msg);
    }
  }
}

void
GlobalNetworkIdeal::helper_VCPerCycleRepliesToCCache(ChannelName vcn)
{
  using namespace rigel;

  std::list<ICMsg>::iterator rep_msg;
  for (std::vector<std::vector<std::list<ICMsg> > >::iterator controllerIterator = ReplyBuffers.begin();
       controllerIterator != ReplyBuffers.end(); ++controllerIterator)
  {
    std::vector<std::list<ICMsg> > &thisControllersBuffers = *controllerIterator;
    std::list<ICMsg> &bufferOfInterest = thisControllersBuffers[vcn];
    rep_msg = bufferOfInterest.begin();
    // Regular messages.
    while (rep_msg != bufferOfInterest.end())
    {
      // Timer for network occupancy starts here, stops at ClusterRemove
      rep_msg->timer_memory_wait.StopTimer();
      rep_msg->timer_network_occupancy.StartTimer();
      // Insert the message downstream (into the tile).
      int tile_num = rep_msg->get_cluster()/CLUSTERS_PER_TILE;
      // Add latency for replies.
      rep_msg->set_ready_cycle(TILENET_GNET2CC_LAT_L1+TILENET_GNET2CC_LAT_L2);
      // Allow broadcast messages to move ahead of other messages.
      if (  IC_MSG_BCAST_UPDATE_NOTIFY ==rep_msg->get_type() 
         || IC_MSG_BCAST_INV_NOTIFY == rep_msg->get_type()) 
      {
        DEBUG_network("GNET REP (BCAST)", *rep_msg);
        tiles[tile_num]->RequestBuffers[vcn].push_front(*rep_msg);
      } else {
        DEBUG_network("GNET REP (NORM) ", *rep_msg);
        tiles[tile_num]->RequestBuffers[vcn].push_back(*rep_msg);
      }
      // Move on to the next message.
      rep_msg = bufferOfInterest.erase(rep_msg);
    }
  }
}

void
GlobalNetworkIdeal::PerCycle()
{
  using namespace rigel;

  // Clock the regular message queues from the clusters/tiles to the global cache.
  for(int i = 0; i < VCN_LENGTH; i++)
  {
    ChannelName vcn = static_cast<ChannelName>(i);
    helper_VCPerCycleRequestsToGCache(vcn);
  }

  // Clock the message queues leaving the global cache going to the cluster/tiles.
  for(int i = 0; i < VCN_LENGTH; i++)
  {
    ChannelName vcn = static_cast<ChannelName>(i);
    helper_VCPerCycleRepliesToCCache(vcn);
  }
}
