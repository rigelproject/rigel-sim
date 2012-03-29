
#ifndef __TILE_BASE_H__
#define __TILE_BASE_H__

#include <cassert>

#include "sim.h"
#include "sim/component_base.h"
#include "util/construction_payload.h"
#include "util/checkpointable.h"

class Packet;
template <class T> class InPortBase;
template <class T> class OutPortBase;

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

    virtual InPortBase<Packet*>* get_inport()   { return NULL; }
    virtual OutPortBase<Packet*>* get_outport() { return NULL; }

	  //FIXME We may not want this, I just hacked it in to get profiler output for the time being.
	  //Note that it's not pure virtual, so all derived classes don't *have* to override it. 
    virtual rigel::ClusterType** getClusters() const { 
	  	assert(0 && "Sorry, your TileBase derived class didn't override getClusters()");
			return (rigel::ClusterType**) NULL;
	  }

    virtual void save_state() const = 0;
    virtual void restore_state() = 0;

  private:

};

#endif
