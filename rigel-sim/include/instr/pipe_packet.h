#ifndef __INSTR_NEW_H__
#define __INSTR_NEW_H__

#include <cstring>
#include <cassert>
#include <map>
#include "instr/instrbase.h"
#include "sim.h"
#include "core/regfile.h" // FIXME remove this dependence on regval32_t
#ifdef ENABLE_GPLV3
#include <dis-asm.h>                    // for disassemble_info, etc
#endif

#define DB_INSTR 0

    enum isa_reg_t {
      SREG_S = 0,
      SREG_T,
      DREG,
      NUM_ISA_OPERAND_REGS
    };

    extern const char isa_reg_names[NUM_ISA_OPERAND_REGS][8];

////////////////////////////////////////////////////////////////////////////////
/// STATIC: TODO: make this static decode packet structure per instruction, set at first decode
////////////////////////////////////////////////////////////////////////////////
class StaticDecodeInfo {

  public:

    /// constructor
    StaticDecodeInfo() 
      :
      valid_(0),
      type_(I_PRE_DECODE)
    { };

    /// decode the instruction into useful/relevant fields
    void decode(uint32_t pc, uint32_t raw) {
      valid_ = true;
      pc_    = pc;
      raw_instr_bits = raw;

      sim_decode_type();
      StaticDecode();
      setRegs();
    }

    /// print useful information about the instruction
    void Dump() {
      printf("pc: 0x%08x raw: 0x%08x I_TYPE: %s\n", 
        pc_, raw_instr_bits, instr::instr_string[type_]);
    }

    // autogenerated
    int sim_decode_type();

    ///
    /// accessors
    ///

    /// get instruction fields
    /// TODO: auto-generate these routines
    // note: these are fragile (hardcoded), 
    // if the ISA changes they may have to be changed
    // Each of these obtains the values from the raw instruction bits
    /// FIXME: these should be private and insulated properties
    uint32_t optype(); /// autogenerated
    uint32_t opcode(); /// autogenerated 
    // FIXME autogenerate these
    uint32_t reg_d()  { return 0x0000001F & (raw_instr_bits >> 23);}
    uint32_t reg_t()  { return 0x0000001F & (raw_instr_bits >> 18);}
    uint32_t reg_s()  { return 0x0000001F & (raw_instr_bits >> 13);}
    uint32_t imm5()   { return 0x0000001F & (raw_instr_bits >> 13);}
    uint32_t imm16()  { return 0x0000FFFF & (raw_instr_bits >> 0 );}
    uint32_t imm26()  { return 0x03FFFFFF & (raw_instr_bits >> 0 );}

    uint32_t simm16() { return int32_t(int16_t(imm16()));}

    bool valid()   { return valid_; }
    instr_t type() { return type_; }

    /// TODO or not TODO:
    /// memoize some of these calls???
    /// functional units (or pipelines)
    bool    isBranch() const;
    bool    isBranchIndirect() const;
    bool    isBranchDirect() const;
    bool    isStoreLinkRegister() const;
    bool    isALU() const;
    bool    isFPU() const;
    /// memory
    bool    isMem() const;
    bool    isGlobal() const;
    bool    isLocalMem() const;
    bool    isLocalLoad() const; // (local? global?)
    bool    isLocalStore() const; // (local? global?)
    bool    isCacheControl() const; 
    /// atomics
    bool    isAtomic() const;
    bool    isStore() const;
    bool    isLoad() const;
    bool    isLocalAtomic() const;
    bool    isGlobalAtomic() const;
    /// other
    bool    isPrefetch() const;
    bool    isSimOp() const; 
    bool    isOther() const; 
    bool    isNOP() const; 
    bool    isSPRFSrc() const; 
    bool    isDREGSrc() const; 
    bool    isSPRFDest() const; 
    bool    isShift() const; 
    bool    isCompare() const; 
    // 
    bool    isSimSpecial() const; 

