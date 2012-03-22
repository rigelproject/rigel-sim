#include "interconnect/tree_network.h"
#include "port/port.h"


ComponentCount TreeNetworkCount;

TreeNetwork::TreeNetwork(
  rigel::ConstructionPayload &cp,
  ComponentCount& count
) :
  ComponentBase(cp.parent,
                cp.change_name("TreeNetwork").component_name.c_str(), 
                count),
  _num_leafnodes(rigel::CLUSTERS_PER_TILE), // FIXME: make this dynamic
  root_inport(NULL),
  root_outport(NULL),
  leaf_inports(_num_leafnodes),
  leaf_outports(_num_leafnodes)
{ 

  // construct ports
  root_inport  = new InPortBase<Packet*>( PortName(name(), id(), "root_in") );  
  root_outport = new OutPortBase<Packet*>( PortName(name(), id(), "root_in") );  

  for (int i = 0; i < leaf_inports.size(); i++) {
    leaf_inports[i] = new InPortBase<Packet*>( PortName(name(), id(), "leaf_in", i) );
  } 

  for (int i = 0; i < leaf_outports.size(); i++) {
    leaf_outports[i] = new OutPortBase<Packet*>( PortName(name(), id(), "leaf_in", i) );
  }
}

