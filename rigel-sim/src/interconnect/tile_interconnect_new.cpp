#include <assert.h>                     // for assert
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, UINT64_MAX, etc
#include <stdio.h>                      // for fprintf, snprintf, stderr
#include <bitset>                       // for bitset
#include <list>                         // for list, _List_iterator, etc
#include <map>                          // for map, map<>::mapped_type, etc
#include <set>                          // for set, set<>::const_iterator, etc
#include <string>                       // for string
#include <vector>                       // for vector, vector<>::reference, etc
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"
#include "icmsg.h"          // for ICMsg
#include "interconnect.h"   // for TileInterconnectNew, etc
#include "mshr.h"           // for MissHandlingEntry, etc
#include "seqnum.h"         // for seq_num_t
#include "sim.h"            // for CLUSTERS_PER_TILE, etc
#include "util/util.h"           // for SimTimer
#include "util/construction_payload.h"

using namespace rigel::interconnect;
using namespace rigel;

// Helper function to generate the cluster -> L1 mapping.
static size_t GET_L1_ROUTER_ID(int cluster) { 
  return (((cluster % CLUSTERS_PER_TILE)+(CLUSTERS_PER_L1_ROUTER-1))/CLUSTERS_PER_L1_ROUTER);
}
static size_t GET_NUM_L1_ROUTERS() { 
  return ((CLUSTERS_PER_TILE)+(CLUSTERS_PER_L1_ROUTER-1)/CLUSTERS_PER_L1_ROUTER);
}

// Constructor.  Parameter is the tile ID number.
TileInterconnectNew::TileInterconnectNew(rigel::ConstructionPayload cp) : 
  TileInterconnectBase(cp.parent, cp.component_name.empty() ? "TileInterconnectNew" : cp.component_name.c_str()),
  tileID(cp.component_index)
{
	cp.parent = this;
	cp.component_name.clear();

  // Add to the global list of tile interconnects for debug printing.
  rigel::GLOBAL_debug_sim.tilenet.push_back(this);

  l1_request.resize(GET_NUM_L1_ROUTERS());
  for(size_t i = 0; i < GET_NUM_L1_ROUTERS(); i++)
    l1_request[i].resize(VCN_LENGTH);

  l1_reply.resize(GET_NUM_L1_ROUTERS());
  for(size_t i = 0; i < GET_NUM_L1_ROUTERS(); i++)
    l1_reply[i].resize(VCN_LENGTH);

  l2_request.resize(VCN_LENGTH);
  l2_reply.resize(VCN_LENGTH);

  stall_l1_request.resize(GET_NUM_L1_ROUTERS());
  for(size_t i = 0; i < GET_NUM_L1_ROUTERS(); i++)
    stall_l1_request[i].resize(VCN_LENGTH, false);
  
  stall_l1_reply.resize(GET_NUM_L1_ROUTERS());
  for(size_t i = 0; i < GET_NUM_L1_ROUTERS(); i++)
    stall_l1_reply[i].resize(VCN_LENGTH, false);

  stall_l2_request.resize(VCN_LENGTH, false);
  stall_l2_reply.resize(VCN_LENGTH, false);
}

