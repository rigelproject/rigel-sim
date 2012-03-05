#ifndef __TILE_INTERCONNECT_BASE_H__
#define __TILE_INTERCONNECT_BASE_H__

#include "icmsg.h"
#include "sim/componentbase.h"

template <int linesize> class MissHandlingEntry;

extern ComponentCount TileInterconnectBaseCount;

////////////////////////////////////////////////////////////////////////////////
// enum ChannelName
////////////////////////////////////////////////////////////////////////////////
//enum that stores the names of all channels desired in the network.
//TODO may want to instantiate networks with different subsets of channels,
//would need to construct networks differently (e.g. pass in vector<ChannelName>)
//NOTE: VCN_LENGTH just serves as a sentinel so we know how many channels there are
//programmatically.  This would be broken if we explicitly assign enum values.
////////////////////////////////////////////////////////////////////////////////
enum ChannelName {
  // Channel for coherence messages.
  VCN_PROBE,
  // Everything else (data, broadcast update/invalidate)
  VCN_DEFAULT, 
  // Handle broadcasts on separate VC.
  VCN_BCAST,
  // I am a sentinel whose value tells you how many real values are in the enum. Don't delete me!
  VCN_LENGTH 
};

////////////////////////////////////////////////////////////////////////////////
// Class TileInterconnectBase  
////////////////////////////////////////////////////////////////////////////////
// Note that this is just the interface and the common tracking logic.  All
// interconnect should derive from this one
////////////////////////////////////////////////////////////////////////////////
class TileInterconnectBase : public ComponentBase
{
  public: /* METHODS */

    /// constructor
    TileInterconnectBase(
      ComponentBase* parent,
      const char* cname = "TileInterconnectBase",
      ComponentCount& count = TileInterconnectBaseCount
    ) : 
        ComponentBase(parent,cname,count),
        RequestBuffers(VCN_LENGTH), 
        ReplyBuffers(VCN_LENGTH)
    { };
    
    /// Component required virtual methods

    /// Virtual destructor (not pure virtual because derived classes may be fine with default destructors)
		virtual ~TileInterconnectBase() { };

    /// PerCycle update method
    virtual int PerCycle() = 0;

    /// Dump interesting stats
    virtual void Dump() = 0;  

    /// for printing intermittent status data
    virtual void Heartbeat() = 0;   

    /// called at end of simulation
    virtual void EndSim() = 0;


    // If the link connected to this cluster is not full, the message is injected
    // into the network and returns true.  Otherwise, returns false.
    virtual bool ClusterInject(int cluster_number,
            MissHandlingEntry<rigel::cache::LINESIZE> &mshr,
            icmsg_type_t msg_type, seq_num_t seq_num, int seq_channel) = 0;
    // Check to see if a message has returned from the global cache.  If it has,
    // return true with the mshr filled with the proper MSHR.  The MSHR is not
    // updated and is not removed until ClusterRemoveAck() is called for the
    // cluster.   TODO: ClusterRemove and ClusterRemovePeek() should be merged.
    // Peek() is the same except that it does not remove a message
    virtual bool ClusterRemove(int cluster_number,
        MissHandlingEntry<rigel::cache::LINESIZE> &mshr) = 0;
    virtual bool ClusterRemovePeek(int cluster_number,
        MissHandlingEntry<rigel::cache::LINESIZE> &mshr) = 0;
    // Dump the full contents of the tile interconnect.
    virtual void dump();
    // Handle Network profiling for message.
    void helper_profile_net_msg_remove(ICMsg &msg);

  private:

  public: /* DATA */
    // Requests need to be removed from the interconnect and handled at the
    // GlobalCache (or possibly memory).  When the request is complete at that
    // level, the packet is sent back down to the cluster cache using the same
    // message.  This method deals with the link between the top level router
    // and the GCache.
		std::vector<std::list<ICMsg> > RequestBuffers;
		std::vector<std::list<ICMsg> > ReplyBuffers;
};
////////////////////////////////////////////////////////////////////////////////

#endif
