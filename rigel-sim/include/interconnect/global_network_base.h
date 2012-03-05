#ifndef __GLOBAL_NETWORK_BASE_H__
#define __GLOBAL_NETWORK_BASE_H__

#include "sim.h"
// Needed for ICMsg and ICLink
#include "interconnect.h"
#include "icmsg.h"
#include <list>
#include <cstdio>
#include <vector>

class LocalityTracker;
class ComponentBase;

// Class Name: GlobalNetworkBase
//
// Abstract interface for link between tiles and global cache banks.  Derive
// from this and implement its methods and all shall be happy.

//FIXME This should inherit from ComponentBase, we just have to implement the required methods.
class GlobalNetworkBase 
{
  public:
    GlobalNetworkBase(ComponentBase *parent, const char *name);
	virtual ~GlobalNetworkBase() { };
    // Clock the global network.
    virtual void PerCycle() = 0;
    // Handle GCache profiling for the message.
    void helper_profile_gcache_stats(ICMsg &msg);
    void addTileInterconnect(int tileID, TileInterconnectBase *t)
    {
      assert((tiles[tileID] == NULL) && "tile ID already registered with GNet");
      tiles[tileID] = t;
    }

    virtual void insert_into_request_buffer(ICMsg &msg);
    
    // Remove a message from the specified position (msg), return an iterator to the next message.
    virtual std::list<ICMsg>::iterator 
      RequestBufferRemove(int queue_num, ChannelName vcn, std::list<ICMsg>::iterator msg);
    // Add a message (msg) to the back of the specified queue and channel.
    // Returns true on success.
    virtual bool ReplyBufferInject(int queue_num, ChannelName vcn, ICMsg &msg);
    // Add a message to the broadcast network if present.  Otherwise submit to
    // the ReplyBuffer.
    virtual bool BroadcastReplyInject(int queue_num, ChannelName vcn, ICMsg &msg);
    // Print the state of the network to stderr.
    void dump();
    void dump_locality() const;

  public: /* DATA */
    // Outer dimension gives one set of buffers for each memory controller.
    // Inner dimension gives one buffer for each VC
		std::vector<std::vector<std::list<ICMsg> > > RequestBuffers;
		std::vector<std::vector<std::list<ICMsg> > > ReplyBuffers;
    // Used when -lt command-line arg is passed to track the DRAM locality of requests in the GNet
    LocalityTracker *aggregateLocalityTracker;
    // Array of tile pointers covered by the global netowrk.
    TileInterconnectBase **tiles;
};

#endif