// Inject the packet.  Called from CCache model.  Returns false if unable to
// send the message this cycle.
bool 
TileInterconnectNew::ClusterInject(
  int cluster_number,
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr,
  icmsg_type_t msg_type, 
  seq_num_t seq_num, 
  int seq_channel) 
{
  using namespace rigel;
  // Get the router number for the cluster.
  const size_t l1router = GET_L1_ROUTER_ID(cluster_number);
  ChannelName vc; 
  if (ICMsg::check_is_coherence(msg_type)) 
  {
    switch (msg_type) {
      case IC_MSG_SPLIT_BCAST_INV_REPLY:
      case IC_MSG_SPLIT_BCAST_SHR_REPLY:
      case IC_MSG_SPLIT_BCAST_OWNED_REPLY:
        vc = VCN_BCAST;
        break;
      default: 
        vc = VCN_PROBE;
        break;
    }
  } else {
    vc = VCN_DEFAULT;
  }
  // Check if we exerted back pressure at the end of last cycle.  If so, stall CCache.
  const bool & stall = stall_l1_request[l1router][vc];
  // The first level router is stalling.  
  if (stall) { return false; }
  uint32_t addr = *(mshr.get_pending_addrs_begin());
  // For now, we use message type instead of dirty bits to determine R/W @ L3$.
  std::bitset<rigel::cache::LINESIZE> dirty_bits;
  ICMsg request_msg(msg_type,
                    addr,
                    dirty_bits,
                    cluster_number,
                    mshr.GetCoreID(),
                    mshr.GetThreadID(),
                    mshr.GetPendingIndex(),
                    mshr.instr
                  );
  // Check to see if the address was valid in the cache at the time of the broadcast.
  if (mshr.get_bcast_resp_valid()) { request_msg.set_bcast_resp_valid(); }
  // Special case bulk prefetches.
  if (msg_type == IC_MSG_PREFETCH_BLOCK_GC_REQ || msg_type == IC_MSG_PREFETCH_BLOCK_CC_REQ) {
    request_msg.remove_all_addrs(); //Don't want a duplicate,
    //since ICMsgs have vectors instead of sets now and don't dedup themselves.
    request_msg.copy_addresses(mshr.get_pending_addrs_begin(), mshr.get_pending_addrs_end());
  }
  // Set the sequence numbers for the limited directory FSM.
  request_msg.set_seq(seq_channel, seq_num);
  // Timer for network occupancy starts here, stops when it hits the GCache
  request_msg.timer_network_occupancy.StartTimer();
  // Set time when message can move to the next level of the interconnect.
  request_msg.set_ready_cycle(TILENET_CC2GNET_LAT_L1);
  // Inject the message into the nework.
  l1_request[l1router][vc].push_back(request_msg);
  DEBUG_network("CCACHE INJECT", request_msg);
  return true;
}

// We cycle through each channel giving priority to each VC in a RR fashion.
// If there are no messages to service this cycle from a VC with prioirty, we
// skip it and try the next one. 
bool 
TileInterconnectNew::ClusterRemoveBase(
  bool erase_msg,
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr)
{
  const size_t l1router = GET_L1_ROUTER_ID(cluster_num);
  // Rotate priority among VCs every cycle.
  for (int i = rigel::CURR_CYCLE % VCN_LENGTH, count = 0; 
        count < VCN_LENGTH; 
        count++, i = (i + 1) % VCN_LENGTH)
  {
    ChannelName vcn = static_cast<ChannelName>(i);
    if (ClusterRemoveBaseNetSelect(erase_msg, cluster_num, mshr, l1_reply[l1router][vcn])) { 
      return true;
    }
  }
  return false;
}

// Remove a message from the network (pull from net)
bool 
TileInterconnectNew::ClusterRemove(
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr)
{
  mshr.clear(false); //the 'false' flag is an "i know what i'm doing, just clear it"
  return ClusterRemoveBase(true, cluster_num, mshr);
}

// See if a message is available.
bool 
TileInterconnectNew::ClusterRemovePeek(
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr)
{
  //NOTE see note in ClusterRemove()
  mshr.addrs.clear();
  mshr.ready_lines.clear();
  // Has to be after we clear the two address sets
  mshr.clear(); 
  return ClusterRemoveBase(false, cluster_num, mshr);
}

