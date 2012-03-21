
#include "cluster/cluster_functional.h"
#include "cluster/cluster_cache_functional.h"
#include "sim.h"
#include "util/construction_payload.h"

/// constructor
ClusterFunctional::ClusterFunctional(
  rigel::ConstructionPayload cp
) : 
  ClusterBase(cp.change_name("ClusterFunctional")),
  numcores(rigel::CORES_PER_CLUSTER), // TODO: set dynamic
  halted_(0),
  to_interconnect(),
  from_interconnect()
{

  cp.parent = this;
	cp.component_name.clear();

  ccache = new ClusterCacheFunctional(cp, from_interconnect, to_interconnect);

  // connect ccache to cluster ports
  // this just assigns the ccache ports to be the cluster's ports
  // we like this because the cluster is contained, but the ccache is a separate object
  // we could instead try to use a DUMMY port object that basically does this assignment via attach
  //from_interconnect = ccache->getMemSideInPort();
  //to_interconnect   = ccache->getMemSideOutPort();

	cores = new CoreFunctional*[numcores];

  // make port connections:
  // TODO: do this somewhere more generic
  for (int i = 0; i < numcores; ++i) {
    DPRINT(true,"CF ports: %d\n", i);
    cp.core_state = cp.cluster_state->add_cores();
    cp.component_index = i;
    cores[i] = new CoreFunctional(cp,ccache);     
    ccache->getCoreSideInPort(i)->attach( cores[i]->getOutPort() );
    cores[i]->getInPort()->attach( ccache->getCoreSideOutPort(i) );
  }

};

/// destructor
ClusterFunctional::~ClusterFunctional() {
  if (cores) {
    for (int i=0; i<numcores; i++) {
      delete cores[i];
    }
    delete cores;
  }
}

void
ClusterFunctional::Heartbeat() {

  // p: core
  for (int p = 0; p < rigel::CORES_PER_CLUSTER; p++) {
		for (int t = 0; t < rigel::THREADS_PER_CORE; t++) {
		  fprintf(stderr, "cycle %"PRIu64": core %d, thread %d: 0x%08x\n", rigel::CURR_CYCLE, p, t, cores[p]->pc(t));
		} // end thread
  } // end core
}

/// PerCycle
int
ClusterFunctional::PerCycle() {

  if (halted()) {
    return halted();
  }

  ccache->PerCycle(); 

  int halted_cores = 0;
  for (int i=0; i < numcores; i++) {
    halted_cores += cores[i]->PerCycle();
  }
  halted_ = halted_cores;

  if (halted()) {
    printf("all %d cores halted in cluster %d\n", numcores, id());
  }

  return halted();
};

void 
ClusterFunctional::Dump() { 
  assert( 0 && "unimplemented!");
};

void 
ClusterFunctional::EndSim() {
	profiler->end_sim();
	profiler->accumulate_stats();
  //assert( 0 && "unimplemented!");
};

void ClusterFunctional::save_state() const {
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    cores[i]->save_state();
  }
}

void ClusterFunctional::restore_state() {
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    cores[i]->restore_state();
  }
}