    void setType(instr_t t) {
      type_ = t;
    }

    /// record register names and dependencies for reg fields
    /// FIXME: autogenerate this per instruction...
    /// FIXME: AUDIT this CODE
    void setRegs() {
      // TODO FIXME: iterate these 3
      if(has_sreg_t) {
        regnums[SREG_T] = reg_t();
        input_deps[SREG_T] = reg_t();
        output_deps[SREG_T] = simconst::NULL_REG;
      } else {
        regnums[SREG_T] = simconst::NULL_REG;
        input_deps[SREG_T] = simconst::NULL_REG;
        output_deps[SREG_T] = simconst::NULL_REG;
      }
      // TODO FIXME: iterate
      if(has_sreg_s) {
        regnums[SREG_S] = reg_s();
        input_deps[SREG_S] = reg_s();
        output_deps[SREG_S] = simconst::NULL_REG;
      } else {
        regnums[SREG_S] = simconst::NULL_REG;
        input_deps[SREG_S] = simconst::NULL_REG;
        output_deps[SREG_S] = simconst::NULL_REG;
      }
      // TODO FIXME: iterate
      // FIXME: make sure these really read and/or write DREG
      if(has_dreg) {
        regnums[DREG]     = reg_d();
        if (isDREGSrc()) {
          input_deps[DREG]  = reg_d(); 
        } else {
          input_deps[DREG]  = simconst::NULL_REG;
        }
        output_deps[DREG] = reg_d(); // FIXME: might NOT be a dest...
      } else {
        if (isStoreLinkRegister()) {
          regnums[DREG]     = rigel::regs::LINK_REG;
          input_deps[DREG]  = simconst::NULL_REG;
          output_deps[DREG] = simconst::NULL_REG;
        } else {
          regnums[DREG]     = simconst::NULL_REG;
          input_deps[DREG]  = simconst::NULL_REG;
          output_deps[DREG] = simconst::NULL_REG;
        }
      }
    }

    uint32_t StaticDecode();

    int32_t regnums[NUM_ISA_OPERAND_REGS];
    int32_t input_deps[NUM_ISA_OPERAND_REGS];
    int32_t output_deps[NUM_ISA_OPERAND_REGS];

    // TODO FIXME make private
    bool has_dreg;
    bool has_sreg_t;
    bool has_sreg_s;
    bool has_imm5;
    bool has_imm16;
    bool has_imm26;

  private:
    ///
    /// private data members
    ///

    bool     valid_;

    /// basic decode information
    uint32_t pc_;
    uint32_t raw_instr_bits;
    instr_t  type_;  /// enum, the instruction type


    uint32_t decode_flags;

};

struct string_descriptor;

////////////////////////////////////////////////////////////////////////////////
// class
////////////////////////////////////////////////////////////////////////////////
class PipePacket { // : public InstrBase { // maybe later...
  
  public:

    /// constructor
    PipePacket( 
      uint32_t pc,   /// program counter for this instruction
      uint32_t raw,  /// raw instruction bits
      int      tid
    ) :
      pc_(pc),
      raw_instr_bits(raw),
      tid_(tid),
      sdInfo_(decode()),
      target_addr(0),
      nextPC_(0)
    { 
      //sdInfo_ = decode(); 
    }

    ///
    /// accessors
    ///
  
    uint32_t nextPC()           { return nextPC_; }
    void     nextPC(uint32_t p) { 
      assert(p%4 == 0 && "pc must be aligned!");
      nextPC_ = p; 
    }
    uint32_t pc()     { return pc_; }
    uint32_t tid()    { return tid_; }
    //uint32_t raw_instr_bits() { return raw_instr_bits; }
    //const StaticDecodeInfo& sdInfo() { return sdInfo_; }

    instr_t type() { return sdInfo_.type(); }  

