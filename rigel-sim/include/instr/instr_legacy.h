
////////////////////////////////////////////////////////////////////////////////
// instr_legacy.h
////////////////////////////////////////////////////////////////////////////////
//
// The "Instr" class that contains most all data passed down the pipeline for a
// given instruction is defined here
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __INSTR_LEGACY_H__
#define __INSTR_LEGACY_H__

#include "sim.h"
// InstrStats only includes sim.h so this is kosher to include here 
#include "instrstats.h"
// There is a dependence between reg_file and instr and fixing it is probably
// not worth the effort.
#include "core/regfile_legacy.h"
#include "instrbase.h"
#include <string>

////////////////////////////////////////////////////////////////////////////////
// InstrSlot
////////////////////////////////////////////////////////////////////////////////
// For clarity of code.  (used universally)
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CLASS NAME: InstrLegacy
////////////////////////////////////////////////////////////////////////////////
//
// This class contains most information that is passed down the pipeline as an
// instruction is executed. In general, instructions are allocated at fetch and
// dellocated later (retirement,flushing). Between stages, pointers to active
// instrs are passed. 
//
// XXX NOTE: No dynamic memory!  No pointers.  If you add them, you will need to
// come into line with the rule of three!
// 
////////////////////////////////////////////////////////////////////////////////
class InstrLegacy : public InstrBase {

  //////////////////////////////////////////////////////////////////////////////
  // public methods
  //////////////////////////////////////////////////////////////////////////////
  public:

    /* Constructors/Destructors */
    InstrLegacy();
    InstrLegacy(uint32_t _pc, int _tid);
    InstrLegacy(uint32_t _pc, int _tid, uint32_t _pred_pc, uint32_t _raw_instr,  instr_t i_type);
    ~InstrLegacy() { }

    void dump();     // Dump the state for debugging purposes

    // required by InstrBase
    void decode() { assert( 0 && "unimplemented" ); }

    // Instruction type checking routines
    bool instr_is_other();
    bool instr_is_memaccess();
    bool instr_is_fpuop();
    bool instr_is_aluop();
    bool instr_is_local_memaccess();
    bool instr_is_branch();
    bool instr_is_local_store();
    bool instr_is_local_load();
    bool instr_is_prefetch();
    bool instr_is_global_atomic();
    bool instr_is_local_atomic();
    bool instr_is_atomic();
    bool instr_is_global();

    // Static versions for use by other classes
    static bool instr_is_other(           instr_t i_type  );
    static bool instr_is_memaccess(       instr_t i_type  );
    static bool instr_is_fpuop(           instr_t i_type  );
    static bool instr_is_aluop(           instr_t i_type  );
    static bool instr_is_local_memaccess( instr_t i_type  );
    static bool instr_is_branch(          instr_t i_type  );
    static bool instr_is_local_store(     instr_t i_type  );
    static bool instr_is_local_load(      instr_t i_type  );
    static bool instr_is_prefetch(        instr_t i_type  );
    static bool instr_is_global_atomic(   instr_t i_type  );
    static bool instr_is_local_atomic(    instr_t i_type  );
    static bool instr_is_atomic(          instr_t i_type  );
    static bool instr_is_global(          instr_t i_type  );

    // Set at ifetch.  Used by SYNC to check at writeback when sync is done
    void SetLastIntrOnCore(uint64_t inum) { last_instr_on_core = inum; }

    /* Accessor Methods */
    uint64_t  GetInstrNumber()        { return InstrNumber; }
    instr_t   get_type()              { return instr_type;  }
    uint32_t  get_raw_instr()         { return raw_instr;   }
    uint64_t  get_spawn_cycle()       { return spawn_cycle; }
    uint64_t  get_fetch_cycle()       { return fetch_cycle; }

    // Used for determining the latency of memory operations.
    uint64_t  get_first_memaccess_cycle() const { return first_memaccess_cycle; }
    uint64_t  get_last_memaccess_cycle() const  { return last_memaccess_cyle; }

    // Return the type that the instruction was decoded as.
    instr_t   get_type_decode() const { return instr_type_decode;  }
    void set_type_decode(instr_t type) { instr_type_decode = type; }
    void set_type(instr_t type)  { instr_type = type;  }
    void set_fetch_cycle(uint64_t cycle) { fetch_cycle = cycle;  }

