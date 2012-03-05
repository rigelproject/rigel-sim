#ifndef __TILE_INTERCONNECT_BROADCAST_H__
#define __TILE_INTERCONNECT_BROADCAST_H__

#include "interconnect/tile_interconnect_new.h"

namespace rigel {
  class ConstructionPayload;
}

////////////////////////////////////////////////////////////////////////////////
// Class Name: TileInterconnectBroadcast
////////////////////////////////////////////////////////////////////////////////
// Variant of the TileInterconnectNew that uses a broadcast network.
////////////////////////////////////////////////////////////////////////////////
class TileInterconnectBroadcast : public TileInterconnectNew
{
  public:

    // Constructor.  Parameter is the tile ID number.
    TileInterconnectBroadcast(rigel::ConstructionPayload cp);

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
    // Debugging print out.
    void dump();


    /// required by ComponentBase
    void Dump() { assert(0&&"unimplemented"); }
    void Heartbeat() { assert(0&&"unimplemented"); }
    void EndSim() { assert(0&&"unimplemented"); }
    

};
////////////////////////////////////////////////////////////////////////////////

#endif
