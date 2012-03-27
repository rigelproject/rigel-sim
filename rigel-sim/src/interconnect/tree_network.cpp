#include "interconnect/tree_network.h"
#include "port/port.h"
#include "packet/packet.h"

#include "util/fifo.h"

#define DB_TREE 1

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
  leaf_outports(_num_leafnodes),
  outbound(99),
  inbound(99)
{ 

  // construct ports
  for (unsigned i = 0; i < leaf_inports.size(); i++) {
    leaf_inports[i] = new InPortBase<Packet*>( PortName(name(), id(), "leaf_in", i) );
  } 
  for (unsigned i = 0; i < leaf_outports.size(); i++) {
    leaf_outports[i] = new OutPortBase<Packet*>( PortName(name(), id(), "leaf_out", i) );
  }
}

int 
TreeNetwork::PerCycle() {
  DPRINT(DB_TREE,"%s\n",__PRETTY_FUNCTION__);

  // LoopBack();

  handleLeafOutputs();

  handleRootInput();

  handleRootOutput();

  handleLeafInputs();

  return 0;

}

void
TreeNetwork::handleLeafInputs() {
  DPRINT(DB_TREE,"%s\n",__PRETTY_FUNCTION__);

  for (unsigned i = 0; i < _num_leafnodes; i++) {
    Packet* p;
    if (p = leaf_inports[i]->read()) {
      DRIGEL(DB_TREE, p->Dump());
      inbound.push(p);
    }
  }

}

void
TreeNetwork::handleLeafOutputs() {
  DPRINT(DB_TREE,"%s\n",__PRETTY_FUNCTION__);

  if (!outbound.empty()) {
    Packet* p = outbound.front();
    DRIGEL(DB_TREE, p->Dump());
    unsigned port = route(p);
    if (leaf_outports[port]->sendMsg(p) == ACK) {
      outbound.pop();
    } else {
      throw ExitSim("unhandled sendMsg failure");
    }
  }

}

// route a return packet down the tree
unsigned
TreeNetwork::route(Packet* p) {
  DPRINT(DB_TREE,"%s\n",__PRETTY_FUNCTION__);

  unsigned port;

  int tid = p->gtid();

  int tile = tid / (rigel::CLUSTERS_PER_TILE * rigel::THREADS_PER_CLUSTER);

  int cluster = tid / rigel::THREADS_PER_CLUSTER;

  port = cluster % rigel::CLUSTERS_PER_TILE;

  //printf("tile: %d cluster: %d port: %d tid: %d \n", tile, cluster, port, tid);

  assert(tile == id() && "message for wrong tile");
  if (tile != id()) {
    throw ExitSim("message for wrong tile!");
  }

  return port;

}

void
TreeNetwork::handleRootInput() {
  DPRINT(DB_TREE,"%s\n",__PRETTY_FUNCTION__);

  Packet* p;
  if (p = root_inport->read()) {
    DRIGEL(DB_TREE, p->Dump());
    outbound.push(p);
  }

}

void
TreeNetwork::handleRootOutput() {
  DPRINT(DB_TREE,"%s\n",__PRETTY_FUNCTION__);

  if (!inbound.empty()) {
    Packet* p = inbound.front();
    if (root_outport->sendMsg(p) == ACK) {
      inbound.pop();
    } else {
      throw ExitSim("this is broked");
    }
  }

}

void
TreeNetwork::LoopBack() {
  DPRINT(DB_TREE,"%s\n",__PRETTY_FUNCTION__);

  // loopback
  for (unsigned i = 0; i < _num_leafnodes; i++) {
    Packet* p;
    if (p = leaf_inports[i]->read()) {
      DRIGEL(DB_TREE, p->Dump());
      if (leaf_outports[i]->sendMsg(p) == ACK) {
      } else {
        throw ExitSim("this is broked");
      }
    }
  }

}

