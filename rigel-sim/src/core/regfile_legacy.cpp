////////////////////////////////////////////////////////////////////////////////
// reg_file.cpp
////////////////////////////////////////////////////////////////////////////////
//
// Implementation of register file and register code. Declarations live in
// reg_file.h.
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ~RegisterFileLegacy()
////////////////////////////////////////////////////////////////////////////////
// Regfile Destructor
#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for snprintf, NULL, sprintf
#include <fstream>                      // for operator<<, basic_ostream, etc
#include <iomanip>                      // for operator<<, setfill, setw, etc
#include <iostream>                     // for hex, dec, cerr
#include <string>                       // for char_traits
#include <vector>                       // for vector
#include "caches/hybrid_directory.h"  // for HybridDirectory, etc
#include "profile/profile.h"        // for ProfileStat
#include "core/regfile_legacy.h"        // for RegisterBase, RegisterFileLegacy, etc
#include "rigellib.h"       // for ::SPRF_HYBRIDCC_BASE_ADDR, etc
#include "sim.h"            // for NUM_SREGS, etc
#include "util/util.h"           // for ExitSim
#include "util/value_tracker.h"  // for ZeroTracker

RegisterFileLegacy::~RegisterFileLegacy() 
{
  if (reg_file) { 
    delete [] reg_file; 
    reg_file = NULL;
  } else { /* ERROR */ }
}

////////////////////////////////////////////////////////////////////////////////
// RegisterFileLegacy()
////////////////////////////////////////////////////////////////////////////////
// Constructor used for custom GPRF
// Parameters: int regs -# of Regs for RF
//             int width - width of each register
////////////////////////////////////////////////////////////////////////////////
RegisterFileLegacy::RegisterFileLegacy(unsigned int regs, unsigned int width) : 
  num_regs(regs), width(width) 
{
  num_free_ports = 4;     // FIXME: Make configurable the amount of read ports
  reg_file = new RegisterBase[regs];
  for (unsigned int i = 0; i < regs; i++) {
    reg_file[i].addr = i;
  }

///  //Write argc, argv values
///  //These will get overwritten if you want to INIT_REGISTER_FILE.
///  //NOTE: argv will still be in memory, but this should be OK, since it's at the top of
///  //the code segment and you shouldn't be reading (from software's point of view)
///  //uninitialized memory anyway.
///  set(rigel::TARGET_ARGS.size(), 4);
///  set(0x400000-(4*rigel::TARGET_ARGS.size()), 5);
///  now stored in SPRFs, and init in core_inorder_legacy constructor
  
  // Read in the register file from a file at the beginning of the simulation
  if(rigel::INIT_REGISTER_FILE){
     char in_file[200];
     uint32_t temp_val;
     //sprintf(in_file,"rf.%d.%d.in",coreid,tid);
     sprintf(in_file,"rf.in");  //FIXME: Use individual files for different rf????
		 std::ifstream rfload;
     rfload.open(in_file);
     for(unsigned int rr=0; rr<regs; rr++) {  // FIXME: hardcoded to 32 regs!
       // FIXME: we need to change the write methods so we don't have to
       // access the data array directly, see bug #146
       rfload >> std::hex >> temp_val;
       set(temp_val,rr); 
     }
     rfload.close();
  }
}

////////////////////////////////////////////////////////////////////////////////
// RegisterFileLegacy::writef
////////////////////////////////////////////////////////////////////////////////
// Writes floating pt data into a location into the GPRF. Also, if
// enabled through the DUMP_REGISTER_TRACE parameter, write a line to a register
// trace file consisting of the PC of the instruction initiating the write, the
// destination register, and the data being written to the register file
// Parameters: float val - value to store
//             uint32_t addr - location to store
//             uint32_t pc - PC of the instruction initiating the write
//             ostream &f - pointer to the register trace output file
////////////////////////////////////////////////////////////////////////////////
void 
RegisterFileLegacy::writef(float val, unsigned int addr, uint32_t pc, std::ostream &f) 
{ 
  RegisterBase rb( addr, val );
  write(rb, pc, f);
  return;
}

