
#include "sim.h"
#include "interconnect/crossbar.h"
#include "port/port.h"
#include "packet/packet.h"
#include "util/fifo.h"

#define DB_XBAR 1

ComponentCount CrossBarCount;

CrossBar::CrossBar(
      rigel::ConstructionPayload cp,
      ComponentCount& count
) :
  ComponentBase(cp.parent,
                cp.change_name("CrossBar").component_name.c_str(),
                count),
  _numports(rigel::NUM_TILES), // FIXME: set dynamically
  _numinports(_numports),
  _numoutports(_numports),
  inports(_numinports),
  outports(_numoutports),
  inbound(_numinports, fifo<Packet*>(99)),
  outbound(_numoutports, fifo<Packet*>(99))
{ 
  // instantiate ports
  // TODO: do elsewhere, and connect
  for (unsigned i = 0; i < inports.size(); i++) {
    inports[i] = new InPortBase<Packet*>( PortName(name(), id(), "in", i) );
  } 
  for (unsigned i = 0; i < outports.size(); i++) {
    outports[i] = new OutPortBase<Packet*>( PortName(name(), id(), "out", i) );
  }
}

int
CrossBar::PerCycle() {

  // set output ports
  setOutports();

  // route inputs
  // FIXME: this doesn't actually make the routing decision
  route(); 

  // read input ports
  readInports();

  return 0;
}

// this doesn't actually route right now
void
CrossBar::route() {
  DPRINT(DB_XBAR,"%s\n",__PRETTY_FUNCTION__);

  for (int i = 0; i < _numinports; i++) {
    if (!inbound[i].empty()) {
      Packet* p = inbound[i].front();
      DRIGEL(DB_XBAR, p->Dump();)
      outbound[i].push(p);
      inbound[i].pop();
    }
  }

}

void 
CrossBar::readInports() {
  DPRINT(DB_XBAR,"%s\n",__PRETTY_FUNCTION__);

  for (int i = 0; i < _numinports; i++) {
    Packet* p;
    if ((p = inports[i]->read()) != NULL) {
      DRIGEL(DB_XBAR, p->Dump();)
      inbound[i].push(p);
    }
  }

}

void 
CrossBar::setOutports() {
  DPRINT(DB_XBAR,"%s\n",__PRETTY_FUNCTION__);

  for (int i = 0; i < _numoutports; i++) {
    if (!outbound[i].empty()) {
      Packet* p = outbound[i].front();
      DRIGEL(DB_XBAR, p->Dump();)
      if (outports[i]->sendMsg(p) == ACK) {
        outbound[i].pop();
      }
    }
  }

}
