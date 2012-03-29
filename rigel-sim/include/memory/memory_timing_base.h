#ifndef __MEMORY_TIMING_BASE_H__
#define __MEMORY_TIMING_BASE_H__

#include <set>
#include <stdint.h>
#include "sim.h" //For rigel::cache::LINESIZE
#include "sim/component_base.h"

class CallbackInterface;
template <int linesize> class MissHandlingEntry;

class MemoryTimingBase : public ComponentBase {
  public:
    MemoryTimingBase( ComponentBase *parent ) :
      ComponentBase( parent )
    { };
    virtual int  PerCycle()    = 0;
    virtual void EndSim()      = 0;
    virtual void PreSimInit()  = 0;
    virtual void Dump()        = 0;
    virtual int  halted() { return 0; }
    virtual void Heartbeat()   = 0;
    virtual bool Schedule(const std::set<uint32_t> addrs,
                                             int size,
                                             const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                                             CallbackInterface *requestingEntity,
                                             int requestIdentifier) = 0;
};

#endif //#ifndef __MEMORY_TIMING_BASE_H__