    // Only allow the cycle to be set once.
    void set_first_memaccess_cycle() { 
      if (0 == first_memaccess_cycle) { 
        first_memaccess_cycle = rigel::CURR_CYCLE; 
      }  
    }
    void set_last_memaccess_cycle() { 
      last_memaccess_cyle = rigel::CURR_CYCLE; 
    }

    // Use for tracking failed store-conditionals
    void set_bad_stc() { bad_stc = true; }
    bool get_bad_stc() { return bad_stc; }

    /* Will fail if type != I_PRE_DECODE */
    void update_type(instr_t type);

    // note: these are fragile (hardcoded), 
    // if the ISA changes they may have to be changed
    // Each of these obtains the values from the raw instruction bits
    uint32_t get_opcode() { return 0xF    & (raw_instr >> 28);}
    uint32_t get_reg_d()  { return 0x1F   & (raw_instr >> 23);}
    uint32_t get_reg_t()  { return 0x1F   & (raw_instr >> 18);}
    uint32_t get_reg_s()  { return 0x1F   & (raw_instr >> 13);}
    uint32_t get_imm5()   { return 0x1F   & (raw_instr >> 13);}

    // In the future we should have a way to change the bitfield width
    // dynamically, in case we decide to add more accumulators
    uint32_t get_acc_d()  { return 0x1F  & (raw_instr >> 23);}
    uint32_t get_acc_s()  { return 0x1F  & (raw_instr >> 8);}

    // Used to determine how many regfile read ports this instruction uses
    uint32_t get_num_read_ports() { return num_read_ports;}
    void set_num_read_ports(uint32_t num_ports){ num_read_ports = num_ports; }
    // immediate value accessors
    int32_t get_simm() {
      if (src_imm & 0x8000)
        return (0xFFFF0000 | (int32_t)src_imm);
      else
        return (0x0000FFFF & (int32_t)src_imm);
    }
    uint32_t get_imm()    { return 0x0FFFF & src_imm; }
    uint32_t _get_imm()   { return (0xFFFF & (raw_instr >> 0));}

    // register accessors...TODO: make these arrays?
    RegisterBase get_sreg0()        { return src_reg0;    }
    RegisterBase get_sreg1()        { return src_reg1;    }
    RegisterBase get_sreg2()        { return src_reg2;    }
    RegisterBase get_sreg3()        { return src_reg3;    }
    RegisterBase get_sacc()         { return src_acc;     }
    RegisterBase get_dacc()         { return dest_acc;    }
    RegisterBase get_dreg()         { return dest_reg;    }
    RegisterBase get_result_reg()   { return result_reg;  }
    uint32_t     get_result_ea()    { return result_ea;   }

    // register file accessors

    // sets the destination and source register numbers, and apparently also the
    // immediate value as well within this same function....
    void set_regs(uint32_t s0, uint32_t s1, uint32_t d)
    {
      src_reg0.addr = s0; src_reg1.addr =s1; dest_reg.addr =d;
      src_imm = _get_imm();
    }
    // Overloaded set_regs function to allow reading/writing of accumulators
    void set_regs(uint32_t acc_src,uint32_t s0, uint32_t s1, uint32_t d,uint32_t acc_dest)
    {
      src_reg0.addr = s0; src_reg1.addr =s1; dest_reg.addr =d;
      src_acc.addr = acc_src; dest_acc.addr = acc_dest;
      src_imm = _get_imm();
    }

    void set_sreg2(RegisterBase s) { src_reg2 = s; }
    void set_result_RF(uint32_t val, uint32_t regnum)
      { result_reg = RegisterBase(regnum, val);}
    void set_result_RF(int32_t val, uint32_t regnum)
      { result_reg = RegisterBase(regnum, *((uint32_t *)&val));}
    void set_result_RF(float val, uint32_t regnum)
      { result_reg = RegisterBase(regnum, val);}
    /* Used by LD/St and also jump/branch with link */
    void set_result_RF(uint32_t val, uint32_t regnum, uint32_t ea)
      { set_result_RF(val, regnum); result_ea = ea;}


