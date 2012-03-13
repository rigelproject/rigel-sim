
#ifndef __CORE_FUNCTIONAL_H__
#define __CORE_FUNCTIONAL_H__

#include "corebase.h"
#include "core/regfile.h"
#include "sim.h"
#include "port.h"


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

    /// FIXME TODO REMOVE ME HACK: replace with general connection interface
    OutPortBase<Packet*>* getOutPort() {return &to_ccache;}
    InPortBase<Packet*>*  getInPort()  { return &from_ccache; }

  private:

    // private methods
    regval32_t doMemoryAccess(Packet* p);

    regval32_t doALU(PipePacket* instr);
    regval32_t doFPU(PipePacket* instr);
    regval32_t doShift(PipePacket* instr);
    regval32_t doCompare(PipePacket* instr);

    regval32_t doMem(PipePacket* instr);

    void       doSimSpecial(PipePacket* instr);

    regval32_t doBranch(PipePacket* instr, uint32_t& branch_target, bool& temp_predicate);

   
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

    OutPortBase<Packet*> to_ccache;
    InPortBase<Packet*>  from_ccache;

    std::vector<CoreFunctionalThreadState*> thread_state;

};

#endif

