#ifndef __INSTR_BASE_H__
#define __INSTR_BASE_H__

#include "config.h"
#include <inttypes.h>

class InstrBase {

  /// public methods
  public:

    /// constructors
   
    /// default constructor 
    InstrBase( 
      uint32_t pc, ///
      uint32_t raw ///
    );
    
		virtual ~InstrBase() { }

    /// accessors
    uint32_t pc()       { return pc_; }
    uint32_t raw_bits() { return raw_bits_; }

    /// get instruction fields
    /// TODO: auto-generate these routines
    // note: these are fragile (hardcoded), 
    // if the ISA changes they may have to be changed
    // Each of these obtains the values from the raw instruction bits
    uint32_t get_opcode() { return 0xF    & (raw_bits_ >> 28);}
    uint32_t get_reg_d()  { return 0x1F   & (raw_bits_ >> 23);}
    uint32_t get_reg_t()  { return 0x1F   & (raw_bits_ >> 18);}
    uint32_t get_reg_s()  { return 0x1F   & (raw_bits_ >> 13);}
    uint32_t get_imm5()   { return 0x1F   & (raw_bits_ >> 13);}

    /// decode the instruction fields into whatever internal subfields are
    /// desired
    virtual void decode() = 0;


  /// private member data
  private:

    uint32_t pc_;        /// program counter associated with this instruction

    uint32_t raw_bits_; /// raw undecoded instruction word

};


// delete me below here....

#include <cstdarg>                      // for va_start, va_end
#include <cstddef>                      // for ptrdiff_t
#include <iostream>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <dis-asm.h>                    // for disassemble_info, etc

typedef struct string_descriptor {
  char *buf;
  char *ptr;
  char *end;

  string_descriptor(int bufsize) {
    buf = new char[bufsize];
    ptr = buf;
    end = buf+bufsize-1;
  }
  ~string_descriptor() {
    delete[] buf;
  }
  size_t length() const {
    return (size_t)(ptr - buf);
  }
} string_descriptor;

#ifdef ENABLE_GPLV3
static int string_descriptor_snprintf(string_descriptor *str, char *format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = vsnprintf(str->ptr, (str->end - str->ptr), format, args);
  str->ptr += ret;
  va_end(args);
  return ret;
};


//////////////////////////////////////////////////////////////////////////
// Below are hooks needed for libfd to print out disassembled instructions
//////////////////////////////////////////////////////////////////////////
extern "C" {
  typedef struct dis_private_data {
    uint32_t raw_instr;
    uint32_t target_addr;
  } dis_priv_t;

  static int 
  dis_read_mem(bfd_vma memaddr, bfd_byte *myaddr, 
    unsigned int len, struct disassemble_info *info) 
  { 
    dis_priv_t *d = (dis_priv_t *)info->private_data;
    *((uint32_t *)myaddr) = d->raw_instr;
    return 0; 
  }

  static void 
  dis_print_addrfunt(bfd_vma addr, struct disassemble_info *info) {
  //  dis_priv_t *d = (dis_priv_t *)info->private_data;
    (*(info->fprintf_func))(info->stream, "0x%04x", addr);
  }
}
#endif //#ifdef ENABLE_GPLV3


#endif
