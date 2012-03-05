#ifndef __REGFILE_LEGACY_H__
#define __REGFILE_LEGACY_H__
////////////////////////////////////////////////////////////////////////////////
// reg_file.h (register file classes)
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the definition a register used in the core, as well as
//  both the general purpose and special purpose register files (which use the
//  register) 
//
////////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <fstream>
#include <ostream>
#include "util/value_tracker.h"
#include "rigelsim.pb.h"
#include <google/protobuf/repeated_field.h>
#include "typedefs.h"

typedef ::google::protobuf::RepeatedField< RIGEL_PROTOBUF_WORD_T > RepeatedWord;

////////////////////////////////////////////////////////////////////////////////
// Class: RegisterBase
////////////////////////////////////////////////////////////////////////////////
// Description -
// This class is used to store a single register worth of information
// (usually in a register file) for the core
////////////////////////////////////////////////////////////////////////////////
typedef class RegisterBase {
  //public member functions
  public:
    RegisterBase() : addr(0xdeadbeef), FlagFloat(false) { 
      data.u32 = 0; 
    }
    RegisterBase(unsigned int inaddr, uint32_t val) : addr(inaddr), FlagFloat(false) { 
      data.u32 = val; 
    }
		RegisterBase(unsigned int inaddr, int32_t val) : addr(inaddr), FlagFloat(false) {
			data.i32 = val;
		}
    // FIXME: bug in FlagFloat init?
    RegisterBase(unsigned int inaddr, float val) : addr(inaddr), FlagFloat(false) { 
      data.f32 = val; 
    }

    // Flag float tracks if this was last written as a float
    void SetFloat() { FlagFloat = true; }
    void SetInt() { FlagFloat = false; }

    bool IsFloat() const { return FlagFloat; }
    bool IsInt() const { return !FlagFloat; }

  //public data members
  public:
    unsigned int addr;  // Register Number
    union {         // Register data
      float f32;
      int32_t i32;
			uint32_t u32;
    } data;
    bool FlagFloat;

} RegisterBase;
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Class: RegisterFileLegacy
////////////////////////////////////////////////////////////////////////////////
// Description -
// This class is used currently as the register file for the core
////////////////////////////////////////////////////////////////////////////////
class RegisterFileLegacy {
  friend class ClusterCheckpoint;
  friend class SpRegisterFileLegacy;

  public:
    RegisterFileLegacy() { reg_file = NULL; };
    ~RegisterFileLegacy();
    RegisterFileLegacy(unsigned int regs, unsigned int width = 32);

    // Copy constructor not implemented.
    RegisterFileLegacy(RegisterFileLegacy &rf);

    RegisterFileLegacy& operator=(const RegisterFileLegacy &rf);
    
    // Method for setting a value of registers outside of execution (i.e. when
    // reading in a saved regfile state) 
    void set(uint32_t val, uint32_t addr);

    // Remove the val/addr versions of write and just use RegisterBase types
    void write(  uint32_t val, unsigned int addr, uint32_t pc, std::ostream &f);
    void writef( float val,    unsigned int addr, uint32_t pc, std::ostream &f);

    void write(  const RegisterBase reg, uint32_t pc, std::ostream &f, bool skip_reg_zero = true);

    RegisterBase read(unsigned int addr, bool skip_reg_zero=true);
    unsigned int get_num_regs() const {
      return num_regs;
    }

    //FIXME NCC Duplicate dump functions
    void dump_reg_file(std::ostream &f);

    // Dump state of register file to STDERR
    void dump() {
      fprintf(stderr, "----------REGISTER FILE STATE--------\n");
      for (unsigned int i = 0; i < num_regs; i++) {
        fprintf(stderr, "[%2d] 0x%08x\n", i, reg_file[i].data.u32);
      }
    }

    // Used for making sure that the regfile is only accessed using a specific
    // number of ports;
    void request_read_ports(uint32_t num_ports){ 
      num_free_ports -= num_ports; 
    }
    uint32_t get_num_free_read_ports() { 
      return num_free_ports;
    }
    void free_read_ports(uint32_t num_ports){ 
      num_free_ports += num_ports; 
    }

    virtual void save_to_protobuf(RepeatedWord *ru) {
      for(unsigned int i = 0; i < num_regs; i++)
        ru->Add(read(i).data.u32);
    }

    virtual void restore_from_protobuf(RepeatedWord *ru) {
      assert(ru->size() == (int)num_regs && "Protobuf has different # of regs than RF object");
      for(unsigned int i = 0; i < num_regs; i++)
        set(ru->Get(i), i);
    }

    // Old
    //uint32_t &operator[](int i) { return reg_file[i]; }
    //uint32_t operator[](int i) const { return reg_file[i]; }
    ZeroTracker zt;

  protected:
    RegisterBase *reg_file;

  private:
    unsigned int num_regs;
    int width;
    int num_free_ports;
};

////////////////////////////////////////////////////////////////////////////////
// Class: SpRegisterFileLegacy
////////////////////////////////////////////////////////////////////////////////
// Description -
// This RF stores special values (core/cluster#, cycle#, etc.)
// FIXME Why is this a separate class?
////////////////////////////////////////////////////////////////////////////////
class SpRegisterFileLegacy : public RegisterFileLegacy {
  public:
    ~SpRegisterFileLegacy();
    SpRegisterFileLegacy() { reg_file = NULL; };
    SpRegisterFileLegacy(unsigned int regs, unsigned int width = 32);


    SpRegisterFileLegacy &operator=(const SpRegisterFileLegacy &rf);

    void write(uint32_t val, unsigned int addr);
    void write(const RegisterBase reg);
    RegisterBase read(unsigned int addr);

    void dump_reg_file(std::ostream &f);

    inline void set_curr_pc(uint32_t pc) { curr_pc = pc; }
    inline void set_next_pc(uint32_t pc) { next_pc = pc; }
    inline uint32_t get_curr_pc() { return curr_pc; }
    inline uint32_t get_next_pc() { return next_pc; }

  private:
    uint32_t curr_pc;
    uint32_t next_pc;
};

#endif
