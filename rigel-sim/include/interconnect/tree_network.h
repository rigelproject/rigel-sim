#ifndef __TREE_NETWORK__
#define __TREE_NETWORK__

#include "sim/componentbase.h"
#include "util/construction_payload.h"

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
      rigel::ConstructionPayload &cp,
      ComponentCount& count = TreeNetworkCount
    );

    void Dump()      {};
    void Heartbeat() {};
    void EndSim()    {};
    int PerCycle()   {};

  private:

    int _num_leafnodes;

    InPortBase<Packet*>*  root_inport;
    OutPortBase<Packet*>* root_outport;

    std::vector< InPortBase<Packet*>* >  leaf_inports;
    std::vector< OutPortBase<Packet*>* > leaf_outports;

};


#endif
