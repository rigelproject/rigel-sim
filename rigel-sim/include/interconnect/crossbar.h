#ifndef __CROSSBAR_H__
#define __CROSSBAR_H__

#include "sim/component_base.h"
#include "util/construction_payload.h"

extern ComponentCount CrossBarCount;

//forward declarations
class Packet;
template<class T> class InPortBase;
template<class T> class OutPortBase;
template<class> class fifo;

namespace rigel {
  class ConstructionPayload;
}

class CrossBar : public ComponentBase {

  public:

    CrossBar(
      rigel::ConstructionPayload cp,
      ComponentCount& count = CrossBarCount
    );

    /// component interface
    /// required interface
    void Dump()      {};
    void Heartbeat() {};
    void EndSim()    {};
    int PerCycle();

    InPortBase<Packet*>* get_inport(int p) { return inports[p]; }
    OutPortBase<Packet*>* get_outport(int p) { return outports[p]; }

  private:

    const int _numports; ///< for now, number of inputs and outputs is the same
    const int _numinports;
    const int _numoutports;

    void readInports();
    void setOutports();
    void route();

    std::vector< InPortBase<Packet*>* > inports;
    std::vector< OutPortBase<Packet*>* > outports;

    std::vector< fifo<Packet*> > inbound;
    std::vector< fifo<Packet*> > outbound;

};


#endif