//NOTE: As part of the contract for this function, we cannot call
//set() on the MSHR unless we are sure we are going to return true.
//Otherwise, as we are cycling through VCs trying to find a channel
//, we may end up calling set() multiple times, which is a no-no.
bool 
TileInterconnectNew::ClusterRemoveBaseNetSelect(
  bool erase_msg,
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr,
  std::list<ICMsg> &chan)
{
  if (chan.size() > 0) 
  {
    // Search for a message destined for this cluster.
    std::list<ICMsg>::iterator iter = chan.begin();
    // Find the first request that matches this cluster and is ready.
    while (iter != chan.end() ) {
      if (iter->get_cluster() != cluster_num) { ++iter; }
      else if (!iter->is_ready_cycle()) { ++iter; }
      else { break; }
    }
    // We hit the end of list, return.
    if (iter == chan.end()) { return false; }

    const ICMsg &return_msg = *iter;
    // Setup the MSHR for the reply and then return
    uint32_t addr = return_msg.get_addr();
    // Since vlatency is set to false here, no need to ack/sched
    assert(return_msg.get_type() != IC_MSG_NULL);
    mshr.set(addr, 0, true, //Set to variable latency.  shouldnt matter (this is a dummy MSHR)
      return_msg.get_type(),
      return_msg.GetCoreID(),
      return_msg.GetPendingIndex(),
      return_msg.GetThreadID()
    );
    // Set the logical channel number for the message.
    mshr.set_seq(return_msg.get_seq_channel(), return_msg.get_seq_number());
    // If the directory says we are incoherent, propogate it down to the
    // cluster cache.
    if (return_msg.get_incoherent()) { mshr.set_incoherent(); }
    // Debug messages...
    if (erase_msg)  DEBUG_network("CCACHE REMOVE", *iter);
    if (!erase_msg) DEBUG_network("CCACHE REMOVE (PEEK)", *iter);
    // On actual remove, handle accounting for statistics tracking.
    if (erase_msg) { helper_profile_net_msg_remove(*iter); }
    // XXX: This is the actual remove, so remove the message from the list.
    if (erase_msg) { chan.erase(iter); }
    return true;
  }
  return false;
}

// Initialize the interconnect.
void 
TileInterconnectNew::init()
{
}

static void
helper_route_oldest_requests(const int MAX_REQUESTS, std::list<ICMsg> &from, std::list<ICMsg> &to)
{
  // Number of requests to allow per cycle.
  int num_req_this_cycle = MAX_REQUESTS;
  // Find the oldest entry and try to route it
  while (from.size() > 0 && num_req_this_cycle > 0)
  {
    std::list<ICMsg>::iterator iter = from.begin();
    std::list<ICMsg>::iterator oldest_iter;
    uint64_t oldest_time = UINT64_MAX;
    while (iter != from.end()) {
      if (oldest_time > iter->get_ready_cycle()) {
        oldest_time = iter->get_ready_cycle();
        oldest_iter = iter;
      }
      iter++;
    }
    // If none are ready, skip and go to next L1Router
    if (!oldest_iter->is_ready_cycle()) { return; }
    // Route request.
    to.push_back(*oldest_iter);
    from.erase(oldest_iter);
    num_req_this_cycle--;
  }
}

static void
helper_L1_to_L2_routing(
  std::vector<std::list<ICMsg> >&l2to, 
  std::vector<std::vector<std::list<ICMsg> > > &l1from, 
  ChannelName vcn)
{
  std::vector<std::vector<std::list<ICMsg> > >::iterator l1r = l1from.begin();
  for ( ; l1r != l1from.end(); ++l1r)
  {
    helper_route_oldest_requests(L1_TO_L2_REQUESTS_PER_CYCLE, (*l1r)[vcn], l2to[vcn]);
  }
}

static void
helper_L2_to_GC(std::list<ICMsg> &from, std::list<ICMsg> &to)
{
  helper_route_oldest_requests(L2_TO_GNET_REQUESTS_PER_CYCLE, from, to);
}

static void
helper_L2_to_L1_routing(
  std::vector<std::list<ICMsg> > &l2from, 
  std::vector<std::vector<std::list<ICMsg> > >&l1to, 
  ChannelName vcn)
{
  std::list<ICMsg>::iterator req_iter = l2from[vcn].begin();
  while (l2from[vcn].end() != req_iter) {
    if (req_iter->is_ready_cycle()) {
      const size_t l1router = GET_L1_ROUTER_ID(req_iter->get_cluster());
      DEBUG_network("TILE G2C L1 ROUTE", *req_iter);
      l1to[l1router][vcn].push_back(*req_iter);
      req_iter = l2from[vcn].erase(req_iter);
    } else { ++req_iter; }
  }
}

static void 
helper_GC_to_L2(std::list<ICMsg> &from, std::list<ICMsg> &to)
{
  std::list<ICMsg>::iterator probe_req_iter = from.begin();
  while (from.end() != probe_req_iter) {
    if (probe_req_iter->is_ready_cycle()) {
      probe_req_iter->set_type(ICMsg::convert(probe_req_iter->get_type()));
      DEBUG_network("TILE G2C L2 ROUTE", *probe_req_iter);
      to.push_back(*probe_req_iter);
      probe_req_iter = from.erase(probe_req_iter);
    } else { probe_req_iter++; }
  }
}

