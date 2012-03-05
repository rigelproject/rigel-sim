#ifndef __GLOBAL_NETWORK_IDEAL_H__
#define __GLOBAL_NETWORK_IDEAL_H__

#include "sim.h"
// Needed for ICMsg and ICLink
#include "interconnect.h"
#include "icmsg.h"
#include "util/construction_payload.h"

// NOTE TO SELF: Each cycle the global network pulls requests from the tiles and
// places them into the global network.  It eventually pushes them up to the
// GCache via the RequestBuffer/ProbeResponseBuffer lists.  It also removes things from the
// RequestBuffer links and pushes them down into the tiles' RequestBuffer links.
// When a message comes down, it is removed from the request buffer.
class GlobalNetworkIdeal : public GlobalNetworkBase 
{
  public:
    GlobalNetworkIdeal(rigel::ConstructionPayload cp) :
			GlobalNetworkBase(cp.parent,
					              cp.component_name.empty() ? "GlobalNetworkIdeal" : cp.component_name.c_str())
    {
			//cp.parent = this;
			//cp.component_name.clear();

      init();
    }
    void init();
    void PerCycle();
    void helper_VCPerCycleRequestsToGCache(ChannelName vcn);
    void helper_VCPerCycleRepliesToCCache(ChannelName vcn);
};

#endif
