
#ifndef __TILE_BASE_H__
#define __TILE_BASE_H__

#include <cassert>

#include "sim.h"
#include "sim/componentbase.h"
#include "util/construction_payload.h"
#include "util/checkpointable.h"

extern ComponentCount TileBaseCount;

class TileBase : public ComponentBase, public rigelsim::Checkpointable
{
  public:
    TileBase(ComponentBase *parent, const char *name) :
      ComponentBase( parent,
                     name,
                     TileBaseCount )
    { }

    virtual ~TileBase()        { }
    virtual int  PerCycle()    = 0;
    virtual void EndSim()      = 0;
    virtual void PreSimInit()  = 0;
    virtual void Dump()        = 0;
    virtual int  halted()      = 0;
    virtual void Heartbeat()   = 0;
	  //FIXME We may not want this, I just hacked it in to get profiler output for the time being.
	  //Note that it's not pure virtual, so all derived classes don't *have* to override it. 
    virtual rigel::ClusterType** getClusters() const { 
	  	assert(0 && "Sorry, your TileBase derived class didn't override getClusters()");
	  }
    virtual void save_state() const = 0;
    virtual void restore_state() = 0;
  private:

};

#endif