    size_t dis_print(FILE *stream = stdout); //Return number of characters printed
		std::string dis_print_string() { return std::string();}
    /* By default, instructions should be on a good path.  If we discover
     * otherwise, say in executing a mispredicted branch, we take the corrective
     * measures here.
     */
    bool is_correct_path()  { return correct_path;  }
    void set_bad_path()     { correct_path = false; }
    bool is_stalled()       { return stall;         }
    void set_stalled()      { stall = true;         }
    void unstall()          { stall = false;        }

    int get_pipe()          { return pipe;          }
    void set_pipe(int pipe) { this->pipe = pipe;    }
    void set_doneExec()     { done_execute = true;  }
    bool doneExec()         { return done_execute;  }

    bool is_badPath()       { return !correct_path; }
    bool is_mispredict()    { return mispredict;    }
    void set_mispredict()   { mispredict = true;    }
    uint32_t get_glHist()   { return ghistory;      } // return a UINT32_T...NOT A BOOLEAN
    void set_glHist(uint32_t gh) { ghistory = gh;   }

    uint32_t get_exLat()      { return INSTR_LAT[instr_type];  }
    bool is_done_decode()     { return done_decode;     }
    void set_done_decode()    { done_decode = true;     }
    bool is_done_regaccess()  { return done_regaccess;  }
    void set_done_regaccess() { assert(done_regaccess == false); done_regaccess = true; }
    bool did_sb_get()         { return sb_get;          }
    void set_sb_get()         { assert(sb_get == false); sb_get = true; }

    // sets bypass to true (what does this _DO_?)
    void set_bypass_sreg0() { bypass_s0 = true; }
    void set_bypass_sreg1() { bypass_s1 = true; }
    void set_bypass_sreg2() { bypass_s2 = true; }
    void set_bypass_sreg3() { bypass_s3 = true; }
    void set_bypass_sacc()  { bypass_sacc = true; }

    // returns value of bypass_X
    bool is_bypass_sreg0() { return bypass_s0; }
    bool is_bypass_sreg1() { return bypass_s1; }
    bool is_bypass_sreg2() { return bypass_s2; }
    bool is_bypass_sreg3() { return bypass_s3; }
    bool is_bypass_sacc() { return bypass_sacc; }

    // execution cycles accessors
    uint32_t get_exCycles()   { return exCycles;   }
    uint32_t inc_exCycyles()  { return ++exCycles; }

    // mem and cache access checks
    bool doneMemAccess();
    void setMemAccess();
    bool doneCCAccess();
    void setCCAccess();

    void SetDoneRegRead() { _regread_done = true; }
    bool GetDoneRegRead() { return _regread_done; }

    // Return true if this was a BCAST_UPDATE that has already finished.
    bool IsCompletedBCAST_UPDATE()  { return is_completed_BCAST_UPDATE; }
    void SetCompletedBCAST_UPDATE() { is_completed_BCAST_UPDATE = true; }


    // Thread ID Accessors for MultiThreading (also used for single-threaded
    // mode for forward compatibility)
    int get_core_thread_id()    { 

      if( thread_id < 0 ) { // debug/invalid thread IDs
        return thread_id;
      } else { // valid thread id range
        return thread_id % rigel::THREADS_PER_CORE; 
      }

    }
    // TODO: rename to get_cluster_thread_id()
    int get_local_thread_id()   { 
      return thread_id % (rigel::CORES_PER_CLUSTER*rigel::THREADS_PER_CORE); 
    }
    int get_global_thread_id()  { 
      return thread_id; 
    } 

    // should these be members of the scoreboard?
    void set_lockReg(uint32_t reg) {
      lock_reg = reg;
      _has_lock = true;
    }
    bool has_lock(uint32_t &reg) {
      reg = lock_reg;
      if (_has_lock)  return true;
      else            return false;
    }