void TileInterconnectNew::helper_HandleReplies(ChannelName vcn)
{
  assert(VCN_BCAST != vcn && "Not for BCASTS.");
  helper_L2_to_L1_routing(l2_reply, l1_reply, vcn);
  // Clocking L2 Routers.  Only let message move on if ready.
  if (!stall_l2_reply[vcn]) {
    helper_GC_to_L2(ReplyBuffers[vcn], l2_reply[vcn]);
  }
}

void TileInterconnectNew::helper_HandleRepliesBCast(ChannelName vcn)
{
  const size_t MAX_TILE_BCASTS_PER_CYCLE = 4;
  const size_t MAX_L1_BCAST_ENTRIES      = 4*CLUSTERS_PER_L1_ROUTER;
  size_t bcasts_this_cycle = MAX_TILE_BCASTS_PER_CYCLE;
  // TODO: We may want to shunt these to a separate VC than the probe vcn.  For
  // now we just dump them into the probe VCN.   This violates the Ghostbusters
  // Principle: Never cross the streams. 
  const ChannelName cluster_vcn = VCN_PROBE;
  assert(VCN_BCAST == vcn && "Only for BCASTS.");
  while (!ReplyBuffers[vcn].empty() && (bcasts_this_cycle-- > 0)) 
  {
    // The message *must* be a splitting broadcast for now.
    assert((ReplyBuffers[vcn].front().get_type() == IC_MSG_SPLIT_BCAST_INV_REQ ||
     ReplyBuffers[vcn].front().get_type() == IC_MSG_SPLIT_BCAST_SHR_REQ) && 
      "Must be splitting BCAST!");
    // For broadcasts, we assume wired logic so we blast out the messages to each
    // cluster in one shot if possible.
    for (size_t i = 0; i < CLUSTERS_PER_TILE/CLUSTERS_PER_L1_ROUTER; i++) {
      // Block on L1 Router buffer overflow.  Forces bcasts to go in waves.
      if (l1_reply[i][cluster_vcn].size() >= MAX_L1_BCAST_ENTRIES) { return; }
    }
    // Split the messages!
    for (size_t cid_off = 0; cid_off < (size_t)CLUSTERS_PER_TILE; cid_off++) {
      size_t cid = (CLUSTERS_PER_TILE*tileID) + cid_off;
      // Skip messages on the skip list.
      if (0 == ReplyBuffers[vcn].front().get_bcast_skip_list().count(cid)) 
      {
        ICMsg new_msg = ReplyBuffers[vcn].front();
        new_msg.set_cluster_num(cid);
        // We need to set up the sequence number/channel for the request.
        new_msg.set_seq(ReplyBuffers[vcn].front().get_bcast_seq_channel(cid),
                        ReplyBuffers[vcn].front().get_bcast_seq_number(cid));
        l1_reply[GET_L1_ROUTER_ID(cid)][cluster_vcn].push_back(new_msg);
      }
    } 
    // Add to the list of waiting entries for the tile on the collective network.
    if (collective_wait_list.count(ReplyBuffers[vcn].front().get_addr()) > 0) {
      GLOBAL_debug_sim.dump_all();
      DEBUG_HEADER();
      ReplyBuffers[vcn].front().dump(__FILE__, __LINE__);
      assert(0 && "Should not have a broadcast to an address with an outstanding broadcast!");
    }
    // Build the list of clusters to wait on, skipping those in the skip list.
    std::set<int> my_wait_list;
    std::set<int> my_skip_list;
    for (int i = 0; i < CLUSTERS_PER_TILE; i++) {
      const int cl = tileID*CLUSTERS_PER_TILE + i;
      (ReplyBuffers[vcn].front().get_bcast_skip_list().count(cl) == 0) ?
        my_wait_list.insert(cl) : // Wait for this cluster before sending BCAST reply.
        my_skip_list.insert(cl);  // This cluster is not part of the BCAST.
    }
    // Insert the collective entry to wait on.
    collective_wait_list[ReplyBuffers[vcn].front().get_addr()] = my_wait_list;
    collective_skip_list[ReplyBuffers[vcn].front().get_addr()] = my_skip_list;
    // Initially, we are not the data source.
    collective_data_source_list[ReplyBuffers[vcn].front().get_addr()] = false;
    // Remove the bcast from the GNET buffer.
    ReplyBuffers[vcn].pop_front();
  }
}

