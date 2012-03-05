////////////////////////////////////////////////////////////////////////////////
// src/debug.cpp
////////////////////////////////////////////////////////////////////////////////
//
// The debug infrastructure is used for dumping system state on a fatal error in
// the simulator, e.g., a WDT expiring.
//
////////////////////////////////////////////////////////////////////////////////

#include <cstdio>                      // for fprintf, stderr
#include "cache_model.h"    // for CacheModel
#include "caches/global_cache.h"  // for GlobalCache, etc
#include "caches/l2d.h"     // for L2Cache, etc
#include "util/debug.h"          // for DebugSim
#include "directory.h"      // for CacheDirectory
#include "global_network.h"  // for GlobalNetworkBase
#include "interconnect.h"   // for TileInterconnectBase

DebugSim rigel::GLOBAL_debug_sim;

void
DebugSim::dump_all()
{
  dump_gcache_ctrl();
  dump_gcache_state();
  dump_ccache_ctrl();
  dump_ccache_state();
  dump_gnet();
  dump_tilenet();
  dump_directory();
}

void
DebugSim::dump_gcache_ctrl()
{
  fprintf(stderr, "----------------------DUMPING GLOBAL CACHE STATE---------------------------\n");
	for(std::vector<GlobalCache *>::const_iterator it = gcache.begin(), end = gcache.end(); it != end; ++it) {
    fprintf(stderr, "==== GLOBAL CACHE %5d ====\n", (*it)->bankID);
    (*it)->dump();
  }
}

void
DebugSim::dump_gcache_state()
{

}

void
DebugSim::dump_ccache_ctrl()
{
  fprintf(stderr, "-----------------------DUMPING CLUSTER CACHE CNTRL STATE--------------------\n");
	for(std::vector<CacheModel *>::const_iterator it = cluster_cache_models.begin(), end = cluster_cache_models.end(); it != end; ++it) {
    fprintf(stderr, "==== CLUSTER CACHE %5d ====\n", (*it)->cluster_num);
    (*it)->L2.dump(); 
    fprintf(stderr, "PENDING MISSES\n");
    (*it)->L2.dump_pending_msgs(); 
    fprintf(stderr, "PENDING PREFETCHES\n");
    (*it)->L2.dump_cache_state(); 
  }
}

void
DebugSim::dump_ccache_state()
{
}

void
DebugSim::dump_gnet()
{
    fprintf(stderr, "-----------------------DUMPING GNET--------------------\n");
		for(std::vector<GlobalNetworkBase *>::const_iterator it = gnet.begin(), end = gnet.end(); it != end; ++it) {
      (*it)->dump();
		}
}

void
DebugSim::dump_tilenet()
{
  fprintf(stderr, "-----------------------DUMPING TILE INTERCONNECT STATE--------------------\n");
	for(std::vector<TileInterconnectBase *>::const_iterator it = tilenet.begin(), end = tilenet.end(); it != end; ++it) { 
    fprintf(stderr, "==== TILE: %5ld ====\n", it-tilenet.begin());
    (*it)->dump(); 
  }

}

void
DebugSim::dump_directory()
{
    fprintf(stderr, "-----------------------DUMPING DIRECTORIES--------------------\n");
		for(std::vector<CacheDirectory *>::const_iterator it = directory.begin(), end = directory.end(); it != end; ++it) {
			(*it)->dump();
		}
}

