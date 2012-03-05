#include <inttypes.h>                   // for PRIx64
#include <stddef.h>                     // for size_t, NULL
#include <stdint.h>                     // for UINT64_MAX, uint64_t, etc
#include <stdio.h>                      // for fprintf, FILE
#include <list>                         // for list, _List_iterator, etc
#include <queue>                        // for priority_queue
#include <string>                       // for string
#include <vector>                       // for vector, std::vector<>::iterator
#include "memory/address_mapping.h"  // for AddressMapping
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim, etc
#include "define.h"
#include "memory/dram.h"           // for CONTROLLERS
#include "global_network.h"  // for GlobalNetworkNew
#include "icmsg.h"          // for ICMsg
#include "interconnect.h"   // for ChannelName, etc
#include "sim.h"            // for NUM_TILES, etc
#include "util/util.h"           // for SimTimer

//Turn this on to dump the number of messages routed up/down to each memory controller/tile every N cycles
//#define DUMP_ROUTE_COUNTS

using namespace rigel::interconnect;

void 
GlobalNetworkNew::init()
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

  routeCountsUp.resize(rigel::DRAM::CONTROLLERS);
  routeCountsDown.resize(rigel::NUM_TILES);
  //Reset all counts (may be superfluous)
  resetRouteCounts();
}

void GlobalNetworkNew::resetRouteCounts()
{
  for(size_t i = 0; i < routeCountsUp.size(); i++)
    routeCountsUp[i] = 0;
  for(size_t i = 0; i < routeCountsDown.size(); i++)
    routeCountsDown[i] = 0;
}

void GlobalNetworkNew::printRouteCounts(FILE *stream)
{
  fprintf(stream, "\n%08"PRIx64": Msgs routed up  : ", rigel::CURR_CYCLE);
  //Find total
  size_t total = 0;
  for(size_t i = 0; i < routeCountsUp.size(); i++) total += routeCountsUp[i];
  fprintf(stream, "%5zu -- ", total);
  for(size_t i = 0; i < routeCountsUp.size(); i++)
    fprintf(stream, "%5zu ", routeCountsUp[i]);
  fprintf(stream, "\n");
  fprintf(stream, "%08"PRIx64": Msgs routed down: ", rigel::CURR_CYCLE);
  //Find total
  total = 0;
  for(size_t i = 0; i < routeCountsUp.size(); i++) total += routeCountsUp[i];
  fprintf(stream, "%5zu -- ", total);
  for(size_t i = 0; i < routeCountsDown.size(); i++)
    fprintf(stream, "%5zu ", routeCountsDown[i]);
  fprintf(stream, "\n");
}

size_t GlobalNetworkNew::getNumUnpendedMessages(int queue_num, ChannelName vcn)
{
  size_t numUnpended = 0;
  for(std::list<ICMsg>::const_iterator it = ReplyBuffers[queue_num][vcn].begin(),
                                       end = ReplyBuffers[queue_num][vcn].end();
      it != end; ++it)
  {
    if(!it->check_gcache_pended())
      numUnpended++;
  }
  return numUnpended;
}

// The following two typedefs and the class tile_comp are used for creating
// priority queues ordered by the tile with the oldest request.  This allows us
// to cycle through only the tiles with pending requests and then do it in an
// age-ordered fashion.  It may be worthwhile considering moving this elsewhere
// as well.
struct tile_pq_elem_t
{
  public:
    tile_pq_elem_t(std::list<ICMsg> *_list, int resourceID) : list(_list), id(resourceID) { }
    std::list<ICMsg> *list;
    int id;
};

bool operator<(tile_pq_elem_t const & lhs, tile_pq_elem_t const & rhs)
{
  return(lhs.list->front().get_ready_cycle() > rhs.list->front().get_ready_cycle());
}

typedef std::priority_queue<tile_pq_elem_t> tile_pq_t;

