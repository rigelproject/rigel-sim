
////////////////////////////////////////////////////////////////////////////////
// scoreboard.h (ScoreBoard)
////////////////////////////////////////////////////////////////////////////////
// ScoreBoard Class
// more info here
// yummy tasty cookies...mmm...
////////////////////////////////////////////////////////////////////////////////

#ifndef _SCOREBOARD_H_
#define _SCOREBOARD_H_

#include "sim.h"
#include "core/regfile_legacy.h"

////////////////////////////////////////////////////////////////////////////////
// type ScoreBoardEntry
////////////////////////////////////////////////////////////////////////////////
typedef struct ScoreBoardEntry {
  ScoreBoardEntry() : 
    ready_cycle(0), reg_num(0), use_count(0), bypass_value(0xbbbbdead),
    bypass_count(0), locked(false)
  {
    //  NADA
  }
  uint64_t ready_cycle;
  int reg_num;
  int use_count;
  // ACC FIXME: Should this be a union so that we can bypass non integer data?
  uint32_t bypass_value;
  int32_t bypass_count;
  bool locked;
} ScoreBoardEntry;

////////////////////////////////////////////////////////////////////////////////
// Class: ScoreBoard
////////////////////////////////////////////////////////////////////////////////
// Base class, is this used in practice for anything?
////////////////////////////////////////////////////////////////////////////////
class ScoreBoard {

  /////////////////////////////////////////////////////////////////////////////
  // public data members
  /////////////////////////////////////////////////////////////////////////////
  public:
    // Bit is set if the register is available to read, it is cleared if
    // something is going to write it down the pipe 
    ScoreBoardEntry *scoreboard;
    uint32_t num_regs;
    bool _wait_for_clear;

  /////////////////////////////////////////////////////////////////////////////
  // public methods/accessors
  /////////////////////////////////////////////////////////////////////////////
  public:

    // constructors
    ScoreBoard() { };
    ScoreBoard(uint32_t _num_regs);

    // destructor
    virtual ~ScoreBoard() { delete [] scoreboard;  }

    // undefined
    ScoreBoard & operator=(ScoreBoard &sc);
 
    // Dump the values in the scoreboard for debugging purposes
    void dump();

    virtual void UpgradeMemAccess(uint32_t reg) {}

    // Used for checking availability of registers this cycle.  Will need to
    // alter later to account for feedback paths
    virtual bool check_reg(uint32_t reg0, bool skip_reg_zero = true) {
      if (reg0 == 0 && skip_reg_zero) return true;
      return (scoreboard[reg0].use_count == 0);
    }

    virtual bool check_reg(uint32_t reg0, uint32_t reg1) {
      return ((scoreboard[reg0].use_count == 0) && (scoreboard[reg1].use_count == 0));
    }

    // Used to stall access to a register for an indeterminate amount of time.
    // After a lockReg() call, no reads will get through until unlockReg() is
    // called.  NOTE: Cannot lock special purpose regs
    virtual void lockReg(uint32_t reg, bool skip_reg_zero = true) {}
    virtual void unlockReg(uint32_t reg, bool skip_reg_zero = true) {}
    virtual bool isLocked(uint32_t reg, bool skip_reg_zero = true) { return false; }

    // Set when register is assigned for writing, but will not be written for
    // multiple cycles
    // Allows an option to use register 0 normally
    virtual void get_reg(uint32_t reg0, instr_t type, bool skip_reg_zero = true) {
      if (reg0 == 0 && skip_reg_zero) return;
      scoreboard[reg0].use_count++;
    }