    // PC-related Accessors: PC, nextPC, predNextPC, currPC, etc.
    void set_predNextPC(uint32_t addr) {
      assert(((addr & 0x00000003) == 0) && "Unaligned pred PC");
      this->pred_next_pc = addr;
    }
    void set_nextPC(uint32_t addr) {
      if ((addr & 0x00000003) != 0) {
        DEBUG_HEADER();
        fprintf(stderr, "Unaligned next PC - currPC: 0x%08x\n", this->pc);
        assert(0);
      }
      this->next_pc = addr;
    }
    uint32_t get_nextPC() {
      //assert(((this->next_pc & 0x00000003) == 0) && "Unaligned next PC");
      return this->next_pc;
    }
    uint32_t get_predNextPC() {
      assert(((this->pred_next_pc & 0x00000003) == 0) && "Unaligned pred PC");
      return this->pred_next_pc;
    }
    uint32_t get_currPC() {
      //assert(((this->pc & 0x00000003) == 0) && "Unaligned PC");
      return this->pc;
    }

  //////////////////////////////////////////////////////////////////////////////
  // public data members
  //////////////////////////////////////////////////////////////////////////////
  public:

    /* Unique ID for instruction */
    static uint64_t GLOBAL_INSTR_NUMBER;
    // Initialized after CORES_TOTAL in sim.cpp
    static uint64_t *LAST_INSTR_NUMBER;
    uint64_t        last_instr_on_core;

    // Track statistics on a per-instruction basis
    InstrStats stats; // TODO: see file ??? for info on this class

    int spec_list_idx;
    struct {
      uint32_t v1;
      uint32_t v2;
      uint32_t v3;
      uint32_t v4;
      uint32_t retval;
    } TQ_Data;
    bool bad_stc;

    bool is_completed_BCAST_UPDATE;

    int thread_id;

  //////////////////////////////////////////////////////////////////////////////
  // private data members
  //////////////////////////////////////////////////////////////////////////////
  private:

    // XXX BEGIN MEMBER *DATA* XXX 
    disassemble_info dis_info;

    uint32_t pc;                // the PC for this instruction
    uint32_t next_pc;           // the (correct?) next PC?
    uint32_t pred_next_pc;      // predicted next PC

    uint32_t raw_instr;         // raw 32-bit encoded instruction
    instr_t  instr_type;        // 
    instr_t  instr_type_decode; // 

    // Values consumed at WB
    uint32_t     result_ea;     // effective address 
    RegisterBase result_reg;    // Register file WB 

    // Register Number.  Sources are overwritten in RF 
    RegisterBase src_reg0, src_reg1, src_reg2, src_reg3, dest_reg;

    // Need to place accumulator in separate file??
    RegisterBase src_acc, dest_acc;

    // Used for File IO instructions
    int32_t src_imm;

    int      pipe; // Which pipeline this went down/is going down 
    uint32_t ghistory;
    uint32_t exCycles;
    bool     correct_path;
    bool     stall;
    bool     mispredict;
    bool     sb_get;      // Use to tell invalidate to free this instr's dest reg

    bool done_decode;
    bool done_regaccess;
    bool done_execute; // Set on completion of EX stage so it will not re execute on stall 
    bool bypass_s0;
    bool bypass_s1;
    bool bypass_s2;
    bool bypass_s3;
    bool bypass_sacc;

    bool _doneMemAccess;
    bool _doneCCAccess;
    bool _regread_done;

    // Used for LDW so that we can unlock registers on a flush
    bool     _has_lock;
    uint32_t lock_reg;
    uint64_t InstrNumber;
    uint64_t spawn_cycle;
    uint64_t fetch_cycle;
    // Used for calculating memory latency.
    uint64_t first_memaccess_cycle, last_memaccess_cyle;
    // Used to determine if there are enough register ports to dual issue
    uint32_t num_read_ports;
    // XXX END MEMBER *DATA* XXX 

  //////////////////////////////////////////////////////////////////////////////
  // public data members
  //////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    // FIXME For VECTOR Ops - REMOVE ME? 
    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    // Added for vector ops.  These are used to set the register numbers
    // (addresses) for the vector regists in the auto-generated scoreboard_read
    // call.
    public: 
    
