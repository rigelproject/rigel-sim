
////////////////////////////////////////////////////////////////////////////////
// ScoreBoard (scoreboard.cpp)
////////////////////////////////////////////////////////////////////////////////
// scoreboard class implementations
//
// contains two classes:
//   A base ScoreBoard class (is this used ever directly?!)
//   A derived ScoreBoardBypass which holds the bypass network values
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu32, PRIu64, PRIx32
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, stderr
#include "core/scoreboard.h"     // for ScoreBoard, etc
#include "sim.h"            // for CURR_CYCLE, etc

////////////////////////////////////////////////////////////////////////////////
// Scoreboard Constructor
////////////////////////////////////////////////////////////////////////////////
ScoreBoard::ScoreBoard(uint32_t _num_regs) : 
  num_regs(_num_regs) 
{ 
  _wait_for_clear = false;
  scoreboard = new ScoreBoardEntry[num_regs];
  for (uint32_t i = 0; i < num_regs; i++) {
    clear_reg(i);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Class: ScoreBoardBypass
////////////////////////////////////////////////////////////////////////////////
// Derived class of ScoreBoard
// Version of ScoreBoard that allows results from EX to be used on the next
// cycle without waiting on WB
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//  ScoreBoardBypass Constructor
////////////////////////////////////////////////////////////////////////////////
ScoreBoardBypass::ScoreBoardBypass(uint32_t num_regs) : ScoreBoard(num_regs) { }

////////////////////////////////////////////////////////////////////////////////
//  ScoreBoardBypass::get_reg()
////////////////////////////////////////////////////////////////////////////////
// Set when register is assigned for writing, but will not be written for
// multiple cycles.  skip_reg_zero allows register zero to be optionally used as
// a standard register (if skip_reg_zero = false). This is needed for the 
// accumulator file since it uses accumulator register zero for normal operation.
void 
ScoreBoardBypass::get_reg(uint32_t reg, instr_t type, bool skip_reg_zero)
{
  if (reg == 0 && skip_reg_zero) return;
  // If we are using single-cycle ALUs, we ignore the latency timings.
  scoreboard[reg].ready_cycle = rigel::SINGLE_CYCLE_ALUS ?
    rigel::CURR_CYCLE : rigel::CURR_CYCLE + INSTR_LAT[type];
  scoreboard[reg].use_count++;
  scoreboard[reg].bypass_count++;
}
///////////////////////////////////////////////////////////////////////////////
// ScoreBoardBypass::get_bypass()
////////////////////////////////////////////////////////////////////////////////
// returns the current bypass value for the provided reg with the option to use
// register zero as a normal register (for use in accumulator)
////////////////////////////////////////////////////////////////////////////////
uint32_t ScoreBoardBypass::get_bypass(uint32_t reg, bool skip_reg_zero) 
{

  #ifdef TRACE
    cerr << " [[BYPASSING r: " << dec << reg << " v: 0x" << 
      HEX_COUT << scoreboard[reg].bypass_value << "]] " << endl;
  #endif
  if(reg >= num_regs)
  {
    fprintf(stderr, "Error: called get_bypass() requesting reg %"PRIu32", we only have %"PRIu32"\n",
            reg, num_regs);
    assert(0 && "ScoreBoardBypass::get_bypass reg is out of bounds!");
  }
  if (reg == 0 && skip_reg_zero) {
    return 0;
  } else {
    return scoreboard[reg].bypass_value;
  }

}

////////////////////////////////////////////////////////////////////////////////
// ScoreBoard::dump()
////////////////////////////////////////////////////////////////////////////////
// prints scoreboard state to STDERR
////////////////////////////////////////////////////////////////////////////////
void ScoreBoard::dump() {
  fprintf(stderr, "----------SCOREBOARD STATE--------\n");
  for (uint32_t i = 0; i < num_regs; i++) {
    fprintf(stderr, 
      "[%2d] READY @ Cycle %8"PRIu64" LOCKED: %01d BYPASS COUNT: %2d "
      "USE COUNT: %2d BYPASS VALUE: 0x%08"PRIx32"\n",
      i, scoreboard[i].ready_cycle, 
      scoreboard[i].locked, scoreboard[i].bypass_count,
      scoreboard[i].use_count, scoreboard[i].bypass_value
    );
  }
}



