
#include "cluster/cluster_functional.h"
#include "cluster/cluster_cache_functional.h"
#include "sim.h"
#include "util/construction_payload.h"

/// constructor
ClusterSimple::ClusterSimple(
  rigel::ConstructionPayload cp
) : 
  ClusterBase(cp.change_name("ClusterSimple")),
  numcores(rigel::CORES_PER_CLUSTER), // TODO: set dynamic
  halted_(0)
  //LoadLinkFIXME(numcores)
{
  cp.parent = this;
	cp.component_name.clear();

  ccache = new ClusterCacheFunctional(cp);

	cores = new CoreFunctional*[numcores];

  for (int i = 0; i < numcores; ++i) {
    cp.core_state = cp.cluster_state->add_cores();
    cp.component_index = i;
    cores[i] = new CoreFunctional(cp,ccache);     
    ccache->getInPort(i)->attach( cores[i]->getOutPort() );
    cores[i]->getInPort()->attach( ccache->getOutPort(i) );
  }

};

/// destructor
ClusterSimple::~ClusterSimple() {
  if (cores) {
    for (int i=0; i<numcores; i++) {
      delete cores[i];
    }
    delete cores;
  }
}

void
ClusterSimple::Heartbeat() {

  // p: core
  for (int p = 0; p < rigel::CORES_PER_CLUSTER; p++) {
		for (int t = 0; t < rigel::THREADS_PER_CORE; t++) {
		  fprintf(stderr, "cycle %"PRIu64": core %d, thread %d: 0x%08x\n", rigel::CURR_CYCLE, p, t, cores[p]->pc(t));
		} // end thread
  } // end core
}

/// PerCycle
int
ClusterSimple::PerCycle() {

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
ClusterSimple::Dump() { 
  assert( 0 && "unimplemented!");
};

void 
ClusterSimple::EndSim() {
	profiler->end_sim();
	profiler->accumulate_stats();
  //assert( 0 && "unimplemented!");
};

void ClusterSimple::save_state() const {
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    cores[i]->save_state();
  }
}

void ClusterSimple::restore_state() {
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    cores[i]->restore_state();
  }
}
