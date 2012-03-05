////////////////////////////////////////////////////////////////////////////////
// global_network.cpp
////////////////////////////////////////////////////////////////////////////////
//
// Implementation of the GlobalNetwork that sits between the tiles and the
// global cache banks.  We may want to add other topologies besides a staged
// corssbar.
//
////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <list>

#include "sim.h"
#include "util/util.h"
#include "global_network.h"
#include "memory/dram_channel_model.h"
#include "profile/profile.h"
#include "memory/address_mapping.h"
#include "RandomLib/Random.hpp"
//Only for G$ hit/miss profiling
#include "cache_model.h"
//#define DEBUG_GNET
#include "instr.h"
#include "util/debug.h"
#include "locality_tracker.h"

class GlobalCache;

////////////////////////////////////////////////////////////////////////////////
// constructor
////////////////////////////////////////////////////////////////////////////////
GlobalNetworkBase::GlobalNetworkBase(ComponentBase *parent, const char *name)
{
  if(rigel::ENABLE_LOCALITY_TRACKING)
  {
    std::string cppname("G$ Requests");
    aggregateLocalityTracker = new LocalityTracker(cppname, 8, LT_POLICY_RR,
                                                  rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
                                                  rigel::DRAM::BANKS, rigel::DRAM::ROWS);
  }
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// dump()
////////////////////////////////////////////////////////////////////////////////
void 
GlobalNetworkBase::dump()
{
  fprintf(stderr, "------------------DUMPING GNET------------------\n");
  for (uint32_t ctrl = 0; ctrl < rigel::DRAM::CONTROLLERS; ctrl++) {
    fprintf(stderr, "----> RequestBuffer #%4d <----\n", ctrl);
    std::list<ICMsg>::iterator iter;
    for (iter = RequestBuffers[ctrl][VCN_DEFAULT].begin(); 
      iter != RequestBuffers[ctrl][VCN_DEFAULT].end(); iter++)
    { iter->dump(__FILE__, __LINE__); }
  }
  for (uint32_t ctrl = 0; ctrl < rigel::DRAM::CONTROLLERS; ctrl++) {
    fprintf(stderr, "----> ReplyBuffer #%4d <----\n", ctrl);
    std::list<ICMsg>::iterator iter;
    for (iter = ReplyBuffers[ctrl][VCN_DEFAULT].begin(); 
      iter != ReplyBuffers[ctrl][VCN_DEFAULT].end(); iter++)
    { iter->dump(__FILE__, __LINE__); }
  }
  fprintf(stderr, "ProbeResponseBuffer:\n");
  for (uint32_t ctrl = 0; ctrl < rigel::DRAM::CONTROLLERS; ctrl++) {
    fprintf(stderr, "----> ProbeResponseBuffer #%4d <----\n", ctrl);
    std::list<ICMsg>::iterator iter;
    for (iter = RequestBuffers[ctrl][VCN_PROBE].begin(); 
      iter != RequestBuffers[ctrl][VCN_PROBE].end(); iter++)
    { iter->dump(__FILE__, __LINE__); }
  }
  fprintf(stderr, "ProbeRequestBuffer:\n");
  for (uint32_t ctrl = 0; ctrl < rigel::DRAM::CONTROLLERS; ctrl++) {
    fprintf(stderr, "----> ProbeRequestBuffer #%4d <----\n", ctrl);
    std::list<ICMsg>::iterator iter;
    for (iter = ReplyBuffers[ctrl][VCN_PROBE].begin(); 
      iter != ReplyBuffers[ctrl][VCN_PROBE].end(); iter++)
    { iter->dump(__FILE__, __LINE__); }
  }
  fprintf(stderr, "------------------END GNET DUMP------------------\n");
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// dump_locality()
////////////////////////////////////////////////////////////////////////////////
void 
GlobalNetworkBase::dump_locality() const
{
  assert(rigel::ENABLE_LOCALITY_TRACKING &&
         "Locality tracking disabled, GNet::dump_locality() called.");
  aggregateLocalityTracker->dump();
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// insert_into_request_buffer()
////////////////////////////////////////////////////////////////////////////////
void GlobalNetworkBase::insert_into_request_buffer(ICMsg &msg)
{
  if(rigel::ENABLE_LOCALITY_TRACKING)
  {
    for(std::vector<GCacheRequestStateMachine>::const_iterator it = msg.get_sms_begin(), end = msg.get_sms_end();
        it != end;
        ++it)
      aggregateLocalityTracker->addAccess(it->getAddr());
  }
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// helper_profile_gcache_stats()
////////////////////////////////////////////////////////////////////////////////
void 
GlobalNetworkBase::helper_profile_gcache_stats(ICMsg &msg)
{
  using namespace rigel;
  using namespace rigel::interconnect;
  // Profile G$ hit/miss statistics
  for (std::vector<GCacheRequestStateMachine>::const_iterator it = msg.get_sms_begin(), end = msg.get_sms_end();
        it != end;
        ++it)
  {
    const uint32_t addr = it->getAddr();
    if (GLOBAL_CACHE_PTR[AddressMapping::GetGCacheBank(addr)]->IsValidLine(addr))
    {
      switch (msg.get_type())
      {
        case IC_MSG_GCACHE_HWPREFETCH_REQ:
        case IC_MSG_CCACHE_HWPREFETCH_REQ:
        case IC_MSG_PREFETCH_REQ:
        case IC_MSG_PREFETCH_NGA_REQ:
          profiler::stats[STATNAME_GLOBAL_CACHE].inc_prefetch_hits();
          break;
        default:
          profiler::stats[STATNAME_GLOBAL_CACHE].inc_hits();
          break;
      }
      //FIXME How meaningful is it to do this multiple times if msg has multiple addrs?
      msg.set_gcache_hit();
    }
    else
    {
      switch (msg.get_type())
      {
        case IC_MSG_GCACHE_HWPREFETCH_REQ:
        case IC_MSG_CCACHE_HWPREFETCH_REQ:
        case IC_MSG_PREFETCH_REQ:
        case IC_MSG_PREFETCH_NGA_REQ:
          profiler::stats[STATNAME_GLOBAL_CACHE].inc_prefetch_misses();
          break;
        default:
          profiler::stats[STATNAME_GLOBAL_CACHE].inc_misses();
          profiler::stats[STATNAME_HISTOGRAM_GLOBAL_CACHE_MISSES].inc_mem_histogram(addr);
          break;
      }
      msg.set_gcache_miss();
    }
  }
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ReplyBufferInject()
////////////////////////////////////////////////////////////////////////////////
bool
GlobalNetworkBase::ReplyBufferInject(
  int queue_num, 
  ChannelName vcn, 
  ICMsg & msg)
{
  ReplyBuffers[queue_num][vcn].push_back(msg);
  return true;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BroadcastReplyInject()
////////////////////////////////////////////////////////////////////////////////
bool
GlobalNetworkBase::BroadcastReplyInject(
  int queue_num, 
  ChannelName vcn, 
  ICMsg & msg)
{
  return ReplyBufferInject(queue_num, vcn, msg);
}

////////////////////////////////////////////////////////////////////////////////
// RequestBufferRemove()
////////////////////////////////////////////////////////////////////////////////
std::list<ICMsg>::iterator 
GlobalNetworkBase::RequestBufferRemove(
  int queue_num, 
  ChannelName vcn, 
  std::list<ICMsg>::iterator msg)
{
  return RequestBuffers[queue_num][vcn].erase(msg);
}
////////////////////////////////////////////////////////////////////////////////


