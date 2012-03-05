
#ifndef __CORE_FUNCTIONAL_H__
#define __CORE_FUNCTIONAL_H__

#include "corebase.h"
#include "core/regfile.h"
#include "sim.h"

// forward declarations
class PipePacket;
class RegisterFile;
namespace rigel {
    class ConstructionPayload;
}
class Syscall;
class ClusterCacheFunctional; // TODO: FIXME: remove direct interface

///////////////////////////////////////////////////////////////////////////////
/// CoreFunctional
///////////////////////////////////////////////////////////////////////////////
///
/// A functional core model
class CoreFunctional : public CoreBase {

  public:
    /// constructor
    CoreFunctional(rigel::ConstructionPayload cp,
                   ClusterCacheFunctional* ccache);

    /// destructor
    ~CoreFunctional();

    /// process one instruction per cycle per issue width
    int PerCycle();

    /// dump useful internal information about this object
    void Dump();

    /// heartbeat
    void Heartbeat();

    /// called at termination of simulation
    void EndSim();

    virtual void save_state() const;
    virtual void restore_state();

    PipePacket* fetch( uint32_t pc, int tid );
    void     decode(  PipePacket* instr );
    void     execute( PipePacket* instr );
    void     regfile_read( PipePacket* instr );
    void     writeback( PipePacket* instr );

  public:
  
    /// accessors 
    uint32_t pc(int tid)     { return thread_state[tid]->pc_; } 

  private:
   
    int width;      /// issue width
    int numthreads; /// numthreads

    struct CoreFunctionalThreadState {

      CoreFunctionalThreadState(
        int numregs, int numsprfregs
      ) :
        rf(numregs),
        sprf(numsprfregs),
        pc_(0)
      { };

      RegisterFile rf;
      SPRegisterFile sprf;

      uint32_t pc_; /// pc
    };


    int current_tid; // local thread id

    ClusterCacheFunctional* ccache;

    // TODO FIXME: this should be a core base class member...
    Syscall* syscall_handler;

    std::vector<CoreFunctionalThreadState*> thread_state;

};

#endif

