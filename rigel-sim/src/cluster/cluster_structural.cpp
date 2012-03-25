
#include "cluster/cluster_structural.h"
#include "cluster/cluster_cache_structural.h"
#include "sim.h"
#include "util/construction_payload.h"

/// constructor
ClusterStructural::ClusterStructural(
  rigel::ConstructionPayload cp
) : 
  ClusterBase(cp.change_name("ClusterStructural")),
  numcores(rigel::CORES_PER_CLUSTER), // TODO: set dynamic
  halted_(0),
  to_interconnect(),
  from_interconnect()
{

  cp.parent = this;
	cp.component_name.clear();

  // connect ccache to cluster ports
  // this just assigns the ccache ports to be the cluster's ports
  // we like this because the cluster is contained, but the ccache is a separate object
  // we could instead try to use a DUMMY port object that basically does this assignment via attach
  from_interconnect = new InPortBase<Packet*>(  PortName(name(), id(), "in") );
  to_interconnect   = new OutPortBase<Packet*>( PortName(name(), id(), "out") );

  // the ccache will actually be responsible for reading, writing to the cluster's ports
  ccache = new ClusterCacheStructural(cp, from_interconnect, to_interconnect);

	cores = new CoreFunctional*[numcores];

  // make port connections:
  // TODO: do this somewhere more generic
  for (int i = 0; i < numcores; ++i) {
    cp.core_state = cp.cluster_state->add_cores();
    cp.component_index = i;
    cores[i] = new CoreFunctional(cp);     
    ccache->getCoreSideInPort(i)->attach( cores[i]->getOutPort() );
    cores[i]->getInPort()->attach( ccache->getCoreSideOutPort(i) );
  }

};

/// destructor
ClusterStructural::~ClusterStructural() {
  if (cores) {
    for (int i=0; i<numcores; i++) {
      delete cores[i];
    }
    delete cores;
  }
}

void
ClusterStructural::Heartbeat() {

  // p: core
  for (int p = 0; p < rigel::CORES_PER_CLUSTER; p++) {
		for (int t = 0; t < rigel::THREADS_PER_CORE; t++) {
		  fprintf(stderr, "cycle %"PRIu64": core %d, thread %d: 0x%08x\n", rigel::CURR_CYCLE, p, t, cores[p]->pc(t));
		} // end thread
  } // end core
}

/// PerCycle
int
ClusterStructural::PerCycle() {

  if (halted()) {
    return halted();
  }

  ccache->PerCycle(); 

  // HACK: clock the cores in rotating priority to avoid starvation
  // ideally, this should be done via multi-cycle arbitration and handled transparently
  // on the other side of the port interface
  int idx = rand() % numcores; 
  int halted_cores = 0;
  for (int offset = 0; offset < numcores; offset++) {
    halted_cores += cores[idx]->PerCycle();
    idx = (idx + 1) % numcores;
  }
  halted_ = halted_cores;

  if (halted()) {
    printf("all %d cores halted in cluster %d\n", numcores, id());
  }

  return halted();
};

void 
ClusterStructural::Dump() { 
  assert( 0 && "unimplemented!");
};

void 
ClusterStructural::EndSim() {
	profiler->end_sim();
	profiler->accumulate_stats();
  //assert( 0 && "unimplemented!");
};

void ClusterStructural::save_state() const {
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    cores[i]->save_state();
  }
}

void ClusterStructural::restore_state() {
  for(int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
    cores[i]->restore_state();
  }
}