//FIXME bufsize should be encapsulated with the buffer itself, rather than a global passed around
void TileInterconnectNew::helper_HandleRequests(ChannelName vcn, size_t bufsize)
{
  // Only let L2 --> GNet occur if there is room.
  if (VCN_BCAST == vcn)
  {
    // Hook BCAST replies.   We use PROBE for the upstream for now (not VCN_BCAST).
    if (RequestBuffers[VCN_PROBE].size() < bufsize) 
    {
      size_t max_routes_this_cycle = L2_TO_GNET_REQUESTS_PER_CYCLE;
      while (l2_request[VCN_BCAST].size() > 0 && max_routes_this_cycle > 0)
      {
        // First we must find the oldest request to service first.
        std::list<ICMsg>::iterator iter = l2_request[VCN_BCAST].begin();
        std::list<ICMsg>::iterator oldest_iter;
        uint64_t oldest_time = UINT64_MAX;
        while (iter != l2_request[VCN_BCAST].end()) {
          if (oldest_time > iter->get_ready_cycle()) {
            oldest_time = iter->get_ready_cycle();
            oldest_iter = iter;
          }
          iter++;
        }
        // If none are ready, skip and go to next L1Router
        if (!oldest_iter->is_ready_cycle()) { return; }

        uint32_t addr = oldest_iter->get_addr();
        int cluster   = oldest_iter->get_cluster();
        // Sanity checks...
        if (0 == collective_wait_list.count(addr)) {
          GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER(); fprintf(stderr, "Addr: %08x cluster: %d\n", addr, cluster);
          assert(0 && "BCAST response to untracked address.");
        }
        if (0 == collective_wait_list[addr].count(cluster) ) {
          GLOBAL_debug_sim.dump_all();
          DEBUG_HEADER(); fprintf(stderr, "Addr: %08x cluster: %d\n", addr, cluster);
          assert(0 && "BCAST response to unexpected cluster.");
        }
        // We are no longer waiting on this cluster.
        collective_wait_list[addr].erase(cluster);
        // Add to sharer list:
        if (oldest_iter->get_bcast_resp_valid()) { 
          if (oldest_iter->get_type() == IC_MSG_SPLIT_BCAST_SHR_REPLY) { 
            collective_sharer_list[addr].insert(cluster); 
          }
          if (oldest_iter->get_type() == IC_MSG_SPLIT_BCAST_OWNED_REPLY) { 
            collective_sharer_list[addr].insert(cluster); 
            collective_data_source_list[addr] = true;
          }
        }
        // Update the list.  When the count drops to zero, we can send the response
        // up to the global network.
        if (!collective_wait_list[addr].empty()) { 
          // At this point, we are done with the reply from the L2 and it can be erased.
          // Note that we allow all bcast combines to be merged each cycle, but
          // we restrict the number of replies to the GNet.
          l2_request[VCN_BCAST].erase(oldest_iter);
          continue; 
        }
        // If we reach this point, all L2 responses have been collected.
        // Reset the cluster number to be first one for the tile.
        cluster = CLUSTERS_PER_TILE * tileID;
        oldest_iter->set_cluster_num(cluster);
        // Update any of the skipped clusters, i.e., ones not participating in BCAST.
        for(std::set<int>::const_iterator rep_iter = collective_skip_list[addr].begin(),
                                          end = collective_skip_list[addr].end();
            rep_iter != end; ++rep_iter)
        {
          oldest_iter->add_bcast_skip_cluster(*rep_iter);
        }
        // Update the sharer list.
        for(std::set<int>::const_iterator rep_iter = collective_sharer_list[addr].begin(),
                                          end = collective_sharer_list[addr].end();
            rep_iter != end; ++rep_iter)
        {
          oldest_iter->add_bcast_resp_sharer(*rep_iter);
        }
        // Check to see if we are responsible for sourcing data.
        if (collective_data_source_list[addr]) {
          oldest_iter->set_type(IC_MSG_SPLIT_BCAST_OWNED_REPLY);
        }
        // Remove from the table and insert the reply for the tile into the network.  
        collective_skip_list.erase(addr);
        collective_wait_list.erase(addr);
        collective_sharer_list.erase(addr);
        collective_data_source_list.erase(addr);
        // Route request.  For now, we use PROBE network.  Could add special BCAST GNet
        RequestBuffers[VCN_PROBE].push_back(*oldest_iter);
        l2_request[VCN_BCAST].erase(oldest_iter);
        max_routes_this_cycle--;
      }
    }
  } else {
    // Handle probes and regular messages.
    if (RequestBuffers[vcn].size() < bufsize) {
      helper_L2_to_GC(l2_request[vcn], RequestBuffers[vcn]);
    }
  }
  // Only let L1s progress if nothing is stalling.
  if (!stall_l2_request[vcn]) {
    helper_L1_to_L2_routing(l2_request, l1_request, vcn);
  }
}