////////////////////////////////////////////////////////////////////////////////
// RegisterFileLegacy::write
////////////////////////////////////////////////////////////////////////////////
// Writes data (either float or int) into a location into the GPRF.  Also, if
// enabled through the DUMP_REGISTER_TRACE parameter, write a line to a register
// trace file consisting of the PC of the instruction initiating the write, the
// destination register, and the data being written to the register file
// Parameters: RegisterBase reg - value/location to store
//             uint32_t pc - PC of the instruction initiating the write
//             ostream &f - pointer to the register trace output file
// FIXME: merge with dead code below
////////////////////////////////////////////////////////////////////////////////
void 
RegisterFileLegacy::write(const RegisterBase reg, const uint32_t pc, std::ostream &f, bool skip_reg_zero ) {

  //assert((uint32_t)reg.addr < (uint32_t)rigel::NUM_REGS && "Invalid register number");
  // Proper checking of out of bounds writes
  //assert((uint32_t)reg.addr < (uint32_t)num_regs && "RegisterFileLegacy::write Invalid register number");

  if (reg.addr == simconst::NULL_REG) {
    throw ExitSim((char *)"RegisterFileLegacy->write(): Trying to write NULL_REG", 1);
  }

  if (reg.addr >= static_cast<uint32_t>(num_regs)) {
		std::cerr << "RegFile:write() val: " << reg.data.i32 << " addr: " << reg.addr << "\n";
    throw ExitSim((char *)"RegisterFileLegacy->write(): Trying to write register beyond num_regs", 1);
  }

  if(reg.addr == 0 && skip_reg_zero) {
    return;
  }

  if(rigel::TRACK_BIT_PATTERNS) {
    zt.addWrite(reg_file[reg.addr].data.i32, reg.data.i32);
		GLOBAL_ZERO_TRACKER_PTR->addWrite(reg_file[reg.addr].data.i32, reg.data.i32);
  }

  // FIXME use accessor...
  if (reg.FlagFloat) {
    reg_file[reg.addr].data.f32 = reg.data.f32;
  } else {
    reg_file[reg.addr].data.u32 = reg.data.u32;
  }

  // print RF Trace
  if(rigel::DUMP_REGISTER_TRACE){
    f.setf(std::ios::right);
    //f << std::dec << std::setfill('0') << std::setw(12) << rigel::CURR_CYCLE << " ";
    f << std::setfill('0') << std::setw(8) << std::hex << pc << " ";
    f << std::dec << reg.addr << " ";
    f << std::setfill('0') << std::setw(8) << std::hex << reg.data.u32;
    f << "\n";
  }

}

////////////////////////////////////////////////////////////////////////////////
// RegisterFileLegacy::write
////////////////////////////////////////////////////////////////////////////////
// Writes data into a location into the GPRF. Also handles special case when 
// GPRF_SLEEP is written to. If DUMP_REGISTER_TRACE is enabled, the function
// writes the PC of the instruction initiating the write, as well as the
// register location, and value being written into a register trace file.
// Parameters: uint32_t val - value to store
//             uint32_t addr - location to store
//             uint32_t pc - PC of the instruction initiating the write
//             ostream &f - pointer to the register trace output file
////////////////////////////////////////////////////////////////////////////////
void 
RegisterFileLegacy::write(uint32_t val, unsigned int addr, uint32_t pc, std::ostream &f) { 
  RegisterBase rb( addr, val );
  write( rb, pc, f);
  return;
}

////////////////////////////////////////////////////////////////////////////////
// RegisterFileLegacy:read
////////////////////////////////////////////////////////////////////////////////
// Reads data from the GPRF. Returns register with resultant data
// Parameters: addr - location in GPRF to read from
RegisterBase
RegisterFileLegacy::read(unsigned int addr, bool skip_reg_zero) { 

  if (addr == simconst::NULL_REG) {
    throw ExitSim((char *)"RegisterFileLegacy->read(): Trying to read NULL_REG", 1);
  }

  if (addr >= static_cast<uint32_t>(rigel::NUM_REGS)) {
    throw ExitSim((char *)"RegisterFileLegacy->read(): Trying to read register beyond NUM_REGS", 1);
  }

  if (addr == 0 && skip_reg_zero ) return RegisterBase(0, 0);

  if(rigel::TRACK_BIT_PATTERNS) {
    zt.addRead(reg_file[addr].data.i32);
    GLOBAL_ZERO_TRACKER_PTR->addRead(reg_file[addr].data.i32);
	}

  return reg_file[addr];
}

