#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, stderr
#include <bitset>                       // for bitset
#include <list>                         // for list, _List_iterator, etc
#include <map>                          // for _Rb_tree_const_iterator
#include <string>                       // for string
#include <vector>                       // for vector, vector<>::iterator
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for ::IC_MSG_NULL, icmsg_type_t, etc
#include "icmsg.h"          // for ICMsg
#include "interconnect.h"   // for TileInterconnectIdeal, etc
#include "mshr.h"           // for MissHandlingEntry
#include "seqnum.h"         // for seq_num_t
#include "sim.h"            // for LINESIZE, CURR_CYCLE, etc
#include "util/util.h"           // for SimTimer
#include "util/construction_payload.h"

// Constructor.  Parameter is the tile ID number.
TileInterconnectIdeal::TileInterconnectIdeal(rigel::ConstructionPayload cp) :
  TileInterconnectBase(cp.parent,cp.component_name.empty() ? "TileInterconnectIdeal" : cp.component_name.c_str()),
  tileID(cp.component_index)
{
	cp.parent = this;
	cp.component_name.clear();

  // Add to the global list of tile interconnects for debug printing.
  rigel::GLOBAL_debug_sim.tilenet.push_back(this);
  // Allocate message buffers
  requests.resize(VCN_LENGTH);
  replies.resize(VCN_LENGTH);
}

// Inject the packet.  Called from CCache model.  Returns false if unable to
// send the message this cycle.
bool 
TileInterconnectIdeal::ClusterInject(
  int cluster_number,
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr,
  icmsg_type_t msg_type, 
  seq_num_t seq_num, 
  int seq_channel) 
{
  using namespace rigel;

  //FIXME should not have addr and addrs, just addrs
  uint32_t addr = *(mshr.get_pending_addrs_begin());
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
  request_msg.remove_all_addrs();
  request_msg.copy_addresses(mshr.get_pending_addrs_begin(), mshr.get_pending_addrs_end());

  // Set the sequence numbers for the limited directory FSM.
  request_msg.set_seq(seq_channel, seq_num);
  // Timer for network occupancy starts here, stops when it hits the GCache
  request_msg.timer_network_occupancy.StartTimer();
  // Set time when message can move to the next level of the interconnect.
  request_msg.set_ready_cycle(TILENET_CC2GNET_LAT_L1+TILENET_CC2GNET_LAT_L2);
  // Differentiate between coherence requests and other.
  if (ICMsg::check_is_coherence(msg_type)) { 
    requests[VCN_PROBE].push_back(request_msg);
  } else { 
    requests[VCN_DEFAULT].push_back(request_msg);
  }

  DEBUG_network("CCACHE INJECT", request_msg);
  return true;
}

bool 
TileInterconnectIdeal::ClusterRemoveBase(
  bool erase_msg,
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr)
{
  //Rotate priority among VCs every cycle.
  for (int i = rigel::CURR_CYCLE % VCN_LENGTH, count = 0; count < VCN_LENGTH; count++, i = (i + 1) % VCN_LENGTH)
  {
    ChannelName vcn = static_cast<ChannelName>(i);
    if (ClusterRemoveBaseNetSelect(erase_msg, cluster_num, mshr, vcn))
      return true;
  }
  return false;
}

bool 
TileInterconnectIdeal::ClusterRemoveBaseNetSelect(
  bool erase_msg,
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr,
  ChannelName vcn)
{
  std::list<ICMsg> &chan = replies[vcn];
  if (chan.size() > 0) 
  {
    // Search for a message destined for this cluster.
    std::list<ICMsg>::iterator iter = chan.begin();
    while (iter != chan.end() 
        && iter->get_cluster() != cluster_num) { iter++; };
    // We hit the end of list, return.
    if (iter == chan.end()) { return false; }

    ICMsg return_msg = *iter;
    // Setup the MSHR for the reply and then return
    uint32_t addr = return_msg.get_addr();
    // Since vlatency is set to false here, no need to ack/sched
    assert(return_msg.get_type() != IC_MSG_NULL);
    mshr.set(addr, 0, false,
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


// Remove a message from the network (pull from net)
bool 
TileInterconnectIdeal::ClusterRemove(
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr)
{
  return ClusterRemoveBase(true, cluster_num, mshr);
}

// See if a message is available.
bool 
TileInterconnectIdeal::ClusterRemovePeek(
  int cluster_num, 
  MissHandlingEntry<rigel::cache::LINESIZE> &mshr)
{
  return ClusterRemoveBase(false, cluster_num, mshr);
}

// Initialize the interconnect.
void 
TileInterconnectIdeal::init()
{
}

// Clock the interconnect.
int 
TileInterconnectIdeal::PerCycle()
{
  // Handle replies from GlobalNetwork.
  for(std::vector<std::list<ICMsg> >::iterator ReplyBuffersIterator = ReplyBuffers.begin(), repliesIterator = replies.begin(); ReplyBuffersIterator != ReplyBuffers.end(); ++ReplyBuffersIterator, ++repliesIterator)
  {
    std::list<ICMsg>::iterator bufIterator = ReplyBuffersIterator->begin();
    while(bufIterator != ReplyBuffersIterator->end())
    {
      // Only let the message move on if it is ready.
      if (bufIterator->is_ready_cycle()) {
        bufIterator->set_type(ICMsg::convert(bufIterator->get_type()));
        DEBUG_network("TILE G$->C$ ROUTE", *bufIterator);
        repliesIterator->push_back(*bufIterator);
        bufIterator = ReplyBuffersIterator->erase(bufIterator);
      } else { ++bufIterator; }
    }
  }
  //Handle requests from clusters
  for(std::vector<std::list<ICMsg> >::iterator RequestBuffersIterator = RequestBuffers.begin(), requestsIterator = requests.begin(); RequestBuffersIterator != RequestBuffers.end(); ++RequestBuffersIterator, ++requestsIterator)
  {
    std::list<ICMsg>::iterator bufIterator = RequestBuffersIterator->begin();
    while(bufIterator != RequestBuffersIterator->end())
    {
      // Only let the message move on if it is ready.
      if (bufIterator->is_ready_cycle()) {
        bufIterator->set_type(ICMsg::convert(bufIterator->get_type()));
        DEBUG_network("TILE C$->G$ ROUTE", *bufIterator);
        RequestBuffersIterator->push_back(*bufIterator);
        bufIterator = requestsIterator->erase(bufIterator);
      } else { ++bufIterator; }
    }
  }
  return 0;
}

void 
TileInterconnectIdeal::dump()
{
  std::list<ICMsg>::iterator iter;
  fprintf(stderr, "requests: \n");
  unsigned int i = 0;
  for (std::vector<std::list<ICMsg> >::iterator it = requests.begin(); it != requests.end(); ++it)
  {
    fprintf(stderr, "channel %u:\n", i++);
    for ( iter = it->begin();
          iter != it->end();
          ++iter) { iter->dump(__FILE__, __LINE__); }
  }
  i = 0;
  fprintf(stderr, "replies: \n");
  for (std::vector<std::list<ICMsg> >::iterator it = replies.begin(); it != replies.end(); ++it)
  {
    fprintf(stderr, "channel %u:\n", i++);
    for ( iter = it->begin();
          iter != it->end();
          ++iter) { iter->dump(__FILE__, __LINE__); }
  }
}