// Clock the interconnect.
int 
TileInterconnectNew::PerCycle()
{
  // Check for back pressure issues.
  for (size_t l1r = 0; l1r < GET_NUM_L1_ROUTERS(); l1r++) {
    for (unsigned int vc = 0; vc < VCN_LENGTH; vc++)
    {
      stall_l1_request[l1r][vc] = (l1_request[l1r][vc].size() >= L1_ROUTER_ENTRIES);
      stall_l1_reply[l1r][vc] = (l1_reply[l1r][vc].size() >= L1_ROUTER_ENTRIES);
    }
  }
  for (unsigned int vc = 0; vc < VCN_LENGTH; vc++)
  {
    stall_l2_request[vc] = (l2_request[vc].size() >= L2_ROUTER_ENTRIES);
    stall_l2_reply[vc] = (l2_reply[vc].size() >= L2_ROUTER_ENTRIES);
  }
  // Handle replies from GlobalNetwork.
  for(int i = 0; i < VCN_LENGTH; i++)
  {
    ChannelName vcn = static_cast<ChannelName>(i);
    if (VCN_BCAST == vcn) { helper_HandleRepliesBCast(vcn); }
    else                  { helper_HandleReplies(vcn);      }
  }
  // Handle requests from the clusters.
  //FIXME Buffer sizes should be encapsulated with buffers
  helper_HandleRequests(VCN_BCAST, MAX_ENTRIES_TILE_PROBE_RESP_BUFFER);
  helper_HandleRequests(VCN_PROBE, MAX_ENTRIES_TILE_PROBE_RESP_BUFFER);
  helper_HandleRequests(VCN_DEFAULT, MAX_ENTRIES_TILE_REQUEST_BUFFER);
  // TODO: As for now, there are no requests from clusters on the BCAST network.
  // helper_HandleRequests(VCN_BCAST ... NOP)
  assert(VCN_LENGTH == 3 && "> 2 VCs not handled properly (should encapsulate buffer sizes)");
  return 0;
}

static void 
helper_dump(std::list<ICMsg> &l, const char *s, const char *c, int line) {
  std::list<ICMsg>::iterator iter;
  fprintf(stderr, "%s: \n", s);
  for ( iter = l.begin(); iter != l.end(); iter++) { iter->dump(c, line); }
}

void 
TileInterconnectNew::dump()
{
  fprintf(stderr, "TILE %d INTERCONNECT DUMP\n", tileID);
  for(int i = 0; i < VCN_LENGTH; i++)
  {
    char s[128];
    snprintf(s, 128, "L2 VC %1d Request", i);
    helper_dump(l2_request[i], s, __FILE__, __LINE__);
    snprintf(s, 128, "L2 VC %1d Reply", i);
    helper_dump(l2_reply[i], s, __FILE__, __LINE__);
  }
  for(size_t l1r = 0; l1r < GET_NUM_L1_ROUTERS(); l1r++)
  {
    for(int vc = 0; vc < VCN_LENGTH; vc++)
    {
      char s[128];
      snprintf(s, 128, "L1R %4zu VC %1d Request", l1r, vc);
      helper_dump(l1_request[l1r][vc], s, __FILE__, __LINE__);
      snprintf(s, 128, "L1R %4zu VC %1d Reply", l1r, vc);
      helper_dump(l1_reply[l1r][vc], s, __FILE__, __LINE__);
    }
  }
}
