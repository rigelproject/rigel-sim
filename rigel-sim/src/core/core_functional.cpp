
//#include "src/stages/decode_switch.h"
#include "core/core_functional.h"
#include "instr.h"
#include "util/syscall.h"
#include "sim.h"
#include "util/util.h"
#include "memory/backing_store.h"
#include "core/regfile.h"
#include "util/construction_payload.h"
#include "packet/packet.h"

#include "isa/rigel_isa.h"

#include "util/rigelprint.h"

#include "cluster/cluster_cache_functional.h" // FIXME: remove this dep!


#define DB_CF 0
#define DB_CF_WB 0
#define DB_CF_ST 0
#define DB_CF_LD 0
#define DB_CF_LDL 0
#define DB_CF_STC 0
#define DB_CF_MEM 0

#define DB_CF_SYSCALL 0

#define CF_WIDTH 1

#define LOCALID ( id()%rigel::CORES_PER_CLUSTER * numthreads )
#define GTID(localtid) ( id() * numthreads + localtid )

///////////////////////////////////////////////////////////////////////////////
/// constructor
///////////////////////////////////////////////////////////////////////////////
CoreFunctional::CoreFunctional(
  rigel::ConstructionPayload cp,
  ClusterCacheFunctional *ccache
) :
  CoreBase(cp.change_name("CoreFunctional")),
  width(CF_WIDTH),
  numthreads(rigel::THREADS_PER_CORE),
  current_tid(0),
  ccache(ccache),
  syscall_handler(cp.syscall),
  to_ccache(),
  from_ccache(),
  icache(),
  thread_state(numthreads)
{
  cp.parent = this;
	cp.component_name.clear();

  // per thread init
  thread_state.resize(numthreads);
  for (int t=0; t < numthreads; t++) {

    active_tids.set(t); // this this thread active

    thread_state[t] = new CoreFunctionalThreadState(rigel::NUM_REGS, rigel::NUM_SREGS);
    CoreFunctionalThreadState *ts = thread_state[t];

    if (rigel::DUMP_REGISTER_TRACE) {
      char filename[128];
      // set up RF tracing if requested
      sprintf(filename,"%s/rf.%d.%d.trace", rigel::DUMPFILE_PATH.c_str(), id(), t);
      ts->rf.setTraceFile(filename);
      ts->sprf.setTraceFile(filename);
    }

    // configure this core's SPRF settings as necessary
    // TODO FIXME: these need to be set DYNAMICALLY, not via rigel
    ts->sprf.assign(SPRF_CORE_NUM,            id());
    // TODO FIXME: set this via counter to ensure monotonic unique tids?
    ts->sprf.assign(SPRF_THREAD_NUM,          id() * numthreads + t); 
    ts->sprf.assign(SPRF_CLUSTER_ID,          parent()->id());
    ts->sprf.assign(SPRF_THREADS_PER_CORE,    numthreads);
    ts->sprf.assign(SPRF_NUM_CLUSTERS,        rigel::NUM_CLUSTERS);
    ts->sprf.assign(SPRF_CORES_PER_CLUSTER,   rigel::CORES_PER_CLUSTER);
    ts->sprf.assign(SPRF_THREADS_PER_CLUSTER, rigel::THREADS_PER_CLUSTER);
    ts->sprf.assign(SPRF_CORES_TOTAL,         rigel::CORES_TOTAL);
    ts->sprf.assign(SPRF_THREADS_TOTAL,       rigel::THREADS_TOTAL);
  }
  

}

/// destructor
CoreFunctional::~CoreFunctional() {
  for (int t=0; t<numthreads; t++) {
    delete thread_state[t];
  }
}