void
GlobalNetworkNew::PerCycle()
{
  using namespace rigel;

  // Clock the regular message queues from the clusters/tiles to the global cache.
  // If the buffer is full, skip insertions.  We may overfill insertion in the
  // helper_VC() call, but we will assume there is reserve capacity to allow
  // the routes this cycle and then block the next cycle.  
  // 
  // Queues for holding the access ordering for the tiles.
  std::vector<tile_pq_t> arbitration_queues(VCN_LENGTH);
  tile_pq_t probe_resp_arb_q;
  tile_pq_t request_arb_q;
  for (int t = 0; t < NUM_TILES; t++) {
    for (int vc = 0; vc < VCN_LENGTH; vc++) {
      if(tiles[t]->RequestBuffers[vc].size() > 0) {
        tile_pq_elem_t temp(&tiles[t]->RequestBuffers[vc], t);
        arbitration_queues[vc].push(temp);
      }
    }
  }

  // Cycle through the requests starting with the tiles with the oldest messages.
  /*
  while (!arbitration_queues[VCN_PROBE].empty()) {
    tile_pq_elem_t tile = arbitration_queues[VCN_PROBE].top();
    helper_VCPerCycleRequestsToGCache(VCN_PROBE,                         
      *tile, MAX_ENTRIES_GCACHE_PROBE_RESP_BUFFER);
      arbitration_queues[VCN_PROBE].pop();
  }
  while (!arbitration_queues[VCN_DEFAULT].empty()) {
    tile_pq_elem_t tile = arbitration_queues[VCN_DEFAULT].top();
    helper_VCPerCycleRequestsToGCache(VCN_DEFAULT, 
      *tile, MAX_ENTRIES_GCACHE_REQUEST_BUFFER);
      arbitration_queues[VCN_DEFAULT].pop();
  }

  // Clock the message queues leaving the global cache going to the cluster/tiles.
  helper_VCPerCycleRepliesToCCache(VCN_PROBE);
  helper_VCPerCycleRepliesToCCache(VCN_DEFAULT);
  */

  //FIXME Need a better solution for iterating through all channels.
  //Iterating through enums is a dirty hack.
  //Probably a static global std::vector<ChannelName> is best.
  //FIXME Routing here is too aggressive.
  //Should respect crossbar resource limitations and only route 1 message to/from each MC
  //and to/from each tile each cycle.  Use DynamicBitset to track resource usage.
  for(int i = 0; i < VCN_LENGTH; i++)
  {
    ChannelName vcn = static_cast<ChannelName>(i);
    std::vector<int> mcUsed(rigel::DRAM::CONTROLLERS);
    for(size_t j = 0; j < mcUsed.size(); j++)
      mcUsed[j] = rigel::interconnect::MAX_GNET_UP_OUTPUT_MESSAGES_PER_CYCLE;
    while (!arbitration_queues[i].empty()) {
      tile_pq_elem_t const &tile = arbitration_queues[i].top();
      //FIXME should not be the same queue depth for all VCs
      helper_VCPerCycleRequestsToGCache(vcn, *(tile.list), mcUsed, MAX_ENTRIES_GCACHE_PROBE_RESP_BUFFER);
      arbitration_queues[i].pop();
    }
    //if(!mcUsed.allClear())
    //  printf("%08X %d requests routed up\n", rigel::CURR_CYCLE, mcUsed.getNumSetBits());
  }
  for(int i = 0; i < VCN_LENGTH; i++)
  {
    ChannelName vcn = static_cast<ChannelName>(i);
    helper_VCPerCycleRepliesToCCache(vcn);
  }

  //Print route counts every N cycles
#ifdef DUMP_ROUTE_COUNTS
  if(rigel::CURR_CYCLE % 5000 == 0)
  {
    printRouteCounts(stderr);
    resetRouteCounts();
  }
#endif
  
}