    void set_regs_vec_s(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3) 
    { // Source 1 (s)
      src_reg0.addr = s0; src_reg1.addr = s1;
      src_reg2.addr = s2; src_reg3.addr = s3;
      src_imm = _get_imm();
    }
    void set_regs_vec_t(uint32_t s4, uint32_t s5, uint32_t s6, uint32_t s7) 
    { // Source 2 (t)
      src_reg4.addr = s4; src_reg5.addr = s5;
      src_reg6.addr = s6; src_reg7.addr = s7;
    }
    void set_regs_vec_d(uint32_t d, uint32_t d1, uint32_t d2, uint32_t d3) 
    { // Dest (d)
      dest_reg.addr = d; dest_reg1.addr = d1; 
      dest_reg2.addr = d2; dest_reg3.addr = d3; 
    }
    #if 0
    void set_regs_val(uint32_t s0,uint32_t s1,uint32_t s2, uint32_t s3,
          uint32_t d = 0, uint32_t D = 0, bool ud = false)
    {
      src_reg0.data.i32 = s0; src_reg1.data.i32 = s1; src_reg2.data.i32 = s2; src_reg3.data.i32 = s3;
      dest_reg.data.i32 = d; dest_reg_D.data.i32 = D; uses_Dest = ud;
    }
    #endif
    void set_regs_val(
      const RegisterBase &s0,
      const RegisterBase &s1,
      const RegisterBase &s2,
      const RegisterBase &s3,
      const RegisterBase &as)
    {
      src_reg0.IsFloat() ? src_reg0.data.f32 = s0.data.f32 : src_reg0.data.i32 = s0.data.i32;
      src_reg1.IsFloat() ? src_reg1.data.f32 = s1.data.f32 : src_reg1.data.i32 = s1.data.i32;
      src_reg2.IsFloat() ? src_reg2.data.f32 = s2.data.f32 : src_reg2.data.i32 = s2.data.i32;
      src_reg3.IsFloat() ? src_reg3.data.f32 = s3.data.f32 : src_reg3.data.i32 = s3.data.i32;
      src_acc.IsFloat() ? src_acc.data.f32 = as.data.f32 : src_acc.data.i32 = as.data.i32;
    }
    void set_regs_val(
      const RegisterBase &s0,
      const RegisterBase &s1,
      const RegisterBase &s2,
      const RegisterBase &s3,
      const RegisterBase &as,
      const RegisterBase &d)
    {
      set_regs_val(s0, s1, s2, s3, as);
      dest_reg.IsFloat() ? dest_reg.data.f32 = d.data.f32 : dest_reg.data.i32 = d.data.i32;
    }
    void set_regs_val_vec(
      const RegisterBase &s4,
      const RegisterBase &s5,
      const RegisterBase &s6,
      const RegisterBase &s7)
    {
      src_reg4.IsFloat() ? src_reg4.data.f32 = s4.data.f32 : src_reg4.data.i32 = s4.data.i32;
      src_reg5.IsFloat() ? src_reg5.data.f32 = s5.data.f32 : src_reg5.data.i32 = s5.data.i32;
      src_reg6.IsFloat() ? src_reg6.data.f32 = s6.data.f32 : src_reg6.data.i32 = s6.data.i32;
      src_reg7.IsFloat() ? src_reg7.data.f32 = s7.data.f32 : src_reg7.data.i32 = s7.data.i32;
    }
    void set_regs_val_vec(
      const RegisterBase &s4,
      const RegisterBase &s5,
      const RegisterBase &s6,
      const RegisterBase &s7,
      const RegisterBase &d1,
      const RegisterBase &d2,
      const RegisterBase &d3)
    {
      set_regs_val_vec(s4, s5, s6, s7);
      dest_reg1.IsFloat() ? dest_reg1.data.f32 = d1.data.f32 : dest_reg1.data.i32 = d1.data.i32;
      dest_reg2.IsFloat() ? dest_reg2.data.f32 = d2.data.f32 : dest_reg2.data.i32 = d2.data.i32;
      dest_reg3.IsFloat() ? dest_reg3.data.f32 = d3.data.f32 : dest_reg3.data.i32 = d3.data.i32;
    }

    // Added for vector ops
    RegisterBase get_sreg4() { return src_reg4; }
    RegisterBase get_sreg5() { return src_reg5; }
    RegisterBase get_sreg6() { return src_reg6; }
    RegisterBase get_sreg7() { return src_reg7; }
    RegisterBase get_dreg1() { return dest_reg1; }
    RegisterBase get_dreg2() { return dest_reg2; }
    RegisterBase get_dreg3() { return dest_reg3; }

