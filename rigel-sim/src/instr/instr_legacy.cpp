////////////////////////////////////////////////////////////////////////////////
// instr.cpp
////////////////////////////////////////////////////////////////////////////////
//
// implementations for the InstrLegacy class
// InstrLegacy is the pipeline packet structure that gets passed down each stage
// of the pipeline. It stores all the information related to the execution of a
// particular instruction (opcodes, registers read, results, etc.)
//
////////////////////////////////////////////////////////////////////////////////
#include "config.h"
#include "instr.h"
#include <cstdio>
#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for fprintf, stderr, FILE
#include <string.h>                     // for memset
#include <iostream>                     // for basic_ostream::operator<<, etc
#include <string>                       // for char_traits
#include <cstdarg>                      // for va_start, va_end
#include <cstddef>                      // for ptrdiff_t

#include "sim.h"            // for CURR_CYCLE, INVALID_REG
#include "util/util.h"           // for ExitSim
#ifdef ENABLE_GPLV3
#include <bfd.h>
#include <dis-asm.h>
#endif

// in instr_base.h, put back later
// struct string_descriptor {
//   char *buf;
//   char *ptr;
//   char *end;
// 
//   string_descriptor(int bufsize) {
//     buf = new char[bufsize];
//     ptr = buf;
//     end = buf+bufsize-1;
//   }
//   ~string_descriptor() {
//     delete[] buf;
//   }
//   size_t length() const {
//     return (size_t)(ptr - buf);
//   }
// };
// 
// int string_descriptor_snprintf(string_descriptor *str, char *format, ...)
// {
//   va_list args;
//   va_start(args, format);
//   int ret = vsnprintf(str->ptr, (str->end - str->ptr), format, args);
//   str->ptr += ret;
//   va_end(args);
//   return ret;
// }

////////////////////////////////////////////////////////////////////////////////
// InstrLegacy Constructor
////////////////////////////////////////////////////////////////////////////////
// more detailed constructor
// use this usually
////////////////////////////////////////////////////////////////////////////////
InstrLegacy::InstrLegacy(uint32_t _pc, int _tid, uint32_t _pred_pc, uint32_t _raw_instr, instr_t i_type) : 
   InstrBase(_pc,_raw_instr), // base constructor 
   // initialization list
   spec_list_idx(-1), 
   bad_stc(false),
   is_completed_BCAST_UPDATE(false),
   thread_id(_tid),
   pc(_pc), 
   next_pc(_pc+4), 
   pred_next_pc(_pred_pc), 
   raw_instr(_raw_instr),
   instr_type(i_type),
   instr_type_decode(i_type),
   result_ea(0xDEADBEEF), //result_RF(0xDEADBEEF),
   /* result_RF_rnum(0xDEADBEEF) */ 
   pipe(-1), 
   exCycles(0), 
   correct_path(true), 
   stall(false),
   mispredict(false), 
   sb_get(false), 
   done_decode(false), 
   done_regaccess(false),
   done_execute(false), 
   bypass_s0(false), 
   bypass_s1(false), 
   bypass_s2(false), 
   bypass_s3(false),
   bypass_sacc(false),
   _doneMemAccess(false), 
   _doneCCAccess(false), 
   _regread_done(false),
   _has_lock(false), 
   lock_reg(rigel::INVALID_REG),
   spawn_cycle(rigel::CURR_CYCLE),
   // Should be reset at actual fetch type.
   fetch_cycle(rigel::CURR_CYCLE),
   first_memaccess_cycle(0),
   last_memaccess_cyle(0),
   num_read_ports(0),
   bypass_s4(false),
   bypass_s5(false),
   bypass_s6(false),
   bypass_s7(false),
   is_vec_op(false)
{
  InstrNumber = GLOBAL_INSTR_NUMBER++;

//    printf("CREATING INSTRUCTION: %x\n", (unsigned int)this);
}