void
GlobalNetworkNew::helper_VCPerCycleRequestsToGCache(
  ChannelName vcn, 
  std::list<ICMsg> &buf,
  std::vector<int> &mcUsed,
  size_t MAX_ENTRIES)
{
  using namespace rigel;
  
  int reqs_this_cycle = rigel::interconnect::MAX_GNET_UP_INPUT_MESSAGES_PER_CYCLE;
  
  while (buf.size() > 0 && reqs_this_cycle > 0)
  {
    // Find the oldest ready message. 
    std::list<ICMsg>::iterator req_msg = buf.begin();
    std::list<ICMsg>::iterator oldest_req_msg;
    uint64_t oldest_req_time = UINT64_MAX;
    bool found_req = false;
    while (buf.end() != req_msg) {
      if (req_msg->get_ready_cycle() < oldest_req_time) {
        // Check to see if we have a blocked request queue.  If so, move on to
        // the next message.  This would require a per-DRAM bank queue in the
        // tile request buffers or an associative lookup on the tile buffer.
        // Neither are all that bad considering the sizes involved
        int dram_ctrl = AddressMapping::GetController(req_msg->get_addr());
        if (getNumUnpendedMessages(dram_ctrl, vcn) < MAX_ENTRIES && mcUsed[dram_ctrl] > 0) {
          found_req = true;
          oldest_req_time = req_msg->get_ready_cycle();
          oldest_req_msg = req_msg; 
        }
      }
      req_msg++;
    }
    // If none are ready, return.
    if (!found_req || !oldest_req_msg->is_ready_cycle()) { break; }

    int dram_ctrl = AddressMapping::GetController(oldest_req_msg->get_addr());
    DEBUG_network("GNET REQ ", *oldest_req_msg);
    // Timer for network occupancy start at inject, stop here
    oldest_req_msg->timer_network_occupancy.StopTimer();
    // Timer for memory occupancy starts here, stops at GCache reply
    oldest_req_msg->timer_memory_wait.StartTimer();
    // Handle hit/miss profiling for request.
    helper_profile_gcache_stats(*oldest_req_msg);
    // Locality tracking, etc.  NOTE: Must be done before erase
    insert_into_request_buffer(*oldest_req_msg);
    // Perform the switcheroo.
    RequestBuffers[dram_ctrl][vcn].push_back(*oldest_req_msg);
    oldest_req_msg = buf.erase(oldest_req_msg);

    //We used up a routing resource
    reqs_this_cycle--;
    mcUsed[dram_ctrl]--;
    routeCountsUp[dram_ctrl]++;
  }
}

void
GlobalNetworkNew::helper_VCPerCycleRepliesToCCache(ChannelName vcn)
{
  using namespace rigel;

  std::vector<int> tileUsed(rigel::NUM_TILES);
  for(size_t i = 0; i < tileUsed.size(); i++)
    tileUsed[i] = rigel::interconnect::MAX_GNET_DOWN_OUTPUT_MESSAGES_PER_CYCLE;
  // We give priority to memory controllers in sequence.
  uint32_t cnt = 0;
  for ( unsigned int dram_ctrl = CURR_CYCLE % DRAM::CONTROLLERS; 
        cnt < DRAM::CONTROLLERS; 
        dram_ctrl = (dram_ctrl + 1) % DRAM::CONTROLLERS, cnt++)
  {
    // Number of request we allow to move across the GNet for this tile.
    size_t reps_this_cycle = MAX_GNET_DOWN_INPUT_MESSAGES_PER_CYCLE;

    // Regular messages.
    while (ReplyBuffers[dram_ctrl][vcn].size() > 0 && reps_this_cycle > 0)
    {
      // Find the oldest ready message. 
      std::list<ICMsg>::iterator rep_msg = ReplyBuffers[dram_ctrl][vcn].begin();
      std::list<ICMsg>::iterator oldest_rep_msg;
      uint64_t oldest_rep_time = UINT64_MAX;
      bool found_req = false;
      while (ReplyBuffers[dram_ctrl][vcn].end() != rep_msg) {
        if (rep_msg->get_ready_cycle() < oldest_rep_time) {
          int tile_num = rep_msg->get_cluster()/CLUSTERS_PER_TILE;
          // If the tile's buffer is full, we skip the request.
          if (tiles[tile_num]->ReplyBuffers[vcn].size() < MAX_ENTRIES_TILE_REPLY_BUFFER && tileUsed[tile_num] > 0) {
            found_req = true;
            oldest_rep_time = rep_msg->get_ready_cycle();
            oldest_rep_msg = rep_msg; 
          }
        }
        rep_msg++;
      }
      // If none are ready, return.
      if (!found_req || !oldest_rep_msg->is_ready_cycle()) { break; }
      // Tile must have room for the request.
      int tile_num = oldest_rep_msg->get_cluster()/CLUSTERS_PER_TILE;
      // Insert the message downstream (into the tile).
      DEBUG_network("GNET REP (NORM) ", *oldest_rep_msg);
      // Timer for network occupancy starts here, stops at ClusterRemove
      oldest_rep_msg->timer_memory_wait.StopTimer();
      oldest_rep_msg->timer_network_occupancy.StartTimer();
      // Add latency for replies.
      oldest_rep_msg->set_ready_cycle(TILENET_GNET2CC_LAT_L2);
      // It is now safe to route the request.
      tiles[tile_num]->ReplyBuffers[vcn].push_back(*oldest_rep_msg);
      ReplyBuffers[dram_ctrl][vcn].erase(oldest_rep_msg);
      // We used up a reply.
      reps_this_cycle--;
      tileUsed[tile_num]--;
      routeCountsDown[tile_num]++;
    }
  }
  //if(!tileUsed.allClear())
  //  printf("%08X %d requests routed down\n", rigel::CURR_CYCLE, tileUsed.getNumSetBits());
}

	std::list<ICMsg>::iterator
