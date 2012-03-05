#ifndef __TILE_INTERCONNECT_IDEAL_H__
#define __TILE_INTERCONNECT_IDEAL_H__

#include "interconnect/tile_interconnect_base.h"

namespace rigel {
  class ConstructionPayload;
}

////////////////////////////////////////////////////////////////////////////////
// Class TileInterconnectIdeal
////////////////////////////////////////////////////////////////////////////////
class TileInterconnectIdeal: public TileInterconnectBase 
{
  public:

    // Default constructor not implemented.
    TileInterconnectIdeal();

    // Constructor.  Parameter is the tile ID number.
    TileInterconnectIdeal(rigel::ConstructionPayload cp);

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

  private:
    // Queue with all of the messages coming from the clusters.  One for each channel.
    std::vector<std::list<ICMsg> > requests;
    std::vector<std::list<ICMsg> > replies;

    // ClusterRemovePeek and ClusterRemove have merged functionality.  This is it.
    bool ClusterRemoveBase(bool erase, int cluster_number, 
      MissHandlingEntry<rigel::cache::LINESIZE> &mshr);
    // Select the channel to use for the cluster remove.
    bool ClusterRemoveBaseNetSelect(bool erase, int cluster_number, 
      MissHandlingEntry<rigel::cache::LINESIZE> &mshr, ChannelName vcn);

    /// required by ComponentBase
    void Dump() { assert(0&&"unimplemented"); }
    void Heartbeat() { assert(0&&"unimplemented"); }
    void EndSim() { assert(0&&"unimplemented"); }

};

#endif
