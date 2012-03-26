#include "isa/rigel_isa.h"

void 
RigelISA::execALU(PipePacket* instr) {

  regval32_t result; // invalid by default
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
RigelISA::execShift(PipePacket* instr) {
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
RigelISA::execCompare(PipePacket* instr) {
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
RigelISA::execBranch(PipePacket* instr) {

  regval32_t result; // empty and invalid by default
  uint32_t incremented_pc = instr->pc() + 4;

  // some branches store a link register (otherwise, no register writes)
  if (instr->isStoreLinkRegister()) {
    assert( instr->regnum(DREG) == rigel::regs::LINK_REG );
    result = incremented_pc;
  }

  execBranchPredicate(instr);

  execBranchTarget(instr);

  // this should be ignored (invalid) unless we have a link register
  instr->setRegVal(DREG,result);
  instr->setCompleted();
}

void 
RigelISA::execBranchTarget(PipePacket* instr) {

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
RigelISA::execBranchPredicate(PipePacket* instr) {

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
RigelISA::execFPU(PipePacket* instr) {
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
RigelISA::execAddressGen(PipePacket* instr) {

  regval32_t result;
  rword32_t sreg_t = instr->regval(SREG_T);
  rword32_t sreg_s = instr->regval(SREG_S);

  // most target addresses are of the following form
  // however, NOT all are (some are overridden)
  uint32_t target_addr = sreg_t.u32 + instr->simm16();

  if (instr->isAtomic()) {
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
        DRIGEL(false, printf("ignoring CACHECONTROL instruction for now...NOP\n"); )
        break;
      default:
        throw ExitSim("unhandled CacheControl operation?");
    }
  } else if (instr->isPrefetch()) {
    DRIGEL(false, printf("ignoring PREFETCH instruction for now...NOP\n"); )
  } else {
    instr->Dump();
    throw ExitSim("unknown memory operation!");
  }
  // above, we have set the instr->target_addr
}

/// execGlobalAtomic
/// not ideal, but assume a packetized global atomic request
/// this could be made more generic and not poke backing_store directly
uint32_t 
RigelISA::execGlobalAtomic(PacketPtr p, rigel::GlobalBackingStoreType *mem_backing_store) {

  uint32_t addr = p->addr();
  uint32_t return_result; // the value we return back to the requester
  uint32_t newval; // the atomic result stored to memory
  uint32_t swapval = p->data();
  uint32_t operand = p->gAtomicOperand();

  bool write_result = true; // all but CAS will write their result by default

  uint32_t oldval = mem_backing_store->read_data_word(addr);

  switch (p->msgType()) {
    // atomics that return a copy of the OLD value before atomic update
    case IC_MSG_ATOMCAS_REQ: {
      newval = swapval;
      if (oldval == operand) { 
        write_result = true; // written below
      } else {
        write_result = false; // skip the write below
      }
      return_result = oldval;
      break;
    }
    case IC_MSG_ATOMXCHG_REQ: {
      newval = swapval;
      return_result = oldval;
      break;
    }
    case IC_MSG_ATOMADDU_REQ: {
      newval = oldval + operand;
      return_result = oldval;
      break;
    }
    // atomics that return the NEW value after atomic update
    // arithmetic
    case IC_MSG_ATOMINC_REQ: {
      newval = oldval + 1;
      return_result = newval;
      break;
    }
    case IC_MSG_ATOMDEC_REQ: {
      newval = oldval - 1;
      return_result = newval;
      break;
    }
    // min,max
    case IC_MSG_ATOMMIN_REQ: {
      newval = std::min(oldval, operand);
      return_result = newval;
      break;
    }
    case IC_MSG_ATOMMAX_REQ: {
      newval = std::max(oldval, operand);
      return_result = newval;
      break;
    }
    // logical
    case IC_MSG_ATOMAND_REQ: {
      newval = oldval & operand;
      return_result = newval;
      break;
    }
    case IC_MSG_ATOMOR_REQ: {
      newval = oldval | operand;
      return_result = newval;
      break;
    }
    case IC_MSG_ATOMXOR_REQ: {
      newval = oldval ^ operand;
      return_result = newval;
      break;
    }
    default:
      p->Dump();
      throw ExitSim("unknown or unimplemented Global ATOMICOP");
      break;
  }

  // write the atomic result back to memory immediately (since it is atomic)
  if (write_result) {
    mem_backing_store->write_word(addr, newval);
  }

  return return_result;
}