///////////////////////////////////////////////////////////////////////////////
/// PerCycle()
///////////////////////////////////////////////////////////////////////////////
/// process one instruction each cycle per issue-width
/// simple functional execution of each instruction
int
CoreFunctional::PerCycle() {

  if (halted()) {
    return halted();
  }

  //if (sprf[SPRF_SLEEP].u32()) {
  //  return 0;
  //}

  CoreFunctionalThreadState *ts;

  // select a thread for this cycle
  current_tid = thread_select();

  if (current_tid >= 0) {

    ts = thread_state[current_tid];

    for(int i=0; i<width; i++) {

      if (!ts->instr) { // if there is not an existing instruction
        // fetch
        ts->instr = fetch(ts->pc_, current_tid);
        // decode
        decode( ts->instr );
        // read operands from regfile (no bypassing in functional core)
        regfile_read( ts->instr );
        // execute
        execute( ts->instr );
        // memory access (if applicable)
        memory(ts->instr);  
      } else { // check on the existing instruction
        // handle memory stall
        checkMemoryRequest(ts->instr);
      }
      
      // writeback
      if (ts->instr->isCompleted()) {

        writeback( ts->instr );
      
        ts->pc_ = ts->instr->nextPC();

        delete ts->instr; // don't forget to clear out that pesky dynamic object...
        ts->instr = 0; // no valid instruction
      } else {
        DPRINT(false, "instruction incomplete!\n");
        //DRIGEL(instr->Dump());
        //throw ExitSim("not allowed to have incomplete instructions now");
      }

    }

  } else {
    // no thread available this cycle, stalled
  }

  if (DB_CF) { ts->rf.Dump(); }

  return halted();

}