////////////////////////////////////////////////////////////////////////////////
// set()
////////////////////////////////////////////////////////////////////////////////
// Set the contents of a register outside of standard program execution (i.e.
// when reading in a saved regfile state from a file).  This function is
// necessary to prevent these writes from appearing in a trace file.
// Parameters: uint32_t val - the value being written
//             uint32_t addr - the register number that is to be written to
void
RegisterFileLegacy::set(uint32_t val, unsigned int addr){
  reg_file[addr].data.u32=val;
  reg_file[addr].SetInt();
}
////////////////////////////////////////////////////////////////////////////////i
// dump_reg_file()
////////////////////////////////////////////////////////////////////////////////
// dump register file contents to the stream specified
// Parameters: ostream f - the io stream to dump to
void 
RegisterFileLegacy::dump_reg_file(std::ostream &f) {
  for (unsigned int i = 0; i < num_regs ; i++) {
    f.setf(std::ios::right);
    if (rigel::CMDLINE_INTERACTIVE_MODE) {// readability for interactive mode
      f << "r" << std::dec << i << " " ;
    }
    f << std::setfill('0') << std::setw(8) << std::hex << reg_file[i].data.u32;
    f << "\n";
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
RegisterFileLegacy &
RegisterFileLegacy::operator=(const RegisterFileLegacy &rf) {
  this->num_regs = rf.num_regs;
  this->width = rf.width;

  for (unsigned int i = 0; i < rf.num_regs; i++) {
    this->reg_file[i] = rf.reg_file[i];
  }

  return *this;
}



////////////////////////////////////////////////////////////////////////////////
// SpRegisterFileLegacy()
////////////////////////////////////////////////////////////////////////////////
// Constructor used for custom SPRF
// Parameters: int regs -# of Regs for RF
//             int width - width of each register
SpRegisterFileLegacy::SpRegisterFileLegacy(unsigned int regs, unsigned int width) {
  reg_file = new RegisterBase[regs];
  this->num_regs = regs;
  this->width = width;
  this->curr_pc = 0;
  this->next_pc = 0;
}

////////////////////////////////////////////////////////////////////////////////
// ~SpRegisterFileLegacy()
////////////////////////////////////////////////////////////////////////////////
// SPRF Destructor
SpRegisterFileLegacy::~SpRegisterFileLegacy() {
  if (reg_file) { 
    delete [] reg_file; 
    reg_file = NULL; 
  } else { /* ERROR */ }
}

////////////////////////////////////////////////////////////////////////////////
// SpRegisterFileLegacy::operator=(const SpRegisterFileLegacy &rf)
////////////////////////////////////////////////////////////////////////////////
// SPRF Custom Copy Constructor
SpRegisterFileLegacy &
SpRegisterFileLegacy::operator=(const SpRegisterFileLegacy &rf) {
  RegisterFileLegacy::operator=(rf);
  
  this->curr_pc = rf.curr_pc;
  this->next_pc = rf.next_pc;

  return *this;
}

bool rigel::SPRF_LOCK_CORES = true;

////////////////////////////////////////////////////////////////////////////////
// SpRegisterFileLegacy::write
////////////////////////////////////////////////////////////////////////////////
// Writes data into a location into the SPRF. Also handles special case when 
// SPRF_SLEEP is written to
// Parameters: uint32_t val - value to store
//             uint32_t addr - location to store
void
SpRegisterFileLegacy::write(uint32_t val, unsigned int addr) {
  //Check if the register requested is in the SPRF
  if (addr >= rigel::NUM_SREGS) {
    char buf[1024];
    snprintf(buf, 1024, "RegisterFileLegacy->write(): Trying to write register (%u) beyond NUM_SREGS (%u)\n",
      addr, rigel::NUM_SREGS);
    throw ExitSim(buf, 1);
  }

  // Lock all cores when SPRF_SLEEP is written with non-zero data.  
  // Unlock if cleared.
  if (addr == SPRF_SLEEP) {
    if (0 == val) {
      rigel::SPRF_LOCK_CORES = false;
      ProfileStat::activate();
    } else {
      rigel::SPRF_LOCK_CORES = true;
      ProfileStat::deactivate();
    }
  }
  // Set the base address for the fine-grain region table.
  if (addr == SPRF_HYBRIDCC_BASE_ADDR) {
    hybrid_directory.set_fine_region_table_base(val);
  }
  if (SPRF_STACK_BASE_ADDR == addr) {
    if (rigel::STACK_MIN_BASEADDR > val) {
      rigel::STACK_MIN_BASEADDR = val;
    }
  }
  reg_file[addr].data.u32 = val;
}

////////////////////////////////////////////////////////////////////////////////
// SpRegisterFileLegacy::read
////////////////////////////////////////////////////////////////////////////////
// Reads data from the SPRF. Returns register with resultant data
// NOTE: For things like timers and cycle counters, it doesn't make sense for
//       the simulator to store the value every cycle when the values are very
//       seldom read.  Instead, we hook this function to produce the values on
//       demand, if possible.  The initial customer for this functionality is
//       the current cycle counter.
// Parameters: addr - location in SPRF to read from
RegisterBase
SpRegisterFileLegacy::read(unsigned int addr) {

  //Check if the register requested is in the SPRF
  if (addr >= rigel::NUM_SREGS) {
    char buf[1024];
    snprintf(buf, 1024, "RegisterFileLegacy->read(): Trying to write register (%u) beyond NUM_SREGS (%u)\n",
      addr, rigel::NUM_SREGS);
    throw ExitSim(buf, 1);
  }

	switch(addr)
	{
		case SPRF_CURR_CYCLE_LOW:
			return RegisterBase(addr, (uint32_t)(rigel::CURR_CYCLE & 0xFFFFFFFFU));
		case SPRF_CURR_CYCLE_HIGH:
			return RegisterBase(addr, (uint32_t)((rigel::CURR_CYCLE >> 32) & 0xFFFFFFFFU));
		default:
			return reg_file[addr];
	}
}  

////////////////////////////////////////////////////////////////////////////////
// SpRegisterFileLegacy::dump_reg_file
////////////////////////////////////////////////////////////////////////////////
void 
SpRegisterFileLegacy::dump_reg_file(std::ostream &f) {
  /* TODO */
}

////////////////////////////////////////////////////////////////////////////////
// SpRegisterFileLegacy::write
////////////////////////////////////////////////////////////////////////////////
// Stores data into the SPRF. Returns nothing.
// Parameters: reg - register which holds the data and location to store
void 
SpRegisterFileLegacy::write(const RegisterBase reg) {
  //Check if the register requested is in the SPRF
  if (reg.addr >= rigel::NUM_SREGS) {
    char buf[1024];
    snprintf(buf, 1024, "RegisterFileLegacy->write(): Trying to write register (%u) beyond NUM_SREGS (%u)\n",
      reg.addr, rigel::NUM_SREGS);
    throw ExitSim(buf, 1);
  }
  // Lock all cores when SPRF_SLEEP is written with non-zero data.  
  // Unlock if cleared.
  if (reg.addr == SPRF_SLEEP) {
    if (reg.data.i32 == 0) {
      rigel::SPRF_LOCK_CORES = false;
      ProfileStat::activate();
    } else  {
      rigel::SPRF_LOCK_CORES = true;
      ProfileStat::deactivate();
    }
  }

  // Set the base address for the fine-grain region table.
  if (reg.addr == SPRF_HYBRIDCC_BASE_ADDR) {
    hybrid_directory.set_fine_region_table_base(reg.data.u32);
  }
  if (SPRF_STACK_BASE_ADDR == reg.addr) {
    if (rigel::STACK_MIN_BASEADDR > reg.data.u32) {
      rigel::STACK_MIN_BASEADDR = reg.data.u32;
    }
  }

  assert(reg.addr < rigel::NUM_REGS && "Invalid register number");
  reg_file[reg.addr] = reg;
}