    // START VECTOR ADDITIONS
    bool bypass_s4;
    bool bypass_s5;
    bool bypass_s6;
    bool bypass_s7;
    bool is_vec_op;
    RegisterBase result_reg1;   /* Register file WB */
    RegisterBase result_reg2;   /* Register file WB */
    RegisterBase result_reg3;   /* Register file WB */
    int vector_op_count;  // Set to vector length (4) at decode by
                          // set_is_vec_op().  Execute is
                          // stalled until it reaches zero.
    // END VECTOR ADDITIONS

    // Vector register additions.
    RegisterBase src_reg4, src_reg5, src_reg6, src_reg7, dest_reg1, dest_reg2, dest_reg3;

    // Added for vector ops.  Note that it expects the vector register number
    // not the scalar one (so take the scalar and divide by four.
    void set_result_RF_vec(uint32_t vec_reg, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
    { 
      result_reg  = RegisterBase(4*vec_reg+0, v0);
      result_reg1 = RegisterBase(4*vec_reg+1, v1);
      result_reg2 = RegisterBase(4*vec_reg+2, v2);
      result_reg3 = RegisterBase(4*vec_reg+3, v3);
    }
    void set_result_RF_vec(uint32_t vec_reg, int32_t v0, int32_t v1, int32_t v2, int32_t v3)
    { 
      result_reg  = RegisterBase(4*vec_reg+0, *((uint32_t *)&v0));
      result_reg1 = RegisterBase(4*vec_reg+1, *((uint32_t *)&v1));
      result_reg2 = RegisterBase(4*vec_reg+2, *((uint32_t *)&v2));
      result_reg3 = RegisterBase(4*vec_reg+3, *((uint32_t *)&v3));
    }
    void set_result_RF_vec(uint32_t vec_reg, float v0, float v1, float v2, float v3)
    { 
      result_reg  = RegisterBase(4*vec_reg+0, v0);
      result_reg1 = RegisterBase(4*vec_reg+1, v1);
      result_reg2 = RegisterBase(4*vec_reg+2, v2);
      result_reg3 = RegisterBase(4*vec_reg+3, v3);
    }
    /* Used by LD/St and also jump/branch with link */
    void set_result_RF_vec(uint32_t vec_reg, uint32_t v0, uint32_t v1, uint32_t v2, 
      uint32_t v3, uint32_t ea)
    { 
      result_reg  = RegisterBase(4*vec_reg+0, v0);
      result_reg1 = RegisterBase(4*vec_reg+1, v1);
      result_reg2 = RegisterBase(4*vec_reg+2, v2);
      result_reg3 = RegisterBase(4*vec_reg+3, v3);
      result_ea = ea;
    }
    // Added for vector ops
    void set_bypass_sreg4() { bypass_s4 = true; }
    void set_bypass_sreg5() { bypass_s5 = true; }
    void set_bypass_sreg6() { bypass_s6 = true; }
    void set_bypass_sreg7() { bypass_s7 = true; }
    // Added for vector ops
    bool is_bypass_sreg4() { return bypass_s4; }
    bool is_bypass_sreg5() { return bypass_s5; }
    bool is_bypass_sreg6() { return bypass_s6; }
    bool is_bypass_sreg7() { return bypass_s7; }

    // Added for vector ops
    RegisterBase get_result_reg1()  { return result_reg1; }
    RegisterBase get_result_reg2()  { return result_reg2; }
    RegisterBase get_result_reg3()  { return result_reg3; }

    // Added for vector ops.  Set in auto-generated scoreboard
    bool get_is_vec_op() { return is_vec_op; }
    void set_is_vec_op() { 
      // Reset vector count
      vector_op_count = 4;
      is_vec_op = true; 
    }
    void clear_is_vec_op() { 
      vector_op_count = -1;
      is_vec_op = false; 
    }
    // Check to see whether all of the scalar operations that comprise one
    // vector operation have completed
    bool completed_vec_op() {
      if (vector_op_count <= 0) { return true; }
      else                      { return false; }
    }
    // Called from the execute stage each cycle that a vector operation is
    // attempted.
    void execute_vec_op() {
      vector_op_count--;
    }

    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    // END VECTOR OPS (REMOVE ME?)
    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
////////////
////////////

};
////////////////////////////
// End InstrLegacy
////////////////////////////

#endif