GlobalNetworkNew::RequestBufferRemove(int queue_num,
                                      ChannelName vcn,
                                      std::list<ICMsg>::iterator msg)
{
  return RequestBuffers[queue_num][vcn].erase(msg);
}

bool
GlobalNetworkNew::ReplyBufferInject(int queue_num, ChannelName vcn, ICMsg & msg)
{
#if 0
  // TODO: We should support this, but the GCache does not cope with it yet.
  // Check that there is cpacity, otherwise fail.
  size_t MAX_BUF_SIZE = -1;
  switch (vcn) {
    case VCN_PROBE:    MAX_BUF_SIZE = MAX_ENTRIES_GCACHE_PROBE_REQUEST_BUFFER; break;
    case VCN_DEFAULT:  MAX_BUF_SIZE = MAX_ENTRIES_GCACHE_REPLY_BUFFER; break;
    default: assert(0 && "Unknown VCN");
  }
  if (ReplyBuffers[queue_num][vcn].size() >= MAX_BUF_SIZE) { return false; }
#endif
  msg.set_ready_cycle(GNET_GC2TILE_LAT);
  ReplyBuffers[queue_num][vcn].push_back(msg);
  return true;
}

bool
GlobalNetworkNew::BroadcastReplyInject(int queue_num, ChannelName vcn, ICMsg & msg)
{
  using namespace rigel;
  using namespace rigel::interconnect;

  // Check that there is capacity, otherwise fail.
  if (ReplyBuffers[queue_num][vcn].size() 
        >= MAX_ENTRIES_GCACHE_PROBE_REQUEST_BUFFER) { return false; }
  // Special case for when we have BCast network enabled to allow messages to be split.
  if (IC_MSG_SPLIT_BCAST_INV_REQ == msg.get_type() || IC_MSG_SPLIT_BCAST_SHR_REQ == msg.get_type()) 
  {
    // Insert one message per tile (use cluster zero of tile).  The tile will
    // then split to the clusters.
    for (int tile = 0; tile < NUM_TILES; tile++) {
      ICMsg new_msg = msg;
      new_msg.set_cluster_num(tile*CLUSTERS_PER_TILE);
      new_msg.set_ready_cycle(GNET_GC2TILE_LAT);
      ReplyBuffers[queue_num][VCN_BCAST].push_back(new_msg);
    }
  } else {
    msg.set_ready_cycle(GNET_GC2TILE_LAT);
    ReplyBuffers[queue_num][VCN_PROBE].push_back(msg);
  }
  return true;
}