////////////////////////////////////////////////////////////////////////////////
// InstrLegacy Constructor (default)
////////////////////////////////////////////////////////////////////////////////
// default constructor, no input parameters
////////////////////////////////////////////////////////////////////////////////
InstrLegacy::InstrLegacy() :
  InstrBase(0,0), // base constructor (invalid pc,raw_bits)
  // massive initialization list:
  spec_list_idx(-1),
  bad_stc(false),
  pc(0xDEADBEEF), 
  instr_type(I_NULL),
  result_ea(0xDEADBEEF), 
  // result_RF(0xDEADBEEF),
  /*result_RF_rnum(0xDEADBEEF)*/ 
  pipe(-1), 
  exCycles(0), 
  correct_path(true),
  stall(false), 
  mispredict(false), 
  sb_get(false), 
  done_decode(false),
  done_regaccess(false), 
  done_execute(false),
  bypass_s0(false), 
  bypass_s1(false), 
  bypass_s2(false), 
  bypass_s3(false),
  bypass_sacc(false),
  _doneMemAccess(false), 
  _doneCCAccess(false),  
  _regread_done(false),
  _has_lock(false), 
  lock_reg(rigel::INVALID_REG),
  spawn_cycle(rigel::CURR_CYCLE),
  num_read_ports(0)
{
  InstrNumber = GLOBAL_INSTR_NUMBER++;
//    printf("CREATING INSTRUCTION: %x\n", (unsigned int)this);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor InstrLegacy (alternate) 
////////////////////////////////////////////////////////////////////////////////
// another InstrLegacy constructor...
////////////////////////////////////////////////////////////////////////////////
InstrLegacy::InstrLegacy(uint32_t _pc, int _tid) : 
  InstrBase(_pc,0), // base constructor -- invalid raw_bits...
  // massive initialization list
  spec_list_idx(-1),
  bad_stc(false),
  thread_id(_tid),
  pc(_pc), 
  instr_type(I_NULL),
  result_ea(0xDEADBEEF), // result_RF(0xDEADBEEF),
  /*result_RF_rnum(0xDEADBEEF)*/ 
  pipe(-1), 
  exCycles(0), 
  correct_path(true),
  stall(false), 
  mispredict(false), 
  sb_get(false), 
  done_decode(false),
  done_regaccess(false), 
  done_execute(false),
  bypass_s0(false), 
  bypass_s1(false), 
  bypass_s2(false), 
  bypass_s3(false),
  _doneMemAccess(false), 
  _doneCCAccess(false), 
  _regread_done(false),
  _has_lock(false), 
  lock_reg(rigel::INVALID_REG),
  spawn_cycle(rigel::CURR_CYCLE),
  // Should be reset at actual fetch type.
  fetch_cycle(rigel::CURR_CYCLE),
  num_read_ports(0)
{ 
  InstrNumber = GLOBAL_INSTR_NUMBER++;
//    printf("CREATING INSTRUCTION: %x\n", (unsigned int)this);
}

uint64_t InstrLegacy::GLOBAL_INSTR_NUMBER = 1;
uint64_t *InstrLegacy::LAST_INSTR_NUMBER;

////////////////////////////////////////////////////////////////////////////////
// InstrLegacy::update_type
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void 
InstrLegacy::update_type(instr_t type) { 
  /* Quick path for nullifying instructions */
  if (type == I_NULL || type == I_DONE) { 
    instr_type = type;
    return;
  }

  if (instr_is_branch()) {
    return;
  }

  switch (instr_type) 
  {
    case I_REP_HLT:
    {
      instr_type = I_REP_HLT;
      break;
    }
    case I_PRE_DECODE:  
    /* Allow conditionals to be nullified */
    case I_CMEQ:
    case I_CMNE:
    /* Branch fall throughs */
    case I_LDW:
    case I_STW:
    {
      instr_type = type; 
      break;
    }
    default:
    {
			std::cerr << "TYPE IS: " << instr_type << "\n"; 
      throw ExitSim("Attempting to update non I_PRE_DECODE instr", 1); 
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: InstrLegacy::dump()
////////////////////////////////////////////////////////////////////////////////
// dumps (prints) fields of the InstrLegacy class
////////////////////////////////////////////////////////////////////////////////
void
InstrLegacy::dump()
{
  fprintf(stderr, "-------INSTR STATE--------\n");
  fprintf(stderr, "Thread %d\n", get_local_thread_id());
  fprintf(stderr, "INSTRUCTION: "); dis_print(stderr); fprintf(stderr, "\n");
  fprintf(stderr, "PC: 0x%08x NextPC: 0x%08x PredNextPC: 0x%08x\n",
    pc, next_pc, pred_next_pc);
  fprintf(stderr, "Type: %d RAW: 0x%08x\n", instr_type, raw_instr);
  fprintf(stderr, "EA: %08x LockReg: %01d\n", result_ea, _has_lock);
  fprintf(stderr, "Pipeline: %01d CorrectPath: %01d Stall: %01d "
                  "Mispredict: %01d\n", 
    pipe, correct_path, stall, mispredict);
  fprintf(stderr, "Bypass Sources (0,1,2,3): (%01d, %01d, %01d, %01d)\n", 
    bypass_s0, bypass_s1, bypass_s2, bypass_s3);
}

////////////////////////////////////////////////////////////////////////////////
// Below are hooks needed for libfd to print out debugging information that is
// used by the user interface code.
////////////////////////////////////////////////////////////////////////////////
//  extern "C" {
//    typedef struct dis_private_data {
//      uint32_t raw_instr;
//      uint32_t target_addr;
//    } dis_priv_t;
//  
//    int 
//    dis_read_mem(bfd_vma memaddr, bfd_byte *myaddr, 
//      unsigned int len, struct disassemble_info *info) 
//    { 
//      dis_priv_t *d = (dis_priv_t *)info->private_data;
//      *((uint32_t *)myaddr) = d->raw_instr;
//      return 0; 
//    }
//  
//    void 
//    dis_print_addrfunt(bfd_vma addr, struct disassemble_info *info) {
//    //  dis_priv_t *d = (dis_priv_t *)info->private_data;
//      (*(info->fprintf_func))(info->stream, "0x%04x", addr);
//    }
//  }
//  

////////////////////////////////////////////////////////////////////////////////
// InstrLegacy::dis_print()
////////////////////////////////////////////////////////////////////////////////
// Prints a disassembled pretty-printed version of the instruction
////////////////////////////////////////////////////////////////////////////////
size_t
InstrLegacy::dis_print(FILE * stream) { //has a default param, see .h (stdout)
#ifndef ENABLE_GPLV3
	return 0;
#else
  // Do not print completed instructions
  if (get_type() == I_DONE) return 0;

  dis_priv_t dis_priv;

  memset(&(this->dis_info), 0, sizeof(this->dis_info));

  string_descriptor s(64);

  dis_info.endian = BFD_ENDIAN_LITTLE;
  dis_info.fprintf_func = (fprintf_ftype)string_descriptor_snprintf;
  dis_info.stream = &s;
  dis_info.read_memory_func = dis_read_mem;
  dis_info.mach = bfd_mach_mipsrigel32;
  dis_info.print_address_func = dis_print_addrfunt;

  /* Used in disassembler callbacks.  EA() may not be valid */
  dis_priv.raw_instr = this->get_raw_instr();
  dis_priv.target_addr = this->get_result_ea();
  dis_info.private_data = &dis_priv;

  if (this->instr_is_branch()) {
      dis_info.insn_type = dis_branch;
      dis_info.target = this->get_currPC();
  } else {
      dis_info.insn_type = dis_nonbranch;
  }
#ifndef _WIN32
  print_insn_little_mips((bfd_vma)0, &(this->dis_info));
  size_t num_chars = s.length();
  fprintf(stream, "%s", s.buf);
#endif
  return num_chars;
#endif //#ifndef ENABLE_GPLV3
}

////////////////////////////////////////////////////////////////////////////////
// InstrLegacy::doneMemAccess()
////////////////////////////////////////////////////////////////////////////////
// accessor for doneMemAccess
////////////////////////////////////////////////////////////////////////////////
bool 
InstrLegacy::doneMemAccess() { 
  return _doneMemAccess; 
}

////////////////////////////////////////////////////////////////////////////////
// InstrLegacy::setMemAccess()
////////////////////////////////////////////////////////////////////////////////
// Called from MEM stage saying we have passed this point.  Either the value is
// a L1 hit and we forward (by calling setCCAccess) or it is a miss and it is
// picked up later from the CC stage when setCCAccess is finally called.
////////////////////////////////////////////////////////////////////////////////
void 
InstrLegacy::setMemAccess() { 
  assert(_doneCCAccess != true && 
    "Trying to setMemAccess after it has already been set!");

  _doneMemAccess = true; 
}

////////////////////////////////////////////////////////////////////////////////
// InstrLegacy::doneCCAccess()
////////////////////////////////////////////////////////////////////////////////
bool 
InstrLegacy::doneCCAccess() { 
  return _doneCCAccess; 
}

////////////////////////////////////////////////////////////////////////////////
// InstrLegacy::setCCAccess()
////////////////////////////////////////////////////////////////////////////////
// Called from MEM or CC stage when the read has completed.  The value can then be
// forwarded and the register unlocked.  We need to make sure that we signal to
// the WB stage that to only write the register file and not do any other
// updates.
void 
InstrLegacy::setCCAccess() { 
  assert(_doneCCAccess != true && 
    "Trying to setCCAccess after it has already been set!");

  // Disallow CC stage to execute again
  _doneCCAccess = true; 

}

////////////////////////////////////////////////////////////////////////////////
// instr_is_()
////////////////////////////////////////////////////////////////////////////////
// Helper functions for the InstrLegacy class.  Separated out to reduce size of
// InstrLegacy.  Every instruction should be covered by one of:
// instr_is_memaccess, instr_is_branch, instr_is_aluop, instr_is_fpuop
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// instr_is_memaccess()
////////////////////////////////////////////////////////////////////////////////
bool 
InstrLegacy::instr_is_memaccess(instr_t i_type) {
  switch (i_type) {
    case I_GLDW: 
    case I_GSTW:
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
      { return true; }
    default:
      { return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_global()
////////////////////////////////////////////////////////////////////////////////
bool 
InstrLegacy::instr_is_global(instr_t i_type) {
  switch (i_type) {
    case I_GLDW: 
    case I_GSTW:
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
InstrLegacy::instr_is_local_atomic(instr_t i_type) {
  switch (i_type) {
    case I_STC:
    case I_LDL: 
      { return true; }
    default:
      { return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_global_atomic()
////////////////////////////////////////////////////////////////////////////////
bool 
InstrLegacy::instr_is_global_atomic(instr_t i_type) {
  switch (i_type) {
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
InstrLegacy::instr_is_atomic(instr_t i_type) {
  return    InstrLegacy::instr_is_global_atomic(i_type) 
         || InstrLegacy::instr_is_local_atomic(i_type);
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_memaccess()
////////////////////////////////////////////////////////////////////////////////
// Memory operations that are expected to hit in L1D and/or L2D.
bool 
InstrLegacy::instr_is_local_memaccess(instr_t i_type) {
  switch (i_type) {
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
InstrLegacy::instr_is_prefetch(instr_t i_type) {
  switch (i_type) {
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
InstrLegacy::instr_is_local_load(instr_t i_type) {
  switch (i_type) {
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
InstrLegacy::instr_is_local_store(instr_t i_type) {
  switch (i_type) {
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
InstrLegacy::instr_is_branch(instr_t i_type) {
  switch (i_type) {
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
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_aluop()
////////////////////////////////////////////////////////////////////////////////
bool 
InstrLegacy::instr_is_aluop(instr_t i_type) {
  switch (i_type) {
    { return true; }
    default: {return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_fpuop()
////////////////////////////////////////////////////////////////////////////////
bool 
InstrLegacy::instr_is_fpuop(instr_t i_type) {
  switch (i_type) {
    { return true; }
    default: {return false; }
  }
}
////////////////////////////////////////////////////////////////////////////////
// instr_is_local_other()
////////////////////////////////////////////////////////////////////////////////
bool 
InstrLegacy::instr_is_other(instr_t i_type) {
  switch (i_type) {
    case I_NULL: 
    case I_PRE_DECODE: 
    case I_REP_HLT: 
    case I_DONE: 
    case I_BRANCH_FALL_THROUGH: 
    case I_CMNE_NOP:
    case I_NOP: 
    case I_CMEQ_NOP: 
    case I_UNDEF: 
    case I_MB:

    { return true; }
    default: {return false; }
  }
}
// FOOOOOOOOOO
bool InstrLegacy::instr_is_memaccess() {
  return InstrLegacy::instr_is_memaccess(get_type());
}
// Memory operations that are expected to hit in L1D and/or L2D.
bool InstrLegacy::instr_is_local_memaccess() {
  return InstrLegacy::instr_is_local_memaccess(get_type());
}
bool InstrLegacy::instr_is_prefetch() {
  return InstrLegacy::instr_is_prefetch(get_type());
}
bool InstrLegacy::instr_is_local_load() {
  return InstrLegacy::instr_is_local_load(get_type());
}
bool InstrLegacy::instr_is_local_store() {
  return InstrLegacy::instr_is_local_store(get_type());
}
bool InstrLegacy::instr_is_branch() {
  return InstrLegacy::instr_is_branch(get_type());
}
bool InstrLegacy::instr_is_aluop() {
  return InstrLegacy::instr_is_aluop(get_type());
}
bool InstrLegacy::instr_is_fpuop() {
  return InstrLegacy::instr_is_fpuop(get_type());
}
bool InstrLegacy::instr_is_other() {
  return InstrLegacy::instr_is_other(get_type());
}
bool InstrLegacy::instr_is_local_atomic() {
  return InstrLegacy::instr_is_local_atomic(get_type());
}
bool InstrLegacy::instr_is_global_atomic() {
  return InstrLegacy::instr_is_global_atomic(get_type());
}
bool InstrLegacy::instr_is_atomic() {
  return InstrLegacy::instr_is_atomic(get_type());
}
bool InstrLegacy::instr_is_global() {
  return InstrLegacy::instr_is_global(get_type());
}


