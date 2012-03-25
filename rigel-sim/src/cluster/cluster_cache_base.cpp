#include "cluster/cluster_cache_base.h"
#include "sim.h"
#include "sim/component_count.h"
#include "util/construction_payload.h"

rigel::ComponentCount ClusterCacheBaseCount;

ClusterCacheBase::ClusterCacheBase(rigel::ConstructionPayload cp)
  : ComponentBase(cp.parent,
			            cp.change_name("ClusterCacheBase").component_name.c_str(),
									*(cp.change_component_count(ClusterCacheBaseCount).component_count)),
    coreside_ins(rigel::CORES_PER_CLUSTER), // FIXME: make this non-const
    coreside_outs(rigel::CORES_PER_CLUSTER), // FIXME: make this non-const
    mem_backing_store(cp.backing_store)
{
}

