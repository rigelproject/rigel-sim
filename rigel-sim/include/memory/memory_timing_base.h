#ifndef __MEMORY_TIMING_BASE_H__
#define __MEMORY_TIMING_BASE_H__

#include "sim/componentbase.h"

typedef uint32_t addr_t;
typedef uint32_t word_t;

class MemoryTimingBase : public ComponentBase {
  public:
    MemoryTimingBase( ComponentBase* parent ) :
      ComponentBase( parent )
    { };
    virtual int  PerCycle()    = 0;
    virtual void EndSim()      = 0;
    virtual void PreSimInit()  = 0;
    virtual void Dump()        = 0;
    virtual int  halted() = 0;
    virtual void Heartbeat()   = 0;
};

#endif //#ifndef __MEMORY_TIMING_BASE_H__
