
////////////////////////////////////////////////////////////////////////////////
// interconnect.h
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifndef __INTERCONNECT_H__
#define __INTERCONNECT_H__

#include "sim.h"
#include "interconnect/tile_interconnect_base.h"
#include "interconnect/tile_interconnect_ideal.h"
#include "interconnect/tile_interconnect_new.h"
#include "interconnect/tile_interconnect_broadcast.h"

#include "interconnect/tree_network.h"
#include "interconnect/crossbar.h"

// Necessary evil for timers.  util.h is copesetic otherwise.
#include "util/util.h"
#include "icmsg.h"
#include "seqnum.h"
#include "memory/address_mapping.h"
#include <string>
#include <stdint.h>
#include <map>
#include <vector>
#include <set>
//#include "gcache_request_state_machine.h" //Have to include since we have a set<> of them :(

#if 0
#define __DEBUG_ADDR 0x2d3c780
#define __DEBUG_CYCLE 0x0
#else
#undef __DEBUG_ADDR
#endif

class SimTimer;
template <int linesize> class MissHandlingEntry;

//namespace directory {
//  struct dir_of_timing_t;
//}

inline void _DEBUG_network( std::string str, 
                            const ICMsg &msg, 
                            const char *filename, 
                            const int linenum, 
                            const char *function);

#define DEBUG_network(s, m) _DEBUG_network(s, m, __FILE__, __LINE__, __FUNCTION__);

// TODO comment this method
inline void
_DEBUG_network(
  std::string str, 
  const ICMsg &msg, 
  const char *filename, 
  const int linenum, 
  const char *function)
{
#ifdef __DEBUG_ADDR
  using namespace rigel;
  const uint32_t addr = msg.get_addr();
  const uint64_t msg_num = msg.get_msg_number();
  const icmsg_type_t type = msg.get_type();
  const int cluster = msg.get_cluster();

  if ((addr == __DEBUG_ADDR) && (__DEBUG_CYCLE< CURR_CYCLE))
  {
    __DEBUG_HEADER(filename, linenum, function);
    fprintf(stderr, "[%s] ", str.c_str());
    fprintf(stderr, "addr: %08x msgnum: %12llu type: %4d cluster: %4d\n",
      get_addr(), msg_num, type, cluster);
  }
#endif
}

class GlobalCache;






#endif

