#ifndef __TREE_NETWORK_H__
#define __TREE_NETWORK_H__

#include "sim/component_base.h"
#include "util/construction_payload.h"

#include "util/fifo.h"

extern ComponentCount TreeNetworkCount;

//forward declarations
class Packet;
template<class T> class InPortBase;
template<class T> class OutPortBase;

namespace rigel {
  class ConstructionPayload;
}

class TreeNetwork : public ComponentBase {

  public:

	  TreeNetwork(
      rigel::ConstructionPayload cp,
      InPortBase<Packet*>* in,
      OutPortBase<Packet*>* out,
      ComponentCount& count = TreeNetworkCount
    );

    void Dump()      {};
    void Heartbeat() {};
    void EndSim()    {};
    int PerCycle();

    InPortBase<Packet*>* get_inport(int p) { return leaf_inports[p]; }
    OutPortBase<Packet*>* get_outport(int p) { return leaf_outports[p]; }

  private:

    unsigned _num_leafnodes;

    void handleLeafInputs();
    void handleLeafOutputs();
    void handleRootInput();
    void handleRootOutput();
    unsigned route(Packet* p);

    void LoopBack();

    // ports
    InPortBase<Packet*>*  root_inport;
    OutPortBase<Packet*>* root_outport;

    std::vector< InPortBase<Packet*>* >  leaf_inports;
    std::vector< OutPortBase<Packet*>* > leaf_outports;

    fifo<Packet*> outbound;
    fifo<Packet*> inbound;

};


#endif
