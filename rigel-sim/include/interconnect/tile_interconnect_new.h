#ifndef __TILE_INTERCONNECT_NEW_H__
#define __TILE_INTERCONNECT_NEW_H__

#include "interconnect/tile_interconnect_base.h"

////////////////////////////////////////////////////////////////////////////////
// Class Name: TileInterconnectNew
////////////////////////////////////////////////////////////////////////////////
// This is a new tile network model that will replace the old Tile H-tree thing.
// It will have to levels of 4-ary routers with fair routing policies and
// tunable queue delays.
////////////////////////////////////////////////////////////////////////////////
class TileInterconnectNew : public TileInterconnectBase 
{
  public:

    // Default constructor not implemented.
    TileInterconnectNew();

    // Constructor.  Parameter is the tile ID number.
    TileInterconnectNew(rigel::ConstructionPayload cp);

    // Inject the packet.  Called from CCache model.  Returns false if unable to
    // send the message this cycle.
    bool ClusterInject(int cluster_number,
            MissHandlingEntry<rigel::cache::LINESIZE> &mshr,
            icmsg_type_t msg_type, seq_num_t seq_num, int seq_channel);
    // Remove a message from the network (pull from net)
    bool ClusterRemove(int cluster_number, MissHandlingEntry<rigel::cache::LINESIZE> &mshr);
    // See if 
    bool ClusterRemovePeek(int cluster_number, MissHandlingEntry<rigel::cache::LINESIZE> &mshr);
    // Initialize the interconnect.
    void init();
    // Clock the interconnect.
    int PerCycle();
    // Identification number for the tile.
    int tileID;
    // Debugging print out.
    void dump();

    /// required by ComponentBase
    void Dump() { assert(0&&"unimplemented"); }
    void Heartbeat() { assert(0&&"unimplemented"); }
    void EndSim() { assert(0&&"unimplemented"); }

  private: /* DATA */
    // Queue with all of the messages coming from the clusters.  One for each
    // channel. Represents the second-level router.
    std::vector<std::list<ICMsg> > l2_request;
    std::vector<std::list<ICMsg> > l2_reply;
    // First-level router queues.  First array index selects router, second selects
    // channel (e.g. l1_request[1][2] is router 1, VC 2.)
    std::vector<std::vector<std::list<ICMsg> > > l1_request;
    std::vector<std::vector<std::list<ICMsg> > > l1_reply;
    // Signals used by routers to stop new requests in the next cycle. Must be
    // set/reset at every PerCycle call.  We need separate signals for
    // Coherence requests and normal requests (one for each VC).
    // 2D array for L1 routers since there are multiple of them per tile, multiple VCs per router
    std::vector<std::vector<bool> > stall_l1_request, stall_l1_reply;
    // 1D array for L2 router (one stall signal per VC)
    std::vector<bool> stall_l2_request, stall_l2_reply;
    // TODO: Temporary hack to bootstrap collective network.  Indexed by address.
    std::map< uint32_t, std::set<int> > collective_wait_list;
    std::map< uint32_t, std::set<int> > collective_skip_list;
    // Set of clusters that share the line.
    std::map< uint32_t, std::set<int> > collective_sharer_list;
    // Bit is set for address if this tile must source the data, i.e., an OWNED
    // reply came in.
    std::map< uint32_t, bool  > collective_data_source_list;
  
  private: /* METHODS */
    // ClusterRemovePeek and ClusterRemove have merged functionality.  This is it.
    bool ClusterRemoveBase(bool erase, int cluster_number, 
      MissHandlingEntry<rigel::cache::LINESIZE> &mshr);
    // Select the channel to use for the cluster remove.
    bool ClusterRemoveBaseNetSelect(bool erase, int cluster_number, 
      MissHandlingEntry<rigel::cache::LINESIZE> &mshr, std::list<ICMsg> &chan);
    void helper_HandleRequests(ChannelName vcn, size_t bufsize);
    void helper_HandleReplies(ChannelName vcn);
    void helper_HandleRepliesBCast(ChannelName vcn);
};
////////////////////////////////////////////////////////////////////////////////

#endif
