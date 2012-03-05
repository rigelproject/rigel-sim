
#ifndef __CORE_BASE_H__
#define __CORE_BASE_H__

/// abstract base class for cores

#include "sim/componentbase.h"
#include "sim.h"
#include "util/checkpointable.h"
#include "rigelsim.pb.h"
#include "util/util.h"
#include <bitset>

namespace rigel {
  class ConstructionPayload;
}

extern ComponentCount CoreBaseCount;

class CoreBase : public ComponentBase, public rigelsim::Checkpointable {

  public:
    /// constructor
    CoreBase(
      rigel::ConstructionPayload &cp,
      ComponentCount& count = CoreBaseCount // add me to allow id overrides
    );

    /// destructor
    virtual ~CoreBase() { };

    ////////////////////////////////////////////////////
    /// Accessors
    ////////////////////////////////////////////////////
    
    /// get memory model pointer
    rigel::GlobalBackingStoreType *getBackingStore() {
      return mem_backing_store;
    }

    virtual void save_state() const = 0;
    virtual void restore_state() = 0;
    /// 
    virtual int halted()        { return halted_; }
    virtual int halted(int tid) { return !active_tids[tid]; }

    //
    virtual void halt(int tid) { 
      if (active_tids.test(tid)) {
        printf("halting core %d:%d\n", id(), tid);
        active_tids.reset(tid);
      } else {
        throw ExitSim("attempting to halt inactive thread!");
      }
      if (active_tids.none()) { 
        halted_ = true;
      }
    }

  protected:

    /// memory model pointer
    rigel::GlobalBackingStoreType *mem_backing_store;
    rigelsim::CoreState *core_state;
    std::bitset<32> active_tids;

  private:

    int halted_; // is this core halted? (no longer executing)

};

#endif
