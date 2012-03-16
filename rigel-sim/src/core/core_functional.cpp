
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
        Packet* reply;
        reply = from_ccache.read();
        if (reply) { // handle reply
          DPRINT(true,"got reply! %d\n", reply->msgType());
          ts->instr->setRegVal(DREG, reply->data()); // FIXME: NOT ALL have results! 
          ts->instr->setCompleted(); // just for now...
          delete reply; // because we are done (for now)
        } 
      }
      
      // writeback
      if (ts->instr->isCompleted()) {

        writeback( ts->instr );
      
        ts->pc_ = ts->instr->nextPC();

        delete ts->instr; // don't forget to clear out that pesky dynamic object...
        ts->instr = 0; // no valid instruction
      } else {
        DPRINT(true, "memory operation returned incomplete!\n");
        throw ExitSim("not allowed to have incomplete instructions now");
      }

    }

  } else {
    // no thread available this cycle, stalled
  }

  if (DB_CF) { ts->rf.Dump(); }

  return halted();

}

int
CoreFunctional::thread_select() {
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

  if ( instr->isDREGDest() ) {
    
    assert( instr->regval(DREG).valid() );
    if (!instr->regval(DREG).valid()) { 
      instr->Dump();
      throw ExitSim("invalid value on writeback"); }

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
    doALU(instr);
  }
  // shift
  else if (instr->isShift()) {
    DPRINT(DB_CF,"%s: isALU\n", __func__);
    doShift(instr);
  }
  // FPU
  else if (instr->isFPU()) {
    DPRINT(DB_CF,"%s: isFPU\n", __func__);
    doFPU(instr);
  }
  // Memory (address computation here)
  else if (instr->isMem()) {
    doMemAddress(instr);
  }
  // Comparisons
  else if (instr->isCompare()) {
    DPRINT(DB_CF,"%s: isCompare\n", __func__);
    doCompare(instr);
  }
  // instructions that modify the PC in some way
  else if (instr->isBranch()) {
    DPRINT(DB_CF,"%s: isBranch\n", __func__);
    doBranch(instr);
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
CoreFunctional::doMemAddress(PipePacket* instr) {

  regval32_t result;
  rword32_t sreg_t = instr->regval(SREG_T);
  rword32_t sreg_s = instr->regval(SREG_S);

  // most target addresses are of the following form
  // however, NOT all are (some are overridden)
  uint32_t target_addr = sreg_t.u32 + instr->simm16();

  if (instr->isAtomic()) {
    DPRINT(DB_CF,"%s: isMem isAtomic\n", __func__);

    // split me into Local and Global atomic sections?
    switch (instr->type()) {

      case I_LDL: 
      case I_STC: 
      case I_ATOMCAS: {
        uint32_t alt_target_addr = sreg_t.u32; // target_addr is in a register for this one
        instr->target_addr(alt_target_addr);
        break;
      }
      case I_ATOMXCHG:
      case I_ATOMINC:
      case I_ATOMDEC: 
      case I_ATOMADDU: 
        instr->target_addr(target_addr); 
        break;

      case I_ATOMMIN: 
      case I_ATOMMAX: 
      case I_ATOMAND: 
      case I_ATOMOR: 
      case I_ATOMXOR: {
        uint32_t alt_target_addr = sreg_t.u32;
        instr->target_addr(alt_target_addr);
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
        instr->target_addr(target_addr);
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
        instr->target_addr(target_addr);
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
        DRIGEL( printf("ignoring CACHECONTROL instruction for now...NOP\n"); )
        break;
      default:
        throw ExitSim("unhandled CacheControl operation?");
    }
  } else if (instr->isPrefetch()) {
    DRIGEL( printf("ignoring PREFETCH instruction for now...NOP\n"); )
  } else {
    instr->Dump();
    throw ExitSim("unknown memory operation!");
  }
  // above, we have set the instr->target_addr
}


void 
CoreFunctional::doMem(PipePacket* instr) {

  regval32_t result;
  rword32_t sreg_t = instr->regval(SREG_T);
  rword32_t sreg_s = instr->regval(SREG_S);

  //doMemAddress(instr); already done

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
      case I_LDL: {
        // perform a regular LDW, but also set a link bit
        p->initCorePacket(addr, 0, id(), GTID(instr->tid()));
        doMemoryAccess(instr, p);
        break;
      }
      case I_STC: {
        // perform a store only if the link bit is set
        p->initCorePacket(addr, sreg_s.u32, id(), GTID(instr->tid()));
        doMemoryAccess(instr, p);
        break;
      }
      // atomics that return a copy of the OLD value before atomic update
      case I_ATOMCAS: {
        p->initCorePacket(addr, instr->regval(DREG).u32(), id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32);
        doMemoryAccess(instr, p);
        break;
      }
      case I_ATOMXCHG: {
        p->initCorePacket(addr, instr->regval(DREG).u32(), id(), GTID(instr->tid()));
        doMemoryAccess(instr, p);
        break;
      }
      // arithmetic
      case I_ATOMINC: 
      case I_ATOMDEC: {
        p->initCorePacket(addr, 0, id(), GTID(instr->tid()));
        doMemoryAccess(instr, p);
        break;
      }
      case I_ATOMADDU:
      // atomics that return the NEW value after atomic update
      // min,max
      case I_ATOMMIN: 
      case I_ATOMMAX: 
      // logical
      case I_ATOMAND:
      case I_ATOMOR:
      case I_ATOMXOR: {
        p->initCorePacket(addr, 0, id(), GTID(instr->tid()));
        p->gAtomicOperand(sreg_s.u32);
        doMemoryAccess(instr, p);
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
        p->initCorePacket(addr ,data, id(), GTID(instr->tid()));
        doMemoryAccess(instr, p); // no result for stores
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
        doMemoryAccess(instr, p);
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
        DRIGEL( printf("ignoring CACHECONTROL instruction for now...NOP\n"); )
        break;
      default:
        throw ExitSim("unhandled CacheControl operation?");
    }
  } else if (instr->isPrefetch()) {
    DRIGEL( printf("ignoring PREFETCH instruction for now...NOP\n"); )
  } else {
    instr->Dump();
    throw ExitSim("unknown memory operation!");
  }

  if (p->isCompleted()) {
    result = p->data();
    delete p; // because we are done (for now)
    instr->setRegVal(DREG,result); // FIXME: NOT ALL have results! 
    instr->setCompleted(); // just for now...
  } else {
    throw ExitSim("unimplemented for incompleted operation\n");
  }
}

// doMemoryAccess
// actually poke the ports
void
CoreFunctional::doMemoryAccess(PipePacket* instr, Packet* p) {

  port_status_t status;
  Packet* reply;

  status = to_ccache.sendMsg(p);

  if (status == ACK) { // port accepted message
    reply = from_ccache.read();
    if (reply) { // handle reply
      DPRINT(DB_CF_MEM,"got reply! %d\n", reply->msgType());
      // FIXME: some operations DO NOT HAVE a result!
      assert( reply == p ); // for now
    } else { 
      // NULL message means we have nothing to process
    }
  } else {
    reply = NULL;
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

}


void 
CoreFunctional::doALU(PipePacket* instr) {

  regval32_t result;
  rword32_t sreg_t = instr->regval(SREG_T);
  rword32_t sreg_s = instr->regval(SREG_S);
  uint32_t zimm16  = instr->imm16();  // zext(imm16)
  uint32_t simm16  = instr->simm16(); // sext(imm16)

  switch (instr->type()) {

    // arithmetic - register
    case I_ADD:  result  =   sreg_t.u32 + sreg_s.u32;  break;
    case I_SUB:  result  =   sreg_t.u32 - sreg_s.u32;  break;
    case I_MUL:  result  =   sreg_t.u32 * sreg_s.u32;  break;
    // arithmetic - immediate
    case I_SUBI: result  =   sreg_t.u32 - simm16;        break;
    case I_ADDI: result  =   sreg_t.u32 + simm16;        break;
    // arithmetic - immediate (unsigned, can't generate overflow exception -- if we cared about those)
    case I_SUBIU: result  =  sreg_t.u32 - simm16;        break;
    case I_ADDIU: result  =  sreg_t.u32 + simm16;        break;

    // logical - register
    case I_AND:  result  =   sreg_t.u32 & sreg_s.u32;  break;
    case I_OR:   result  =   sreg_t.u32 | sreg_s.u32;  break;
    case I_XOR:  result  =   sreg_t.u32 ^ sreg_s.u32;  break;
    case I_NOR:  result  = ~(sreg_t.u32 | sreg_s.u32); break;
    // logical - immediates
    case I_ANDI: result  =   sreg_t.u32 & zimm16;        break;
    case I_ORI:  result  =   sreg_t.u32 | zimm16;        break;
    case I_XORI: result  =   sreg_t.u32 ^ zimm16;        break;

    // zero extensions
    case I_ZEXTB: result  =  sreg_t.u32 & 0x000000FF;    break;
    case I_ZEXTS: result  =  sreg_t.u32 & 0x0000FFFF;    break;
    // sign extensions
    case I_SEXTB: result  = int32_t(int8_t(sreg_t.u32));  break;
    case I_SEXTS: result  = int32_t(int16_t(sreg_t.u32)); break;

    // other
    case I_MVUI: result  =   zimm16 << 16;                 break;

    // clz
    case I_CLZ: {
      uint32_t val = sreg_t.u32;
      int lz;
      for (lz = 0; lz <= 32; ) {
        if ( val & 0x80000000) { break; }
        val = val << 1;
        lz++;
      }    
      result  = lz; 
      break;
    }
    // sprf
    case I_MFSR: result  = sreg_t; break; // just a move
    case I_MTSR: result  = sreg_t; break; // just a move

    default:
      assert(0 && "unhandled ALU operation type!"); 
      break;
  }
  instr->setRegVal(DREG,result);
  instr->setCompleted();
}

void 
CoreFunctional::doFPU(PipePacket* instr) {
  regval32_t result;
  rword32_t sreg_t = instr->regval(SREG_T);
  rword32_t sreg_s = instr->regval(SREG_S);
  switch (instr->type()) {
    case I_FADD: result = sreg_t.f32 + sreg_s.f32;      break;
    case I_FSUB: result = sreg_t.f32 - sreg_s.f32;      break;
    case I_FMUL: result = sreg_t.f32 * sreg_s.f32;      break;
    case I_FABS: result = float(fabs(sreg_t.f32));        break;
    case I_FRCP: result = float(1.0 / sreg_t.f32);        break;
    case I_FRSQ: result = float(1.0 / sqrtf(sreg_t.f32)); break;
    case I_FMADD:
      result = instr->regval(DREG).f32() + (sreg_t.f32 * sreg_s.f32);
      break;
    // conversion (this could be a separate class)
    case I_I2F:  result = float(sreg_t.i32); break;
    case I_F2I:  result = int(sreg_t.f32);   break;
    // comparison
    case I_CEQF:  result = (sreg_t.f32 == sreg_s.f32) ? 1 : 0;  break;
    case I_CLTF:  result = (sreg_t.f32 <  sreg_s.f32) ? 1 : 0;  break;
    case I_CLTEF: result = (sreg_t.f32 <= sreg_s.f32) ? 1 : 0;  break;
    // unimplemented
    case I_FMRS:  // UNIMPLEMENTED! (status?)
    case I_FMOV:  // UNIMPLEMENTED! (status?)
    case I_FMSUB: // UNIMPLEMENTED! (status?)
    default:
      instr->Dump();
      assert(0 && "unknown FPUOP");
      result = 0;
      break;
  }
  instr->setRegVal(DREG,result);
  instr->setCompleted();
}

void
CoreFunctional::doShift(PipePacket* instr) {
  regval32_t result;
  rword32_t sreg_t = instr->regval(SREG_T);
  rword32_t sreg_s = instr->regval(SREG_S);
  uint32_t   imm5   = instr->imm5();
  switch (instr->type()) {
    // register based shifts
    case I_SLL:  result = sreg_t.u32 << (sreg_s.u32 & 0x01FU); break;
    case I_SRL:  result = sreg_t.u32 >> (sreg_s.u32 & 0x01FU); break;
    case I_SRA:  result = sreg_t.i32 >> (sreg_s.u32 & 0x01FU); break;
    // immediate shifts
    case I_SLLI: result = sreg_t.u32 << imm5;  break;
    case I_SRLI: result = sreg_t.u32 >> imm5;  break;
    case I_SRAI: result = sreg_t.i32 >> imm5;  break;
    default:
      assert(0 && "unknown SHIFT");
      break;
  }
  instr->setRegVal(DREG,result);
  instr->setCompleted();
}


void 
CoreFunctional::doCompare(PipePacket* instr) {
  regval32_t result;
  rword32_t sreg_t = instr->regval(SREG_T);
  rword32_t sreg_s = instr->regval(SREG_S);
  switch (instr->type()) {
    // signed compare
    case I_CEQ:  result = (sreg_t.i32 == sreg_s.i32) ? 1 : 0;  break;
    case I_CLT:  result = (sreg_t.i32 <  sreg_s.i32) ? 1 : 0;  break;
    case I_CLE:  result = (sreg_t.i32 <= sreg_s.i32) ? 1 : 0;  break;
    // unsigned compare
    case I_CLTU: result = (sreg_t.u32 <  sreg_s.u32) ? 1 : 0;  break;
    case I_CLEU: result = (sreg_t.u32 <= sreg_s.u32) ? 1 : 0;  break;
    default:
      instr->Dump();
      throw ExitSim("unhandled COMPARE");
      break;
  }
  instr->setRegVal(DREG,result);
  instr->setCompleted();
}

void 
CoreFunctional::doBranch(PipePacket* instr) {

  regval32_t result; // empty and invalid by default
  uint32_t incremented_pc = instr->pc() + 4;

  // some branches store a link register (otherwise, no register writes)
  if (instr->isStoreLinkRegister()) {
    assert( instr->regnum(DREG) == rigel::regs::LINK_REG );
    result = incremented_pc;
  }

  doBranchPredicate(instr);

  doBranchTarget(instr);

  // this should be ignored (invalid) unless we have a link register
  instr->setRegVal(DREG,result);
  instr->setCompleted();
}

void 
CoreFunctional::doBranchPredicate(PipePacket* instr) {

  bool predicate = false;
  rword32_t  sreg_t = instr->regval(SREG_T);

  // compute branch predicate
  switch (instr->type()) {

    // unconditional, always true
    case I_JAL: 
    case I_JALR: 
    case I_JMP: 
    case I_JMPR: 
    case I_LJ:   
    case I_LJL:  predicate = true;  break;

    // conditional branches
    // simple compare to zero branches
    case I_BE:   predicate = (sreg_t.i32 == 0);  break;
    case I_BN:   predicate = (sreg_t.i32 != 0);  break;
    case I_BLT:  predicate = (sreg_t.i32 <  0);  break;
    case I_BGT:  predicate = (sreg_t.i32 >  0);  break;
    case I_BLE:  predicate = (sreg_t.i32 <= 0);  break;
    case I_BGE:  predicate = (sreg_t.i32 >= 0);  break;
    // complex branches (cmp-branch with 2 register compare sources)
    case I_BEQ:  predicate = (sreg_t.i32 == instr->regval(DREG).i32()); break;
    case I_BNE:  predicate = (sreg_t.i32 != instr->regval(DREG).i32()); break;
    default:
      assert(0&&"unknown BRANCH"); break;
  }

  instr->branch_predicate(predicate);
}

void 
CoreFunctional::doBranchTarget(PipePacket* instr) {

  uint32_t branch_target = 0;

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
      case I_JMP: {
        uint32_t incremented_pc = instr->pc() + 4;
        branch_target = incremented_pc + (instr->simm16()<<2);
        break;
      }
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
        branch_target = instr->regval(SREG_T).u32(); // the link register
        break;
      default:
        assert(0&&"unknown indirect branch");
    }
  }
  instr->target_addr(branch_target);
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
