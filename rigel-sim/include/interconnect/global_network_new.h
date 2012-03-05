#ifndef __GLOBAL_NETWORK_NEW_H__
#define __GLOBAL_NETWORK_NEW_H__

// Needed for ICMsg and ICLink
#include "interconnect.h"
#include "icmsg.h"
#include <list>
#include <cstdio>
#include <vector>

class LocalityTracker;

// Class Name: GlobalNetworkNew
//
// This is a new network model that is meant to replace the current global
// network with something more readable and better performing.  It has the same
// interface as GlobalNetworkIdeal.  It simulates the timing of an N-stage fully
// connected crossbar without blocking.  The latency is tunable using the
// '--gnet-link-delay <N>' command-line option.
//
class GlobalNetworkNew : public GlobalNetworkBase
{
  public:
    GlobalNetworkNew(rigel::ConstructionPayload cp) :
			GlobalNetworkBase(cp.parent,
					              cp.component_name.empty() ? "GlobalNetworkNew" : cp.component_name.c_str())
		{
			//cp.parent = this;
			//cp.component_name.clear();

			init();
		}

    void init();
    void PerCycle();
    void helper_VCPerCycleRequestsToGCache(ChannelName vcn,
                                           std::list<ICMsg> &buf,
                                           std::vector<int> &mcUsed,
                                           size_t MAX_ENTRIES);
    void helper_VCPerCycleRepliesToCCache(ChannelName vcn);
    // Used by the GCache to interact with the GNet.
		std::list<ICMsg>::iterator RequestBufferRemove(int queue_num, ChannelName vcn, std::list<ICMsg>::iterator msg);
    bool ReplyBufferInject(int queue_num, ChannelName vcn, ICMsg & msg);
    bool BroadcastReplyInject(int queue_num, ChannelName vcn, ICMsg &msg);

    size_t getNumUnpendedMessages(int queue_num, ChannelName vcn);

    void resetRouteCounts();
    void printRouteCounts(FILE *stream);

    std::vector<size_t> routeCountsUp;
    std::vector<size_t> routeCountsDown;
};

#endif
