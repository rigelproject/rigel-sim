#ifndef __DEBUG_H__
#define __DEBUG_H__
#include "sim.h"
#include <vector>
////////////////////////////////////////////////////////////////////////////////
// include/debug.h
////////////////////////////////////////////////////////////////////////////////
//
// The debug infrastructure is used for dumping system state on a fatal error in
// the simulator, e.g., a WDT expiring.
//
////////////////////////////////////////////////////////////////////////////////

class GlobalNetworkBase;
class ClusterLegacy;
class CacheModel;
class GlobalCache;
class CacheDirectory;
class TileInterconnectBase;

class DebugSim 
{

  public:
    void dump_all();
    void dump_gcache_ctrl();
    void dump_gcache_state();
    void dump_ccache_ctrl();
    void dump_ccache_state();
    void dump_gnet();
    void dump_tilenet();
    void dump_directory();

    std::vector<GlobalNetworkBase *> gnet;
    std::vector<ClusterLegacy *> cluster;
    std::vector<CacheModel *> cluster_cache_models;
    std::vector<GlobalCache *> gcache;
    std::vector<CacheDirectory *> directory;
    std::vector<TileInterconnectBase *> tilenet;

    int _indent_depth;

};

namespace rigel {
  extern DebugSim GLOBAL_debug_sim;
}

#endif
