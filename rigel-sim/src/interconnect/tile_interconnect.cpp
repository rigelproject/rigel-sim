

////////////////////////////////////////////////////////////////////////////////
// interconnect.cpp 
////////////////////////////////////////////////////////////////////////////////
// Implementation of classes related to the tile-level interconnect
//
// InterconnectBase implementations ONLY
//
////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>                     // for uint64_t
#include <stdio.h>                      // for fprintf, stderr
#include <list>                         // for list, _List_iterator, etc
#include <string>                       // for string
#include <vector>                       // for vector, std::vector<>::iterator
#include "define.h"         // for rigel_log2, etc
#include "icmsg.h"          // for ICMsg
#include "interconnect.h"   // for TileInterconnectBase
#include "profile/profile.h"        // for NetworkStat, Profile, etc
#include "sim.h"
#include "util/util.h"           // for SimTimer

#if 0
#define __DEBUG_ADDR 0x0046240
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#undef __DEBUG_CLUSTER
#undef __DEBUG_CYCLE
#endif

ComponentCount TileInterconnectBaseCount;

// TODO: why is this here?
uint64_t ICMsg::GLOBAL_MSG_NUMBER = 0;

////////////////////////////////////////////////////////////////////////////////
// dump()
////////////////////////////////////////////////////////////////////////////////
void
TileInterconnectBase::dump()
{
	  std::list<ICMsg>::iterator iter;
    unsigned int i = 0;
    fprintf(stderr, " --- RequestBuffers --- \n");
    for(std::vector<std::list<ICMsg> >::iterator it = RequestBuffers.begin(); it != RequestBuffers.end(); ++it)
    {
      fprintf(stderr, " ---- Channel %u ---- \n", i++);
      for ( iter = it->begin(); iter != it->end(); ++iter)
      {
        iter->dump(__FILE__, __LINE__);
      }
    }
    i = 0;
    fprintf(stderr, " --- ReplyBuffers --- \n");
    for(std::vector<std::list<ICMsg> >::iterator it = ReplyBuffers.begin(); it != ReplyBuffers.end(); ++it)
    {
      fprintf(stderr, " ---- Channel %u ---- \n", i++);
      for ( iter = it->begin(); iter != it->end(); ++iter)
      {
        iter->dump(__FILE__, __LINE__);
      }
    }
}

////////////////////////////////////////////////////////////////////////////////
// helper_profile_net_msg_remove()
////////////////////////////////////////////////////////////////////////////////
void
TileInterconnectBase::helper_profile_net_msg_remove(ICMsg &msg)
{
  using namespace rigel::profiler;
  using namespace rigel;

  msg.timer_network_occupancy.StopTimer();
  // Handle profiler statistics for network occupancy
  if (PROFILE_HIST_GCACHEOPS) 
  {
    uint64_t profiler_hist_bin = (CURR_CYCLE / gcacheops_histogram_bin_size)
          * gcacheops_histogram_bin_size;
    int size_bin = rigel_log2(
      msg.timer_network_occupancy.GetTime() +
      msg.timer_memory_wait.GetTime()
    );

    ProfileStat::net_hist_stats[profiler_hist_bin][1<<size_bin]++;
  }
  // Calculate the time spent in the network and the amount of time spent
  // waiting on memory.
  Profile::global_network_stats.msg_removes++;
  Profile::global_network_stats.network_occupancy +=
      msg.timer_network_occupancy.GetTime();
  Profile::global_network_stats.gcache_wait_time +=
      msg.timer_memory_wait.GetTime();
  // Handle profiling separately for regular load misses.
  if (IC_MSG_READ_REPLY == msg.get_type()) {
    Profile::global_network_stats.gcache_wait_time_read +=
        msg.timer_memory_wait.GetTime();
    Profile::global_network_stats.network_occupancy_read +=
        msg.timer_network_occupancy.GetTime();
    Profile::global_network_stats.msg_removes_read++;
  }

  // For accounting purposes, gcmiss and gchit are not counted for
  // prefetches and coherence messages..
  if (msg.get_type() != IC_MSG_PREFETCH_REPLY
   && msg.get_type() != IC_MSG_PREFETCH_NGA_REPLY
   && msg.get_type() != IC_MSG_CCACHE_HWPREFETCH_REPLY
   && msg.get_type() != IC_MSG_GCACHE_HWPREFETCH_REPLY
   && !ICMsg::check_is_coherence(msg.get_type()))
  {
    if (msg.get_gcache_hit()) {
      Profile::global_network_stats.gcache_wait_time_gchit +=
          msg.timer_memory_wait.GetTime();
    } else {
      Profile::global_network_stats.gcache_wait_time_gcmiss +=
          msg.timer_memory_wait.GetTime();
    }
  }
}