// thread_select()
// select a thread to issue from
int
CoreFunctional::thread_select() {

  // if there is an incomplete instruction stalled, wait for it to finish
  if (thread_state[current_tid]->instr) {
    return current_tid;
  }

  int next_tid = current_tid + 1;
  for (int i = 0; i < numthreads; i++) {
    next_tid = (next_tid + i) % numthreads;
    // find a thread that is ready to execute (not halted,stalled)
    if ( !halted(next_tid) && !thread_state[next_tid]->stalled ) {
      return next_tid; // select this thread 
    }
  }
  printf("no unstalled threads available this cycle!\n");
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
PipePacket*
CoreFunctional::fetch( uint32_t pc, int tid ) {

  DPRINT(DB_CF,"%s\n",__func__);

  uint32_t data = mem_backing_store->read_instr_word(pc);
  PipePacket* instr = new PipePacket(pc,data,tid);
  //instr->Dump();
  return instr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void
CoreFunctional::decode( PipePacket* instr ) {
  // PipePacket decodes on instruction presently
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void
CoreFunctional::regfile_read( PipePacket* instr ) {

  // use thread state
  CoreFunctionalThreadState *ts = thread_state[instr->tid()];

  for (int r = 0; r < NUM_ISA_OPERAND_REGS; ++r) {
    isa_reg_t rr = (isa_reg_t)r;
    uint32_t regnum = instr->input_deps(rr);
    if (regnum == simconst::NULL_REG) {
      DPRINT(DB_CF,"skipping NULL_REG\n");
      continue;
    }
    regval32_t v;
    if (instr->isSPRFSrc()) {
      v = ts->sprf[regnum];
      DPRINT(DB_CF,"[%d] sprf regnum [%d] is %08x\n",id(),regnum,v.u32());
    } else {
      v = ts->rf[regnum];
      DPRINT(DB_CF,"rf regnum [%d]\n is %08x\n",regnum,v.u32());
    }
    instr->setRegVal( rr, v ); 
    DPRINT(DB_CF,"%s regval %s: 0x%08x\n", __func__, isa_reg_names[rr],v.u32());
  }
  //instr->Dump();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void
CoreFunctional::writeback( PipePacket* instr ) {

  DPRINT(DB_CF_WB,"%s\n",__func__);

  // use thread state
  CoreFunctionalThreadState *ts = thread_state[instr->tid()];

  if ( instr->isDREGDest() ) {
    
    assert( instr->regval(DREG).valid() );
    if (!instr->regval(DREG).valid()) { 
      instr->Dump();
      throw ExitSim("invalid value on writeback"); 
    }

    // SPRF writes (rare)
    if (instr->isSPRFDest()) {
      DPRINT(DB_CF /*DB_CF_WB*/,"[%d] SPRF write: pc rf val %08x %d %08x\n",
        id(), instr->pc(), instr->regnum(DREG), instr->regval(DREG).u32());
      assert(instr->regnum(DREG) < rigel::NUM_SREGS && "DREG sprf id out of range!");
      ts->sprf.write(instr->regnum(DREG), instr->regval(DREG), instr->pc());
    } 
    // normal register file
    else {
      //DPRINT(DB_CF,"%08x %d %08x\n",
      DPRINT(DB_CF_WB,"[%d] RF write: pc rf val %08x %d %08x\n",
        id(), instr->pc(), instr->regnum(DREG), instr->regval(DREG).u32());
      assert(instr->regnum(DREG) < rigel::NUM_REGS && "DREG rf id out of range!");
      ts->rf.write(instr->regnum(DREG), instr->regval(DREG), instr->pc());
    }
  }

  if (DB_CF) {instr->Dump();}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void
CoreFunctional::execute( PipePacket* instr ) {

  CoreFunctionalThreadState *ts = thread_state[instr->tid()];

  DPRINT(DB_CF,"%s\n",__func__);

  uint32_t incremented_pc = instr->pc() + 4;
  uint32_t next_pc        = incremented_pc;

  if (instr->input_deps(SREG_T) != simconst::NULL_REG) { 
    assert( instr->sreg_t().valid() );
  }
  if (instr->input_deps(SREG_S) != simconst::NULL_REG) {
    assert( instr->sreg_s().valid() );
  }
  if (instr->input_deps(DREG)   != simconst::NULL_REG) {
    assert( instr->dreg().valid() );
  }

  if (id()==0 && instr->output_deps() != simconst::NULL_REG) {
    //printf("\n>>>\n instruction has an output dep: %d\n", instr->output_deps());
    //instr->Dump();
    //printf("<<<\n\n");
  }

  DPRINT(false,"exec: sreg_t:0x%08x sreg_s:0x%08x\n zimm16:%08x simm16:%08x imm5%d",
    instr->sreg_t().u32(), instr->sreg_s().u32(), 
    instr->imm16(), instr->simm16(), instr->imm5());

  // FIXME: switch statement based on pre-decoded type in sdInfo?
  // TODO:  switches could be on per-type enums (ALU, FPU, mem, etc) 
  //  like RTL rather than full I_TYPE enum
  
  // ALU
  if (instr->isALU()) {
    DPRINT(DB_CF,"%s: isALU\n", __func__);
    RigelISA::execALU(instr);
  }
  // shift
  else if (instr->isShift()) {
    DPRINT(DB_CF,"%s: isALU\n", __func__);
    RigelISA::execShift(instr);
  }
  // FPU
  else if (instr->isFPU()) {
    DPRINT(DB_CF,"%s: isFPU\n", __func__);
    RigelISA::execFPU(instr);
  }
  // Memory (address computation here)
  else if (instr->isMem()) {
    RigelISA::execAddressGen(instr);
  }
  // Comparisons
  else if (instr->isCompare()) {
    DPRINT(DB_CF,"%s: isCompare\n", __func__);
    RigelISA::execCompare(instr);
  }
  // instructions that modify the PC in some way
  else if (instr->isBranch()) {
    DPRINT(DB_CF,"%s: isBranch\n", __func__);
    RigelISA::execBranch(instr);
  }
  // Sim special ops
  else if (instr->isSimSpecial()) {
    doSimSpecial(instr);
  }
  // other instruction types
  else if (instr->isOther()) {
    DPRINT(DB_CF,"%s: isOther\n", __func__);
    instr->Dump();
    throw ExitSim("unhandled 'isOther' instruction type");
  }
  else {
    DPRINT(true,"%s: UNKNOWN!\n", __func__);
    instr->Dump();
    throw ExitSim("unhandled instruction type"); 
  }
 
  // select the next PC 
  if( instr->branch_predicate() ) {
    instr->nextPC(instr->target_addr());
  } else {
    instr->nextPC(next_pc);
  }

  //instr->Dump();
}


// memory 
// memory access 'stage'
void
CoreFunctional::memory(PipePacket* instr) {
  if (instr->isMem()) {
    doMem(instr);
  }
}

///////////////////////////////////////////////////////////////////////////////
/// Dump
///////////////////////////////////////////////////////////////////////////////
/// dump information about this object
void 
CoreFunctional::Dump() {
  printf("%s[%d]::%s\n",name(),id(),__func__);
  for (int t=0; t<numthreads; t++) {
    printf("tid[%d]\n",t);
    CoreFunctionalThreadState *ts = thread_state[t];
    printf("pc: %08x \n", ts->pc_);
    printf("halted: %d \n", halted());
    ts->rf.Dump();
    ts->sprf.Dump();
  }
}



void 
CoreFunctional::doMem(PipePacket* instr) {

  rword32_t sreg_t = instr->regval(SREG_T);
  rword32_t sreg_s = instr->regval(SREG_S);

  uint32_t addr = instr->target_addr();

  // new packet based on instr type
  Packet* p = new Packet( rigel::instr_to_icmsg_full(instr->type()) );

  if (instr->isAtomic()) {
    DPRINT(DB_CF,"%s: isMem isAtomic\n", __func__);

    // split me into Local and Global atomic sections?
    switch (instr->type()) {
      // local atomics
      // these require intra-cluster synchronization 
      // (or for alternate implementations at least _some_ cross-core synch)
      case I_LDL:
        // perform a regular LDW, but also set a link bit
        p->initCorePacket(addr, 0, id(), GTID(instr->tid()));
        break;
      case I_STC:
        // perform a store only if the link bit is set
        p->initCorePacket(addr, sreg_s.u32, id(), GTID(instr->tid()));
        break;
      // atomics that return a copy of the OLD value before atomic update
      case I_ATOMCAS:
        p->initCorePacket(addr, instr->regval(DREG).u32(), id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32);
        break;
      case I_ATOMXCHG:
        p->initCorePacket(addr, instr->regval(DREG).u32(), id(), GTID(instr->tid()));
        break;
      // arithmetic
      case I_ATOMINC: 
      case I_ATOMDEC:
        p->initCorePacket(addr, 0, id(), GTID(instr->tid()));
        break;
      case I_ATOMADDU:
      // atomics that return the NEW value after atomic update
      // min,max
      case I_ATOMMIN: 
      case I_ATOMMAX: 
      // logical
      case I_ATOMAND:
      case I_ATOMOR:
      case I_ATOMXOR:
        p->initCorePacket(addr, 0, id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32);
        break;
      default:
        instr->Dump();
        assert(0 && "unknown or unimplemented ATOMICOP");
        break;
    }
    sendMemoryRequest(p);
  // store value
  }
  else if (instr->isStore()) {
    DPRINT(DB_CF,"%s: isMem isStore\n", __func__);

    switch (instr->type()) {
      case I_STW:
      case I_GSTW:
      case I_BCAST_UPDATE: {
        uint32_t data        = instr->regval(DREG).u32();
        p->initCorePacket(addr ,data, id(), GTID(instr->tid()));
        sendMemoryRequest(p); // no result for stores
        break;
      }
      default:
        instr->Dump();
        assert(0 && "unknown STORE");
        break;
    }
  }
  else if (instr->isLoad()) {
    DPRINT(DB_CF,"%s: isMem isLoad\n", __func__);
    switch (instr->type()) {
      case I_LDW:
      case I_GLDW: {
        p->initCorePacket(addr, 0, id(), GTID(instr->tid()));
        sendMemoryRequest(p);
        break;
      }
      default:
        instr->Dump();
        assert(0 && "unknown LOAD");
        break;
    }
  } 
  else if (instr->isCacheControl()) {
    switch (instr->type()) {
      case I_LINE_WB:
      case I_LINE_INV:
      case I_LINE_FLUSH:
        // do nothing for these in functional mode, for now, with no caches
        instr->setCompleted();
        DRIGEL( printf("ignoring CACHECONTROL instruction for now...NOP\n"); )
        break;
      default:
        throw ExitSim("unhandled CacheControl operation?");
    }
  } else if (instr->isPrefetch()) {
    instr->setCompleted();
    DRIGEL( printf("ignoring PREFETCH instruction for now...NOP\n"); )
  } else {
    instr->Dump();
    throw ExitSim("unknown memory operation!");
  }

  instr->memRequest(p); // save pointer to request
  if (instr->isCompleted()) {
    return;
  } else {
    checkMemoryRequest(instr);
  }

}

// sendMemoryRequest
// actually poke the ports
void
CoreFunctional::sendMemoryRequest(Packet* p) {

  port_status_t status;

  status = to_ccache.sendMsg(p);

  if (status == ACK) { // port accepted message

  } else {
    throw ExitSim("unhandled path sendMemoryRequest()");
  }

}

void
CoreFunctional::checkMemoryRequest(PipePacket* instr) {

  Packet* reply;
  regval32_t result;
  Packet* p = instr->memRequest();

  reply = from_ccache.read();

  if (reply) { // handle reply
    DPRINT(DB_CF_MEM,"got reply! %d\n", reply->msgType());
    // FIXME: some operations DO NOT HAVE a result!
    assert( reply == p ); // for now

    // check response types
    if (reply->msgType()==IC_MSG_STC_REPLY_NACK) {
      DPRINT(DB_CF_STC,"[STC: fail]");
    } else if (reply->msgType()==IC_MSG_STC_REPLY_ACK) {
      DPRINT(DB_CF_STC,"[STC: success]");
    }
    // TODO: FIXME: re-enable checking for _all_ proper message request/reply pairs
    //else {
    //  throw ExitSim("Invalid response to STC request!\n");
    //}

    if (reply->isCompleted()) {
      result = reply->data();
      instr->setRegVal(DREG, result); // FIXME: NOT ALL have results! 
      instr->setCompleted(); // just for now...
      delete p; // because we are done (for now)
    } else {
      throw ExitSim("unimplemented for incompleted operation\n");
    }

  } else { 
    DPRINT(DB_CF_MEM,"no reply yet core:%d tid:%d\n", id(), instr->tid());
    // NULL message means we have nothing to process
    //instr->Dump();
    //p->Dump();
    //throw ExitSim("unhandled path checkMemoryRequest()");
  }

}

void
CoreFunctional::doSimSpecial(PipePacket* instr) {
  switch (instr->type()) {
    case I_PRINTREG:
      rigel::RigelPRINTREG(
        instr->pc(), instr->regval(DREG), id(), LOCALID + instr->tid() ); 
      break;
    case I_SYSCALL: {
      uint32_t addr = instr->regval(DREG).u32(); // Pointer to syscall_struct
      rigel::RigelSYSCALL(addr,syscall_handler,mem_backing_store); break; 
    }
    case I_EVENT:
      DRIGEL( printf("ignore I_EVENT event instruction for now...NOP\n"); )
      break;
    case I_NOP:
      break;
    case I_HLT:
      printf("Halting Core %d @ pc %08x @ cycle %" PRIu64 "\n", id(), instr->pc(), rigel::CURR_CYCLE);
      halt(instr->tid()); // halt this thread
      break;
    case I_ABORT:
      instr->Dump();
      char str[128];
      sprintf(str, "ABORT @pc 0x%08x, id:%d\n", instr->pc(), id() );
      throw ExitSim(str);
    default:
      instr->Dump();
      throw ExitSim("unhandled SimSpecial instruction type");
  }
  instr->setCompleted();
}

///////////////////////////////////////////////////////////////////////////////
/// heartbeat
///////////////////////////////////////////////////////////////////////////////
void 
CoreFunctional::Heartbeat() {
}

///////////////////////////////////////////////////////////////////////////////
/// called at termination of simulation
///////////////////////////////////////////////////////////////////////////////
void 
CoreFunctional::EndSim() {
}

void CoreFunctional::save_state() const {
  //rigelsim::ThreadState *ts = core_state->add_threads();
  //assert(0 && "patch me for multithreading and test!");
  //rf.save_to_protobuf(ts->mutable_rf());
  //sprf.save_to_protobuf(ts->mutable_sprf());
}

void CoreFunctional::restore_state() {
  
}
