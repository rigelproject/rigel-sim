
#ifndef __CORE_FUNCTIONAL_H__
#define __CORE_FUNCTIONAL_H__

#include "core_base.h"
#include "core/regfile.h"
#include "sim.h"
#include "port/port.h"


// forward declarations
class PipePacket;
class RegisterFile;
class Packet;
class regval32_t;
template<class T> class InPortBase;
template<class T> class OutPortBase;
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
    CoreFunctional(rigel::ConstructionPayload cp);

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

  private:
    int  thread_select();
    PipePacket* fetch( uint32_t pc, int tid );
    void     decode(  PipePacket* instr );
    void     regfile_read( PipePacket* instr );
    void     execute( PipePacket* instr );
    void     memory( PipePacket* instr );
    void     writeback( PipePacket* instr );

  public:
  
    /// accessors 
    uint32_t pc(int tid)     { return thread_state[tid]->pc_; } 

    /// FIXME TODO REMOVE ME HACK: replace with general connection interface
    OutPortBase<Packet*>* getOutPort() { return to_ccache;   }
    InPortBase<Packet*>*  getInPort()  { return from_ccache; }

  private:

    int stall;

    // private methods
    void doMem(PipePacket* instr);
    void sendMemoryRequest(PipePacket* p);
    void checkMemoryRequest(PipePacket* p);

    void doSimSpecial(PipePacket* instr);

    int width;      /// issue width
    int numthreads; /// numthreads

    struct CoreFunctionalThreadState {

      CoreFunctionalThreadState(
        int numregs, int numsprfregs
      ) :
        rf(numregs),
        sprf(numsprfregs),
        pc_(0), 
        instr(0),
        stalled(0)
      { };

      RegisterFile rf;
      SPRegisterFile sprf;

      uint32_t pc_; /// pc

      PipePacket* instr; /// for a simple model, hold the current instr

      bool stalled;
    };


    int current_tid; // local thread id

    //ClusterCacheFunctional* ccache;

    // TODO FIXME: this should be a core base class member...
    Syscall* syscall_handler;

    OutPortBase<Packet*>* to_ccache;
    InPortBase<Packet*>*  from_ccache;
    InPortBase<Packet*>*  icache;

    std::vector<CoreFunctionalThreadState*> thread_state;

};

#endif