    // Done with register 
    // This function is used during writeback to indicate that reg0 has been
    // written back to the reg_file. skip_reg_zero allows for normal use of 
    // reg 0 for accumulator file
    virtual void put_reg(uint32_t reg0, instr_t type = I_NULL, bool skip_reg_zero = true) {
      if (reg0 == 0 && skip_reg_zero) return;
      assert(scoreboard[reg0].use_count > 0);
      scoreboard[reg0].use_count--;
    }
    virtual void put_reg(RegisterBase reg, instr_t type = I_NULL, bool skip_reg_zero = true) {
      if (reg.addr == 0 && skip_reg_zero) return;
      assert(scoreboard[reg.addr].use_count > 0);
      scoreboard[reg.addr].use_count--;
    }
    // Done with register completely 
    virtual void clear_reg(uint32_t reg0,bool skip_reg_zero = true) {
      if (reg0 == 0) return;
      scoreboard[reg0].use_count = 0;
      scoreboard[reg0].locked = false;
    }

    virtual void clear() {
      for (uint32_t i = 0; i < this->num_regs; i++) {
        clear_reg(i);
      }
    }

    // Send the value to waiting instructions.  Must be called within the time
    // set in sim.h
    virtual void bypass_reg(uint32_t reg, uint32_t val, bool skip_reg_zero = true) { }

    // Get the value from the bypass path.
    virtual uint32_t get_bypass(uint32_t reg, bool skip_reg_zero = true) { return 0; }

    // Check if the value is on the bypass path.  If not, get it from the
    // register file directly
    virtual bool is_bypass(uint32_t reg) { return false; }

    // If full pipeline flushing is used on mispredicts, wait_for_clear will not
    // allow any scoreboarding until all previous scoreboard entries are
    // completed.
    virtual void wait_for_clear() {
      _wait_for_clear = true;
    }


};

////////////////////////////////////////////////////////////////////////////////
// Class ScoreBoardBypass
////////////////////////////////////////////////////////////////////////////////
// Derived class of ScoreBoard
// Version of ScoreBoard that allows results from EX to be used on the next
// cycle without waiting on WB
////////////////////////////////////////////////////////////////////////////////
class ScoreBoardBypass : public ScoreBoard {

  //////////////////////////////////////////////////////////////////////////////
  public:
  //////////////////////////////////////////////////////////////////////////////
 
    // constructor 
    ScoreBoardBypass(uint32_t num_regs);

    // no destructor ( scoreboard cleaned up in superclass destructor)
    //  ~ScoreBoardBypass();
    //  ScoreBoardBypass & operator=(ScoreBoardBypass &sc);
    
    virtual bool check_reg(uint32_t reg, bool skip_reg_zero = true) {
      //if (reg == 0 && skip_reg_zero) { printf("check_reg(%d) returning true\n", reg); return true; }
      // waits for whole pipeline to flush before dispatching instructions again
      if (rigel::core::FORCE_SB_FLUSH) {
        //printf("Waiting for SB Flush\n");
        if (this->_wait_for_clear) {
          //printf("Waiting for Clear\n");
          // If all are clear, we can clear flag, otherwise check fails
          for (uint32_t i = 0; i < this->num_regs; i++) {
            if (scoreboard[i].use_count != 0) {
              //fprintf(stderr, "Reg %d has use count %d, failing\n", i, scoreboard[i].use_count);
              return false;
            }
          }
          // All regs are finally cleared, pipeline has drained
          _wait_for_clear = false;
          this->clear();
        }
      }

      //cerr << "check_reg:  checking availability" << endl;
      uint64_t avail = scoreboard[reg].ready_cycle;
      bool locked = scoreboard[reg].locked;
      //if(!(rigel::CURR_CYCLE >= avail && !locked))
      // printf("CurrCycle %"PRIu64", Avail %"PRIu64", locked %01d\n", rigel::CURR_CYCLE, avail, locked);
      //printf("check_reg(%d) returning %s\n", reg, ((rigel::CURR_CYCLE >= avail && !locked)) ? "true" : "false");
      return (rigel::CURR_CYCLE >= avail && !locked);
    }

    virtual bool check_reg(uint32_t reg0, uint32_t reg1) {
      return check_reg(reg0) && check_reg(reg1);
    }

