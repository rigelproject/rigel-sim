#ifndef __CROSSBAR_H__
#define __CROSSBAR_H__

#include "sim/componentbase.h"
#include "util/construction_payload.h"

extern ComponentCount CrossBarCount;

//forward declarations
class Packet;
template<class T> class InPortBase;
template<class T> class OutPortBase;

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
    int PerCycle()   {};

  private:

    int _numports;

    std::vector< InPortBase<Packet*>* > inports;
    std::vector< OutPortBase<Packet*>* > outports;

};


#endif
