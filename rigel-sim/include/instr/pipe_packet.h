#ifndef __INSTR_NEW_H__
#define __INSTR_NEW_H__

#include <cstring>
#include <cassert>
#include <map>
#include "instr/instr_base.h"
#include "sim.h"
#include "instr/static_decode_info.h"
#include "core/regfile.h" // FIXME remove this dependence on regval32_t
#ifdef ENABLE_GPLV3
#include <dis-asm.h>                    // for disassemble_info, etc
#endif

#define DB_INSTR 0

// forward declarations
class Packet;

extern const char isa_reg_names[NUM_ISA_OPERAND_REGS][8];

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
      _valid(true),
      _completed(false),
      _request_pending(false),
      _pc(pc),
      raw_instr_bits(raw),
      _tid(tid),
      _mem_request(0),
      _sdInfo(decode()),
      _target_addr(0),
      _branch_predicate(0),
      _nextPC(0)
    { 
      //_sdInfo = decode(); 
    }

    ///
    /// accessors
    ///
  
    uint32_t nextPC()           { return _nextPC; }
    uint32_t pc()               { return _pc; }
    uint32_t tid()              { return _tid; }
    uint32_t target_addr()      { return _target_addr; }
    bool     branch_predicate() { return _branch_predicate; }

    void     nextPC(uint32_t p) { 
      assert(p%4 == 0 && "pc must be aligned!");
      _nextPC = p; 
    }
    void target_addr(uint32_t ta) { _target_addr = ta; }
    void branch_predicate(bool p) { _branch_predicate = p; }

    //uint32_t raw_instr_bits() { return raw_instr_bits; }

    instr_t type() { return _sdInfo.type(); }  

    // passthrough to static decode packet
    bool    isBranch() const         { return _sdInfo.isBranch();         };
    bool    isBranchIndirect() const { return _sdInfo.isBranchIndirect(); };
    bool    isBranchDirect() const   { return _sdInfo.isBranchDirect();   };
    bool    isStoreLinkRegister() const   { return _sdInfo.isStoreLinkRegister();   };
    bool    isALU() const            { return _sdInfo.isALU();            };
    bool    isFPU() const            { return _sdInfo.isFPU();            };
    /// memory
    bool    isMem() const            { return _sdInfo.isMem();            };
    bool    isGlobal() const         { return _sdInfo.isGlobal();         };
    bool    isLocalMem() const       { return _sdInfo.isLocalMem();       };
    bool    isCacheControl() const   { return _sdInfo.isCacheControl();   };
    //bool    isLocalLoad() const      { return _sdInfo.isLocalLoad();      }; // (local? global?)
    //bool    isLocalStore() const     { return _sdInfo.isLocalStore();     }; // (local? global?)
    /// atomics
    bool    isAtomic() const         { return _sdInfo.isAtomic();         };
    bool    isStore() const          { return _sdInfo.isStore();          };
    bool    isLoad() const           { return _sdInfo.isLoad();           };
    //bool    isLocalAtomic() const    { return _sdInfo.isLocalAtomic();    };
    //bool    isGlobalAtomic() const   { return _sdInfo.isGlobalAtomic();   };
    /// other
    bool    isPrefetch() const       { return _sdInfo.isPrefetch();       };
    //bool    isSimOp() const          { return _sdInfo.isSimOp();          }; 
    bool    isOther() const          { return _sdInfo.isOther();          }; 
    bool    isNOP() const            { return _sdInfo.isNOP();            }; 
    bool    isSPRFSrc() const        { return _sdInfo.isSPRFSrc();        }; 
    bool    isDREGSrc() const        { return _sdInfo.isDREGSrc();        }; 
    bool    isDREGDest() const       { return (! _sdInfo.isDREGNotDest());}; 
    bool    isSPRFDest() const       { return _sdInfo.isSPRFDest();       }; 
    bool    isShift() const          { return _sdInfo.isShift();          }; 
    bool    isCompare() const        { return _sdInfo.isCompare();        }; 
    //
    bool    isSimSpecial() const     { return _sdInfo.isSimSpecial();     }; 

  ///
  /// public methods
  /// 
  public:

    void pretty_print(FILE * stream = stdout) {
      dis_print(); 
      //fprintf(stream,"\n");
    }

    /// dump useful internal state
    void Dump() {
      pretty_print();
      _sdInfo.Dump();
      printf("valid:%d completed:%d request_pending:%d\n", _valid, _completed, _request_pending);
      // print regvals
      printf("regs: ");
      for(int i=0;i<NUM_ISA_OPERAND_REGS;i++) {
        printf("%s:%08x ",isa_reg_names[i],regvals[i].i32()); /// for storing temporary register values
      } 
      printf("\n");
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
      dis_priv.target_addr = _target_addr; 
      dis_info.private_data = &dis_priv;
   
      if (_sdInfo.isBranch()) {
          dis_info.insn_type = dis_branch;
          dis_info.target = _pc;
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
      return _sdInfo.regnums[r];
    }

    uint32_t input_deps(isa_reg_t r) {
      assert(r >= 0 && r < NUM_ISA_OPERAND_REGS);
      return _sdInfo.input_deps[r];
    }

    /// for now, we assume the only output dependency of an instruction is the DREG field
    /// however, we can extend this in the future if desired
    uint32_t output_deps() {
      return _sdInfo.output_deps[DREG];
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

    bool has_imm5()   { return _sdInfo.has_imm5;   }
    bool has_imm16()  { return _sdInfo.has_imm16;  }
    bool has_imm26()  { return _sdInfo.has_imm26;  }
    bool has_sreg_t() { return _sdInfo.has_sreg_t;  }
    bool has_sreg_s() { return _sdInfo.has_sreg_s;  }

    uint32_t imm26() { return _sdInfo.imm26();  }
    uint32_t imm16() { return _sdInfo.imm16();  }
    uint32_t imm5()  { return _sdInfo.imm5();   }

    uint32_t simm16(){ return _sdInfo.simm16();  }

    bool valid()      { return _valid; }
    void invalidate() { _valid = false; }

    bool isCompleted()  { return _completed; }
    void setCompleted() { _completed = true; }

    bool requestPending()    { return _request_pending; }
    void setRequestPending() { _request_pending = true; }

    Packet* memRequest() { return _mem_request; }
    void memRequest(Packet* p) { _mem_request = p; }
    
  /// private methods
  private:

    /// decode
    /// decode the instruction into internal subfields as desired
    StaticDecodeInfo& decode() {
      // use pre-existing decoded info when it exists
      // otherwise, decode the instruction
      if (!decodedInstructions[_pc].valid()) {
        DPRINT(DB_INSTR,"SDInfo:Decoding unseen pc %08x\n", _pc);
        decodedInstructions[_pc].decode(_pc,raw_instr_bits);
      }
      return decodedInstructions[_pc];
    }

  /// private member data
  private:

    /// basic attributes
    bool _valid;
    bool _completed;
    bool _request_pending;

    uint32_t _pc;        /// program counter associated with this instruction
    uint32_t raw_instr_bits;  /// for internal class use only

    int _tid;  /// thread id: whose data is in this pipe packet?

    Packet* _mem_request; ///< pointer to outstanding memory request

    /// pointer to static decode information
    StaticDecodeInfo& _sdInfo;


    ///////////////////////////////////////////////////////////////////////////
    /// DYNAMIC: these change at runtime
    ///////////////////////////////////////////////////////////////////////////

    /// computed values

    uint32_t _target_addr;
    bool _branch_predicate;
    uint32_t _nextPC;

    /// input values
    regval32_t regvals[NUM_ISA_OPERAND_REGS]; /// for storing temporary register values

    /// static structure for holding STATIC instruction decode data
    static std::map<uint32_t,StaticDecodeInfo> decodedInstructions;

    ///////////////////////////////////////////////////////////////////////////

};

#endif
