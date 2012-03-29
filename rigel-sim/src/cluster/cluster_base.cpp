////////////////////////////////////////////////////////////////////////////////
// clusterbase.cpp
////////////////////////////////////////////////////////////////////////////////
// pure abstract class, never intended to be instantiated
////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>                     // for uint32_t
#include "cluster/cluster_base.h"    // for ClusterBase, etc
#include "profile/profile.h"        // for Profile
#include "sim.h"            // for MemoryModelType
#include "util/construction_payload.h"
#include "rigelsim.pb.h"

ComponentCount ClusterBaseCount;

/// constructor
ClusterBase::ClusterBase(
  rigel::ConstructionPayload &cp
) :
  ComponentBase(cp.parent, cp.change_name("ClusterBase").component_name.c_str(), ClusterBaseCount),
  TileInterconnect(cp.tile_network),
  backing_store(cp.backing_store), 
  _cluster_id(cp.component_index),
  cluster_state(cp.cluster_state)
{
  // FIXME profiler is only in the base class because TileLegacy calls getProfiler()
	// unconditionally.
  profiler = new Profile(backing_store, _cluster_id);
}

////////////////////////////////////////////////////////////////////////////////
// Cluster Destructor
////////////////////////////////////////////////////////////////////////////////
ClusterBase::~ClusterBase() {
  delete profiler;
}


