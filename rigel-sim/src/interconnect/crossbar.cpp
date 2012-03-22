
#include "interconnect/crossbar.h"
#include "port/port.h"

ComponentCount CrossBarCount;

CrossBar::CrossBar(
      rigel::ConstructionPayload cp,
      ComponentCount& count
) :
  ComponentBase(cp.parent,
                cp.change_name("CrossBar").component_name.c_str(),
                count),
  _numports(rigel::NUM_TILES), // FIXME: set dynamically
  inports(_numports),
  outports(_numports)
{ 
  // instantiate ports
  // TODO: do elsewhere, and connect
  for (int i = 0; i < inports.size(); i++) {
    inports[i] = new InPortBase<Packet*>( PortName(name(), id(), "in", i) );
  } 

  for (int i = 0; i < outports.size(); i++) {
    outports[i] = new OutPortBase<Packet*>( PortName(name(), id(), "out", i) );
  }
}

