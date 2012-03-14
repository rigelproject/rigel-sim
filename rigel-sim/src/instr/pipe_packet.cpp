#include "instr/pipe_packet.h"

std::map<uint32_t,StaticDecodeInfo> PipePacket::decodedInstructions;

const char isa_reg_names[NUM_ISA_OPERAND_REGS][8] = {
  "SREG_S",
  "SREG_T",
  "DREG",
};


/// FIXME: AUTO-GENERATE ALL of THESE HELPERS (or, at least statically store flag result)
///
////////////////////////////////////////////////////////////////////////////////
// helper function
// every instruction should be covered by one of these

bool
StaticDecodeInfo::isCacheControl() const {
  switch (type_) {
    case I_LINE_WB:
    case I_LINE_INV:
    case I_LINE_FLUSH:
      return true; break;
    default:
      return false; break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// instr_is_memaccess()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isMem() const {
  switch (type_) {
    case I_GLDW: 
    case I_GSTW:
    case I_BCAST_UPDATE:
    case I_LDW: 
    case I_STW:
    case I_STC:
    case I_LDL: 
    case I_ATOMINC:
    case I_ATOMDEC: 
    case I_ATOMCAS:
    case I_ATOMXCHG: 
    case I_ATOMADDU:
    case I_ATOMMAX:
    case I_ATOMMIN:
    case I_ATOMOR:
    case I_ATOMAND:
    case I_ATOMXOR:
    case I_VSTW: 
    case I_VLDW:
    case I_PREF_NGA:
    case I_PREF_L:
    case I_LINE_WB:
    case I_LINE_INV:
    case I_LINE_FLUSH:
      { return true; }
    default:
      { return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_global()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isGlobal() const {
  switch (type_) {
    case I_GLDW: 
    case I_GSTW:
    case I_BCAST_UPDATE:
    case I_ATOMINC:
    case I_ATOMDEC: 
    case I_ATOMCAS:
    case I_ATOMXCHG: 
    case I_ATOMADDU:
    case I_ATOMMAX:
    case I_ATOMMIN:
    case I_ATOMOR:
    case I_ATOMAND:
    case I_ATOMXOR:
      { return true; }
    default:
      { return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_atomic()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isLocalAtomic() const {
  switch (type_) {
    case I_STC:
    case I_LDL: 
      { return true; }
    default:
      { return false; }
  }
}

bool
StaticDecodeInfo::isStore() const {
  switch (type_) {
    case I_STW:
    case I_GSTW:
    case I_BCAST_UPDATE:
      return true;
    default:
      return false;
  }
}
bool
StaticDecodeInfo::isLoad() const {
  switch (type_) {
    case I_LDW:
    case I_GLDW:
      return true;
    default:
      return false;
  }
}
bool
StaticDecodeInfo::isStoreLinkRegister() const {
  switch (type_) {
    case I_JAL:
    case I_JALR:
    case I_LJL:
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// instr_is_global_atomic()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isGlobalAtomic() const {
  switch (type_) {
    case I_ATOMINC:
    case I_ATOMDEC: 
    case I_ATOMCAS:
    case I_ATOMXCHG: 
    case I_ATOMADDU:
    case I_ATOMMAX:
    case I_ATOMMIN:
    case I_ATOMOR:
    case I_ATOMAND:
    case I_ATOMXOR:
      { return true; }
    default:
      { return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_atomic()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isAtomic() const {
  return    StaticDecodeInfo::isGlobalAtomic() 
         || StaticDecodeInfo::isLocalAtomic();
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_memaccess()
////////////////////////////////////////////////////////////////////////////////
// Memory operations that are expected to hit in L1D and/or L2D.
bool 
StaticDecodeInfo::isLocalMem() const {
  switch (type_) {
    case I_LDW: 
    case I_STW:
    case I_STC: 
    case I_LDL: 
      { return true; }
    default:
      { return false; }
  }
}

////////////////////////////////////////////////////////////////////////////////
// instr_is_prefetch()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isPrefetch() const {
  switch (type_) {
    case I_PREF_NGA: 
    case I_PREF_L: 
      { return true; }
    default:
      { return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_load()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isLocalLoad() const {
  switch (type_) {
    case I_LDW: 
    case I_LDL: 
      { return true; }
    default:
      { return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_store()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isLocalStore() const {
  switch (type_) {
    case I_STW:
    case I_STC: 
      { return true; }
    default:
      { return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_branch()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isBranch() const {
  switch (type_) {
    case I_BEQ: 
    case I_BNE:
    case I_BE:  
    case I_BN:
    case I_BLT: 
    case I_BGT:
    case I_BLE: 
    case I_BGE:
    case I_JALR: 
    case I_JMPR:
    case I_JAL: 
    case I_JMP:
    case I_LJ:
    case I_LJL:
    { return true; }
    default: {return false; }
  }
}
bool
StaticDecodeInfo::isBranchDirect() const {
  switch (type_) {
    case I_BEQ: 
    case I_BNE:
    case I_BE:  
    case I_BN:
    case I_BLT: 
    case I_BGT:
    case I_BLE: 
    case I_BGE:
    case I_JAL: 
    case I_JMP:
    case I_LJ:
    case I_LJL:
    { return true; }
    default: {return false; }
  }
}
bool
StaticDecodeInfo::isBranchIndirect() const {
  switch (type_) {
    case I_JALR: 
    case I_JMPR:
    { return true; }
    default: {return false; }
  }
}

// FLOAT compares???
bool
StaticDecodeInfo::isCompare() const {
  switch (type_) {
    case I_CEQ:
    case I_CLT:
    case I_CLE:
    case I_CLEU:
    case I_CLTU:
      { return true; }
      default: {return false; }
  }
}

////////////////////////////////////////////////////////////////////////////////
// instr_is_local_aluop()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isALU() const {
  switch (type_) {
    // arithmetic
    case I_ADD:
    case I_SUB:
    case I_ADDI:
    case I_ADDIU:
    case I_SUBI:
    case I_SUBIU:
    case I_MUL:
    // logic
    case I_OR:
    case I_ORI:
    case I_AND:
    case I_ANDI:
    case I_XOR:
    case I_XORI:
    case I_NOR:
    // extends
    case I_SEXTB:
    case I_ZEXTB:
    case I_SEXTS:
    case I_ZEXTS:
    // shift -- classified separately
    // case I_SLL:
    // case I_SRL:
    // case I_SRA:
    // case I_SLLI:
    // case I_SRLI:
    // case I_SRAI:
    // other
    case I_CLZ:
    // moves
    case I_MVUI:
    // sprf
    case I_MFSR:
    case I_MTSR:
    { return true; }
    default: {return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_fpuop()
////////////////////////////////////////////////////////////////////////////////
bool 
StaticDecodeInfo::isFPU() const {
  switch (type_) {
    case I_FADD:
    case I_FSUB:
    case I_FMUL:
    case I_FABS:
    case I_FRCP:
    case I_FRSQ:
    case I_I2F:
    case I_F2I:
    case I_CEQF:
    case I_CLTF:
    case I_CLTEF:
    case I_FMADD:
    case I_FMSUB:
    { return true; }
    default: {return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_other()
////////////////////////////////////////////////////////////////////////////////
bool
StaticDecodeInfo::isOther() const {
  switch (type_) {
    case I_PRE_DECODE: 
    case I_NULL: 
    case I_REP_HLT: 
    case I_DONE: 
    case I_BRANCH_FALL_THROUGH: 
    case I_CMNE_NOP:
    case I_CMEQ_NOP: 
    case I_UNDEF: 
    case I_MB:
    { return true; }
    default: {return false; }
  }
}

bool
StaticDecodeInfo::isNOP() const {
  switch (type_) {
    case I_NOP:
    { return true; }
    default: {return false; }
  }
}
bool
StaticDecodeInfo::isSPRFSrc() const {
  switch (type_) {
    case I_MFSR:
    { return true; }
    default: {return false; }
  }
}

bool
StaticDecodeInfo::isDREGSrc() const {
  switch (type_) {
    case I_BEQ:
    case I_BNE:
    case I_STW:
    case I_GSTW:
    case I_BCAST_UPDATE:
    case I_ATOMXCHG:
    case I_ATOMCAS:
    case I_FMADD: // NOTE: HACK: this encoding is unstable, FIXME when FMADD finalized
    // these should be re-encoded to use the proper sreg_t and not read DREG...
    case I_PRINTREG:
    case I_SYSCALL:
    case I_START_TIMER:
    case I_STOP_TIMER:
      return true; 
    default: {return false; }
  }
}
bool
StaticDecodeInfo::isSPRFDest() const {
  switch (type_) {
    case I_MTSR:
    { return true; }
    default: {return false; }
  }
}
bool
StaticDecodeInfo::isSimSpecial() const {
  switch (type_) {
    case I_PRINTREG: // RigelPrint
    case I_SYSCALL:  // System Call Emulation
    case I_EVENT:
    case I_NOP: 
    case I_ABORT:
    case I_HLT: 
    { return true; }
    default: {return false; }
  }
}

bool 
StaticDecodeInfo::isShift() const {
  switch (type_) {
    case I_SLL: 
    case I_SLLI: 
    case I_SRL: 
    case I_SRLI: 
    case I_SRA: 
    case I_SRAI: 
      { return true; }
    default:
      { return false; }
  }
}