    // passthrough to static decode packet
    bool    isBranch() const         { return sdInfo_.isBranch();         };
    bool    isBranchIndirect() const { return sdInfo_.isBranchIndirect(); };
    bool    isBranchDirect() const   { return sdInfo_.isBranchDirect();   };
    bool    isStoreLinkRegister() const   { return sdInfo_.isStoreLinkRegister();   };
    bool    isALU() const            { return sdInfo_.isALU();            };
    bool    isFPU() const            { return sdInfo_.isFPU();            };
    /// memory
    bool    isMem() const            { return sdInfo_.isMem();            };
    bool    isGlobal() const         { return sdInfo_.isGlobal();         };
    bool    isLocalMem() const       { return sdInfo_.isLocalMem();       };
    bool    isCacheControl() const   { return sdInfo_.isCacheControl();   };
    //bool    isLocalLoad() const      { return sdInfo_.isLocalLoad();      }; // (local? global?)
    //bool    isLocalStore() const     { return sdInfo_.isLocalStore();     }; // (local? global?)
    /// atomics
    bool    isAtomic() const         { return sdInfo_.isAtomic();         };
    bool    isStore() const          { return sdInfo_.isStore();          };
    bool    isLoad() const           { return sdInfo_.isLoad();           };
    //bool    isLocalAtomic() const    { return sdInfo_.isLocalAtomic();    };
    //bool    isGlobalAtomic() const   { return sdInfo_.isGlobalAtomic();   };
    /// other
    bool    isPrefetch() const       { return sdInfo_.isPrefetch();       };
    //bool    isSimOp() const          { return sdInfo_.isSimOp();          }; 
    bool    isOther() const          { return sdInfo_.isOther();          }; 
    bool    isNOP() const            { return sdInfo_.isNOP();            }; 
    bool    isSPRFSrc() const        { return sdInfo_.isSPRFSrc();        }; 
    bool    isDREGSrc() const        { return sdInfo_.isDREGSrc();        }; 
    bool    isSPRFDest() const       { return sdInfo_.isSPRFDest();       }; 
    bool    isShift() const          { return sdInfo_.isShift();          }; 
    bool    isCompare() const        { return sdInfo_.isCompare();        }; 
    //
    bool    isSimSpecial() const     { return sdInfo_.isSimSpecial();     }; 

  ///
  /// public methods
  /// 
  public:

    void pretty_print(FILE * stream = stdout) {
      dis_print(); 
      fprintf(stream,"\n");
    }

    /// dump useful internal state
    void Dump() {
      pretty_print();
      sdInfo_.Dump();
      // print regvals
      printf("regs: ");
      for(int i=0;i<NUM_ISA_OPERAND_REGS;i++) {
        printf("%s:%08x ",isa_reg_names[i],regvals[i].i32()); /// for storing temporary register values
      } printf("\n");
    };

    disassemble_info dis_info;
    ////////////////////////////////////////////////////////////////////////////////
    // Prints a disassembled pretty-printed version of the instruction
    ////////////////////////////////////////////////////////////////////////////////
    size_t
    dis_print(FILE * stream = stdout) { //has a default param, see .h (stdout)
#ifndef ENABLE_GPLV3
			return 0;
#else
      // Do not print completed instructions (why not?)
      //if (get_type() == I_DONE) return 0;
    
      dis_priv_t dis_priv;
    
      memset(&(dis_info), '\0', sizeof(disassemble_info));
    
      string_descriptor s(64);
    
      dis_info.endian = BFD_ENDIAN_LITTLE;
      dis_info.fprintf_func = (fprintf_ftype)string_descriptor_snprintf;
      dis_info.stream = &s;
      dis_info.read_memory_func = dis_read_mem;
      dis_info.mach = bfd_mach_mipsrigel32;
      dis_info.print_address_func = dis_print_addrfunt;
    
      /* Used in disassembler callbacks.  EA() may not be valid */
      dis_priv.raw_instr = raw_instr_bits;
      dis_priv.target_addr = target_addr; 
      dis_info.private_data = &dis_priv;
   
      if (sdInfo_.isBranch()) {
          dis_info.insn_type = dis_branch;
          dis_info.target = pc_;
      } else {
          dis_info.insn_type = dis_nonbranch;
      }
      print_insn_little_mips((bfd_vma)0, &(dis_info));
      size_t num_chars = s.length();
      fprintf(stream, "%s", s.buf);
      printf("num chars: %zu \n", num_chars);
      return num_chars;
#endif //#ifndef ENABLE_GPLV3
    }