    // Called when a memory value is returned in MEM or CC. It turns a locked
    // reg into a normal bypassed reg
    virtual void UpgradeMemAccess(uint32_t reg) {
      if (reg == 0) return;
      //If we have full bypass, unlock now, otherwise wait until WB.
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_full) {
        unlockReg(reg);
      }
      scoreboard[reg].ready_cycle = rigel::CURR_CYCLE;
    }

    // Locking allows for register to be unavailable for variable amount of time
    // and will not be bypassed.  Useful for IO operations and other operations
    // that are synthetic, but want to prevent hazards
    virtual void lockReg(uint32_t reg, bool skip_reg_zero = true) {
      if (reg == 0 && skip_reg_zero) return;
      assert(!scoreboard[reg].locked 
        && "Trying to lock a reg that is already locked");
      scoreboard[reg].locked = true;
    }

    virtual bool isLocked(uint32_t reg,bool skip_reg_zero = true) {
      if (reg == 0 && skip_reg_zero) return false;
      return scoreboard[reg].locked;
    }

    virtual void unlockReg(uint32_t reg,bool skip_reg_zero = true) {
      if (reg == 0 && skip_reg_zero) return;
      //FIXME there are some mystery double-unlocks with no bypassing.
      //TODO investigate these.
      if(rigel::PIPELINE_BYPASS_PATHS == rigel::pipeline_bypass_full) {
        assert(scoreboard[reg].locked
          && "Trying to unlock a reg that is not locked");
      }
      scoreboard[reg].locked = false;
    }

    // Lock a register for writing.  Implemented in scoreboard.cpp
    virtual void get_reg(uint32_t reg, instr_t type, bool skip_reg_zero = true);
    
    virtual void put_reg(RegisterBase reg, instr_t type = I_NULL, bool skip_reg_zero = true) {
      if (reg.addr == 0 && skip_reg_zero) return;
      assert(scoreboard[reg.addr].use_count > 0);
      scoreboard[reg.addr].use_count--;
      // Saturates at zero --> go to regfile.
      if (scoreboard[reg.addr].bypass_count > 0) scoreboard[reg.addr].bypass_count--;
    }

    // Same as above function except used for accumulator since it allows the
    // option of using register zero
    virtual void put_reg(uint32_t reg, instr_t type = I_NULL, bool skip_reg_zero = true) {
      if (reg == 0 && skip_reg_zero) return;
      assert(scoreboard[reg].use_count > 0);
      scoreboard[reg].use_count--;
      // Saturates at zero --> go to regfile.
      if (scoreboard[reg].bypass_count > 0) scoreboard[reg].bypass_count--;
    }

    virtual void clear_reg(uint32_t reg, bool skip_reg_zero = true) {
      if (reg == 0 && skip_reg_zero) return;
      scoreboard[reg].ready_cycle = 0;
      scoreboard[reg].bypass_count = 0;
      scoreboard[reg].locked = false;
    }

    // Member functions for handling bypass logic.  EX needs to set these bits
    // and WB needs to clear them.  Bypassing is also part of uarch state that
    // needs to be saved in checkpoints.
    // Allows bypassing register 0.  This is needed
    // for the accumulator file which uses accumulator 0.
    void bypass_reg(uint32_t reg, uint32_t val, bool skip_reg_zero = true) {
      if (reg == 0 && skip_reg_zero) return;
      scoreboard[reg].bypass_value = val;
    }

    // Functions defined in scoreboard.cpp
    uint32_t get_bypass(uint32_t reg, bool skip_reg_zero = true); 
    bool is_bypass(uint32_t reg) {
      switch(rigel::PIPELINE_BYPASS_PATHS) {
        case rigel::pipeline_bypass_full: return (scoreboard[reg].bypass_count > 0);
        case rigel::pipeline_bypass_none: return false;
        default: assert(0 && "Illegal PIPELINE_BYPASS_PATHS value!"); return false;
      }
    }
};

#endif

