#include "interconnect/tree_network.h"
#include "port/port.h"


ComponentCount TreeNetworkCount;

TreeNetwork::TreeNetwork(
  rigel::ConstructionPayload cp,
  InPortBase<Packet*>* in,
  OutPortBase<Packet*>* out,
  ComponentCount& count
) :
  ComponentBase(cp.parent,
                cp.change_name("TreeNetwork").component_name.c_str(), 
                count),
  _num_leafnodes(rigel::CLUSTERS_PER_TILE), // FIXME: make this dynamic
  root_inport(in),
  root_outport(out),
  leaf_inports(_num_leafnodes),
  leaf_outports(_num_leafnodes)
{ 

  // construct ports
  for (int i = 0; i < leaf_inports.size(); i++) {
    leaf_inports[i] = new InPortBase<Packet*>( PortName(name(), id(), "leaf_in", i) );
  } 
  for (int i = 0; i < leaf_outports.size(); i++) {
    leaf_outports[i] = new OutPortBase<Packet*>( PortName(name(), id(), "leaf_out", i) );
  }
}