    /// r: which register to set (T,S,D)
    /// v: 
    void setRegVal(isa_reg_t r, regval32_t v) {
      /// TODO: compile out bounds check
      assert(r>=0 && r< NUM_ISA_OPERAND_REGS);
      //if(r < 0 || r >= NUM_ISA_OPERAND_REGS) {
      //  printf("unexpected value %d for r\n",r);
      //  assert(0 && "invalid register specified"); 
      //}
      regvals[r] = v;
    }

    uint32_t regnum(isa_reg_t r) {
      assert(r >= 0 && r < NUM_ISA_OPERAND_REGS);
      return sdInfo_.regnums[r];
    }

    uint32_t input_deps(isa_reg_t r) {
      assert(r >= 0 && r < NUM_ISA_OPERAND_REGS);
      return sdInfo_.input_deps[r];
    }

    regval32_t& regval(isa_reg_t r) {
      assert(r>=0 && r<=NUM_ISA_OPERAND_REGS);
      return regvals[r];
    }

    regval32_t& dreg() {
      return regvals[DREG];
    }

    regval32_t& sreg_s() {
      return regvals[SREG_T];
    }

    regval32_t& sreg_t() {
      return regvals[SREG_T];
    }

    // TODO FIXME: relete me, move functionality into sdInfo
    bool has_imm5()  { return sdInfo_.has_imm5;   }
    bool has_imm16() { return sdInfo_.has_imm16;  }
    bool has_imm26() { return sdInfo_.has_imm26;  }
    bool has_sreg_t() { return sdInfo_.has_sreg_t;  }
    bool has_sreg_s() { return sdInfo_.has_sreg_s;  }

    uint32_t imm26() { return sdInfo_.imm26();  }
    uint32_t imm16() { return sdInfo_.imm16();  }
    uint32_t imm5()  { return sdInfo_.imm5();   }

    uint32_t simm16(){ return sdInfo_.simm16();  }

    
  /// private methods
  private:

    /// decode
    /// decode the instruction into internal subfields as desired
    StaticDecodeInfo& decode() {
      // use pre-existing decoded info when it exists
      // otherwise, decode the instruction
      if (!decodedInstructions[pc_].valid()) {
        DPRINT(DB_INSTR,"SDInfo:Decoding unseen pc %08x\n", pc_);
        decodedInstructions[pc_].decode(pc_,raw_instr_bits);
      }
      return decodedInstructions[pc_];
    }

  /// private member data
  private:

    /// basic attributes
    uint32_t pc_;        /// program counter associated with this instruction
    uint32_t raw_instr_bits;  /// for internal class use only

    int tid_;  /// thread id: whose data is in this pipe packet?

    /// pointer to static decode information
    StaticDecodeInfo& sdInfo_;

    ///////////////////////////////////////////////////////////////////////////
    /// DYNAMIC: these change at runtime
    ///////////////////////////////////////////////////////////////////////////

    /// computed values

    uint32_t target_addr;
    uint32_t nextPC_;

    /// input values
    regval32_t regvals[NUM_ISA_OPERAND_REGS]; /// for storing temporary register values

    /// static structure for holding STATIC instruction decode data
    static std::map<uint32_t,StaticDecodeInfo> decodedInstructions;

    ///////////////////////////////////////////////////////////////////////////

};

#endif
