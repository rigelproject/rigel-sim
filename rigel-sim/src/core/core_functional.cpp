
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

#include "util/rigelprint.h"

#include "cluster/cluster_cache_functional.h" // FIXME: remove this dep!

#include <cmath>

#define DB_CF 0
#define DB_CF_WB 0
#define DB_CF_ST 0
#define DB_CF_LD 0
#define DB_CF_LDL 0
#define DB_CF_STC 0

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
      //assert(rigel::THREADS_PER_CORE==1 && "not multithreaded");
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

  // select a thread
  do {
    current_tid = (current_tid+1) % numthreads;
  } while(halted(current_tid));

  CoreFunctionalThreadState *ts = thread_state[current_tid];

  for(int i=0; i<width; i++) {

    // fetch
    PipePacket* instr;
    instr = fetch(ts->pc_, current_tid);

    // decode
    decode( instr );

    // read operands from regfile (no bypassing in functional core)
    regfile_read( instr );
    
    // execute
    execute( instr );
    
    // writeback
    writeback( instr );
    
    ts->pc_ = instr->nextPC();

    delete instr; // don't forget to clear out that pesky dynamic object...

  }

  if (DB_CF) { ts->rf.Dump(); }

  return halted();

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
PipePacket*
CoreFunctional::fetch( uint32_t pc, int tid ) {

  DPRINT(DB_CF,"%s\n",__func__);

  uint32_t data = mem_backing_store->read_word(pc);
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

  if (instr->regval(DREG).valid()) {
    // SPRF writes (rare)
    if (instr->isSPRFDest()) {
      DPRINT(DB_CF /*DB_CF_WB*/,"[%d] SPRF write: pc rf val %08x %d %08x\n",
        id(), instr->pc(), instr->regnum(DREG), instr->regval(DREG).u32());
      ts->sprf.write(instr->regnum(DREG), instr->regval(DREG), instr->pc());
    } 
    // normal register file
    else {
      DPRINT(DB_CF_WB,"[%d] RF write: pc rf val %08x %d %08x\n",
      //DPRINT(DB_CF,"%08x %d %08x\n",
        id(), instr->pc(), instr->regnum(DREG), instr->regval(DREG).u32());
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

  regval32_t sreg_t = instr->regval(SREG_T);
  regval32_t sreg_s = instr->regval(SREG_S);

  if( instr->input_deps(SREG_T) != simconst::NULL_REG ) { 
    assert( sreg_t.valid() );
  }
  if( instr->input_deps(SREG_S) != simconst::NULL_REG ) {
    assert( sreg_s.valid() );
  }
  if( instr->input_deps(DREG)   != simconst::NULL_REG ) {
    assert( instr->regval(DREG).valid() );
  }

  // macros would be more efficient here...
  uint32_t zimm16  = instr->imm16();  // zext(imm16)
  uint32_t simm16  = instr->simm16(); // sext(imm16)
  uint32_t imm5    = instr->imm5();   // imm5

  DPRINT(false,"exec: sreg_t:0x%08x sreg_s:0x%08x\n zimm16:%08x simm16:%08x imm5%d",
    sreg_t.u32(), sreg_s.u32(), zimm16, simm16, imm5);

  //
  regval32_t temp_result;

  uint32_t branch_target = 0;
  bool temp_predicate = false;

  // FIXME: switch statement based on pre-decoded type in sdInfo?
  // TODO:  switches could be on per-type enums (ALU, FPU, mem, etc) 
  //  like RTL rather than full I_TYPE enum
  
  // ALU
  if (instr->isALU()) {
    DPRINT(DB_CF,"%s: isALU\n", __func__);
    temp_result = doALU(instr);
  }
  // shift
  else if (instr->isShift()) {
    DPRINT(DB_CF,"%s: isALU\n", __func__);
    temp_result = doShift(instr);
  }
  // FPU
  else if (instr->isFPU()) {
    DPRINT(DB_CF,"%s: isFPU\n", __func__);
    temp_result = doFPU(instr);
  }
  // Memory
  else if (instr->isMem()) {
    temp_result = doMem(instr);
  }
  else if (instr->isCacheControl()) {
    switch (instr->type()) {
      case I_LINE_WB:
      case I_LINE_INV:
      case I_LINE_FLUSH:
        // do nothing for these in functional mode, for now, with no caches
        break;
      default:
        throw ExitSim("unhandled CacheControl operation?");
    }
  }
  // Comparisons
  else if (instr->isCompare()) {
    DPRINT(DB_CF,"%s: isCompare\n", __func__);
    temp_result = doCompare(instr);
  }
  // instructions that modify the PC in some way
  else if (instr->isBranch()) {
    DPRINT(DB_CF,"%s: isBranch\n", __func__);
    temp_result = doBranch(instr, branch_target, temp_predicate);
  }
  // Sim special ops
  else if (instr->isSimSpecial()) {
    doSimSpecial(instr);
  }
  // other instruction types
  else if (instr->isOther()) {
    DPRINT(DB_CF,"%s: isOther\n", __func__);

    switch (instr->type()) {
      case I_NOP:
        break;
      case I_HLT:
        printf("Halting Core %d @ pc %08x @ cycle %" PRIu64 "\n", id(), instr->pc(), rigel::CURR_CYCLE);
        halt(instr->tid()); // halt this thread
        break;
      default:
        instr->Dump();
        throw ExitSim("unhandled instruction type");
    }
  }
  else {
    if (instr->type()==I_EVENT) {
      DRIGEL( printf("ignore I_EVENT event instruction for now...NOP\n"); )
    } else {
    DPRINT(true,"%s: UNKNOWN!\n", __func__);
    instr->Dump();
    throw ExitSim("unhandled instruction type"); }
  }
 
  // select the next PC 
  if( temp_predicate ) { // instr->branchPredicate()) 
    instr->nextPC(branch_target);
  } else {
    instr->nextPC(next_pc);
  }

  // set the result output
  instr->setRegVal(DREG,temp_result);

  //instr->Dump();
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


regval32_t 
CoreFunctional::doMem(PipePacket* instr) {

  regval32_t sreg_t = instr->regval(SREG_T);
  regval32_t sreg_s = instr->regval(SREG_S);
  regval32_t temp_result;

  // most target addresses are of the following form
  // however, NOT all are (some are overridden)
  uint32_t target_addr = sreg_t.u32() + instr->simm16();

  // new packet based on instr type
  Packet* p = new Packet( rigel::instr_to_icmsg_full(instr->type()) );

  if (instr->isAtomic()) {
    DPRINT(DB_CF,"%s: isMem isAtomic\n", __func__);

    // split me into Local and Global atomic sections?
    switch (instr->type()) {

      // local atomics
      // these require intra-cluster synchronization 
      // (or for alternate implementations at least _some_ cross-core synch)
      case I_LDL: {
      uint32_t alt_target_addr = sreg_t.u32();
        // perform a regular LDW, but also set a link bit
        p->initCorePacket(alt_target_addr, 0, id(), GTID(instr->tid()));
        temp_result = doMemoryAccess(p);
        DPRINT(DB_CF_LDL,"load-linked: %08x from %08x\n",temp_result.u32(), alt_target_addr);
        break;
      }
      case I_STC: {
        uint32_t alt_target_addr = sreg_t.u32();
        // perform a store only if the link bit is set
        p->initCorePacket(alt_target_addr, sreg_s.u32(), id(), GTID(instr->tid()));
        temp_result = doMemoryAccess(p);
        DPRINT(DB_CF_STC,"store-conditional: %08x to %08x\n", temp_result.u32(), alt_target_addr);
        break;
      }

      // atomics that return a copy of the OLD value before atomic update
      case I_ATOMCAS: {
        uint32_t alt_target_addr = sreg_t.u32(); // target_addr is in a register for this one
        p->initCorePacket(alt_target_addr, instr->regval(DREG).u32(), id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32());
        temp_result = doMemoryAccess(p);
        DPRINT(DB_CF,"cas target_addr: %08x \n",alt_target_addr);
        break;
      }
      case I_ATOMXCHG: {
        p->initCorePacket(target_addr, instr->regval(DREG).u32(), id(), GTID(instr->tid()));
        temp_result = doMemoryAccess(p);
        break;
      }
      // arithmetic
      case I_ATOMINC: {
        p->initCorePacket(target_addr, 0, id(), GTID(instr->tid()));
        temp_result = doMemoryAccess(p);
        break;
      }
      case I_ATOMDEC: {
        p->initCorePacket(target_addr, 0, id(), GTID(instr->tid()));
        temp_result = doMemoryAccess(p);
        break;
      }
      case I_ATOMADDU: {
        p->initCorePacket(target_addr, 0, id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32());
        temp_result = doMemoryAccess(p);
        break;
      }
      // atomics that return the NEW value after atomic update
      // min,max
      case I_ATOMMIN: {
        uint32_t alt_target_addr = sreg_t.u32();
        p->initCorePacket(alt_target_addr, 0, id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32());
        temp_result = doMemoryAccess(p);
        break;
      }
      case I_ATOMMAX: {
          uint32_t alt_target_addr = sreg_t.u32();
          p->initCorePacket(alt_target_addr, 0, id(), GTID(instr->tid()));
          p->gAtomicOperand(sreg_s.u32());
          temp_result = doMemoryAccess(p);
        break;
      }
      // logical
      case I_ATOMAND: {
        uint32_t alt_target_addr = sreg_t.u32();
        p->initCorePacket(alt_target_addr, 0, id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32());
        temp_result = doMemoryAccess(p);
        break;
      }
      case I_ATOMOR: {
        uint32_t alt_target_addr = sreg_t.u32();
        p->initCorePacket(alt_target_addr, 0, id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32());
        temp_result = doMemoryAccess(p);
        break;
      }
      case I_ATOMXOR: {
        uint32_t alt_target_addr = sreg_t.u32();
        p->initCorePacket(alt_target_addr, 0, id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32());
        temp_result = doMemoryAccess(p);
        break;
      }
      default:
        instr->Dump();
        assert(0 && "unknown or unimplemented ATOMICOP");
        break;
    }
  // store value
  }
  else if (instr->isStore()) {
    DPRINT(DB_CF,"%s: isMem isStore\n", __func__);

    switch (instr->type()) {
      case I_STW:
      case I_GSTW:
      case I_BCAST_UPDATE: {
        uint32_t data        = instr->regval(DREG).u32();
        p->initCorePacket(target_addr,data,id(),GTID(instr->tid()));
        temp_result = doMemoryAccess(p);
        DPRINT(DB_CF_ST,"store: %08x to %08x\n",data, target_addr);
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
        p->initCorePacket(target_addr,0,id(),GTID(instr->tid()));
        //helper method? p->setMsgType(instr_t)
        temp_result = doMemoryAccess(p);
        DPRINT(DB_CF_LD,"load: %08x from %08x\n",temp_result.u32(), target_addr);
        break;
      }
      default:
        instr->Dump();
        assert(0 && "unknown LOAD");
        break;
    }
  } else if (instr->isPrefetch()) {
    DRIGEL( printf("ignoring PREFETCH instruction for now...NOP\n"); )
  } else {
    instr->Dump();
    assert(0 && "unknown memory operation!");
  }

  return temp_result;
}


regval32_t
CoreFunctional::doMemoryAccess(Packet* p) {

  port_status_t status;
  Packet* reply;
  regval32_t temp_result;

  status = to_ccache.sendMsg(p);

  if (status == ACK) {
    reply = from_ccache.read();
    DPRINT(DB_CF_LDL,"got reply! %d\n", reply->msgType());
    temp_result = reply->data();
  } else {
    throw ExitSim("unhandled path doMemoryAccess()");
  }

  if (reply->msgType()==IC_MSG_STC_REPLY_NACK) {
    DPRINT(DB_CF_STC,"[fail]");
  } else if (reply->msgType()==IC_MSG_STC_REPLY_ACK) {
    DPRINT(DB_CF_STC,"[success]");
  }
  // TODO: FIXME: re-enable checking for _all_ proper message request/reply pairs
  //else {
  //  throw ExitSim("Invalid response to STC request!\n");
  //}

  delete p;
  return temp_result;
}


regval32_t 
CoreFunctional::doALU(PipePacket* instr) {

  regval32_t temp_result;
  regval32_t sreg_t = instr->regval(SREG_T);
  regval32_t sreg_s = instr->regval(SREG_S);
  uint32_t zimm16  = instr->imm16();  // zext(imm16)
  uint32_t simm16  = instr->simm16(); // sext(imm16)
  uint32_t imm5    = instr->imm5(); 

  switch (instr->type()) {

    // arithmetic - register
    case I_ADD:  temp_result =   sreg_t.u32() + sreg_s.u32();  break;
    case I_SUB:  temp_result =   sreg_t.u32() - sreg_s.u32();  break;
    case I_MUL:  temp_result =   sreg_t.u32() * sreg_s.u32();  break;
    // arithmetic - immediate
    case I_SUBI: temp_result =   sreg_t.u32() - simm16;        break;
    case I_ADDI: temp_result =   sreg_t.u32() + simm16;        break;
    // arithmetic - immediate (unsigned, can't generate overflow exception -- if we cared about those)
    case I_SUBIU: temp_result =  sreg_t.u32() - simm16;        break;
    case I_ADDIU: temp_result =  sreg_t.u32() + simm16;        break;

    // logical - register
    case I_AND:  temp_result =   sreg_t.u32() & sreg_s.u32();  break;
    case I_OR:   temp_result =   sreg_t.u32() | sreg_s.u32();  break;
    case I_XOR:  temp_result =   sreg_t.u32() ^ sreg_s.u32();  break;
    case I_NOR:  temp_result = ~(sreg_t.u32() | sreg_s.u32()); break;
    // logical - immediates
    case I_ANDI: temp_result =   sreg_t.u32() & zimm16;        break;
    case I_ORI:  temp_result =   sreg_t.u32() | zimm16;        break;
    case I_XORI: temp_result =   sreg_t.u32() ^ zimm16;        break;

    // zero extensions
    case I_ZEXTB: temp_result =  sreg_t.u32() & 0x000000FF;    break;
    case I_ZEXTS: temp_result =  sreg_t.u32() & 0x0000FFFF;    break;
    // sign extensions
    case I_SEXTB: temp_result = int32_t(int8_t(sreg_t.u32()));  break;
    case I_SEXTS: temp_result = int32_t(int16_t(sreg_t.u32())); break;

    // other
    case I_MVUI: temp_result =   zimm16 << 16;                 break;

    // clz
    case I_CLZ: {
      uint32_t val = sreg_t.u32();
      int lz;
      for (lz = 0; lz <= 32; ) {
        if ( val & 0x80000000) { break; }
        val = val << 1;
        lz++;
      }    
      temp_result = lz; 
      break;
    }
    // sprf
    case I_MFSR: temp_result = sreg_t; break; // just a move
    case I_MTSR: temp_result = sreg_t; break; // just a move

    default:
      assert(0 && "unhandled ALU operation type!"); break;
  }
  return temp_result;
}

regval32_t 
CoreFunctional::doFPU(PipePacket* instr) {
  regval32_t temp_result;
  regval32_t sreg_t = instr->regval(SREG_T);
  regval32_t sreg_s = instr->regval(SREG_S);
  switch (instr->type()) {
    case I_FADD: temp_result = sreg_t.f32() + sreg_s.f32();      break;
    case I_FSUB: temp_result = sreg_t.f32() - sreg_s.f32();      break;
    case I_FMUL: temp_result = sreg_t.f32() * sreg_s.f32();      break;
    case I_FABS: temp_result = float(fabs(sreg_t.f32()));        break;
    case I_FRCP: temp_result = float(1.0 / sreg_t.f32());        break;
    case I_FRSQ: temp_result = float(1.0 / sqrtf(sreg_t.f32())); break;
    case I_FMADD:
      temp_result = instr->regval(DREG).f32() + (sreg_t.f32() * sreg_s.f32());
      break;
    // conversion (this could be a separate class)
    case I_I2F:  temp_result = float(sreg_t.i32()); break;
    case I_F2I:  temp_result = int(sreg_t.f32());   break;
    // comparison
    case I_CEQF:  temp_result = (sreg_t.f32() == sreg_s.f32()) ? 1 : 0;  break;
    case I_CLTF:  temp_result = (sreg_t.f32() <  sreg_s.f32()) ? 1 : 0;  break;
    case I_CLTEF: temp_result = (sreg_t.f32() <= sreg_s.f32()) ? 1 : 0;  break;
    // unimplemented
    case I_FMRS:  // UNIMPLEMENTED! (status?)
    case I_FMOV:  // UNIMPLEMENTED! (status?)
    case I_FMSUB: // UNIMPLEMENTED! (status?)
    default:
      instr->Dump();
      assert(0 && "unknown FPUOP");
      break;
  }
  return temp_result;
}

regval32_t 
CoreFunctional::doShift(PipePacket* instr) {
  regval32_t temp_result;
  regval32_t sreg_t = instr->regval(SREG_T);
  regval32_t sreg_s = instr->regval(SREG_S);
  uint32_t   imm5   = instr->imm5();
  switch (instr->type()) {
    // register based shifts
    case I_SLL:  temp_result = sreg_t.u32() << (sreg_s.u32() & 0x01FU); break;
    case I_SRL:  temp_result = sreg_t.u32() >> (sreg_s.u32() & 0x01FU); break;
    case I_SRA:  temp_result = sreg_t.i32() >> (sreg_s.u32() & 0x01FU); break;
    // immediate shifts
    case I_SLLI: temp_result = sreg_t.u32() << imm5;  break;
    case I_SRLI: temp_result = sreg_t.u32() >> imm5;  break;
    case I_SRAI: temp_result = sreg_t.i32() >> imm5;  break;
    default:
      assert(0 && "unknown SHIFT");
      break;
  }
  return temp_result;
}


regval32_t
CoreFunctional::doCompare(PipePacket* instr) {
  regval32_t sreg_t = instr->regval(SREG_T);
  regval32_t sreg_s = instr->regval(SREG_S);
  regval32_t temp_result;
  switch (instr->type()) {
    // signed compare
    case I_CEQ:  temp_result = (sreg_t.i32() == sreg_s.i32()) ? 1 : 0;  break;
    case I_CLT:  temp_result = (sreg_t.i32() <  sreg_s.i32()) ? 1 : 0;  break;
    case I_CLE:  temp_result = (sreg_t.i32() <= sreg_s.i32()) ? 1 : 0;  break;
    // unsigned compare
    case I_CLTU: temp_result = (sreg_t.u32() <  sreg_s.u32()) ? 1 : 0;  break;
    case I_CLEU: temp_result = (sreg_t.u32() <= sreg_s.u32()) ? 1 : 0;  break;
    default:
      instr->Dump();
      throw ExitSim("unhandled COMPARE");
      break;
  }
  return temp_result;
}

regval32_t
CoreFunctional::doBranch(PipePacket* instr, uint32_t& branch_target, bool& temp_predicate) {

  regval32_t temp_result;
  regval32_t sreg_t = instr->regval(SREG_T);
  regval32_t sreg_s = instr->regval(SREG_S);
  uint32_t incremented_pc = instr->pc() + 4;

  // some branches store a link register
  if (instr->isStoreLinkRegister()) {
    assert( instr->regnum(DREG) == rigel::regs::LINK_REG );
    temp_result = incremented_pc;
  }

  // compute branch predicate
  switch (instr->type()) {

    // unconditional, always true
    case I_JAL: 
    case I_JALR: 
    case I_JMP: 
    case I_JMPR: 
    case I_LJ:   
    case I_LJL:  temp_predicate = true;  break;

    // conditional branches
    // simple compare to zero branches
    case I_BE:   temp_predicate = (sreg_t.i32() == 0);  break;
    case I_BN:   temp_predicate = (sreg_t.i32() != 0);  break;
    case I_BLT:  temp_predicate = (sreg_t.i32() <  0);  break;
    case I_BGT:  temp_predicate = (sreg_t.i32() >  0);  break;
    case I_BLE:  temp_predicate = (sreg_t.i32() <= 0);  break;
    case I_BGE:  temp_predicate = (sreg_t.i32() >= 0);  break;
    // complex branches (cmp-branch with 2 register compare sources)
    case I_BEQ:  temp_predicate = (sreg_t.i32() == instr->regval(DREG).i32()); break;
    case I_BNE:  temp_predicate = (sreg_t.i32() != instr->regval(DREG).i32()); break;
    default:
      assert(0&&"unknown BRANCH"); break;
  }

  if (instr->isBranchDirect()) {
    switch (instr->type()) {

      // complex branches (cmp-branch with 2 compare sources)
      // branch_target = (pc+4) + (sext(imm16)<<2) 16-bit offsets
      case I_BEQ: 
      case I_BNE:
      // compare to zero and unconditional branches
      // branch_target = (pc+4) + (sext(imm16)<<2) 16-bit offsets
      case I_BE:  
      case I_BN:
      case I_BLT: 
      case I_BGT:
      case I_BLE: 
      case I_BGE:
      case I_JAL: 
      case I_JMP:
        branch_target = incremented_pc + (instr->simm16()<<2);
        break;
      // long jumps, 16-bit offsets
      // pc[31:28] | (imm26<<2)
      case I_LJ:
      case I_LJL:
        branch_target = (instr->pc() & 0xF0000000) | (instr->imm26()<<2);
        break;
      default:
        assert(0&&"unknown direct branch");
    }
  } else if (instr->isBranchIndirect()) {
    // PC is in register
    switch (instr->type()) {
      case I_JALR:
      case I_JMPR:
        branch_target = sreg_t.u32(); // the link register
        break;
      default:
        assert(0&&"unknown indirect branch");
    }
  }
  return temp_result;
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
    case I_ABORT:
      instr->Dump();
      char str[128];
      sprintf(str, "ABORT @pc 0x%08x, id:%d\n", instr->pc(), id() );
      throw ExitSim(str);
    default:
      instr->Dump();
      throw ExitSim("unhandled SimSpecial instruction type");
  }
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
