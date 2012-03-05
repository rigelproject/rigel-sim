////////////////////////////////////////////////////////////////////////////////
// syscall_timers.cpp
////////////////////////////////////////////////////////////////////////////////
//
// Implementation for timer-related syscalls
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint64_t, uint32_t
#include "profile/profile.h"        // for Timer_t, Profile, InstrStat, etc
#include "sim.h"            // for NUM_TIMERS, CURR_CYCLE
#include "util/syscall.h"        // for Syscall, syscall_struct

// TODO: move this?
// see /include/profile.h for timer data structures
Timer_t Profile::SystemTimers[rigel::NUM_TIMERS];


////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_starttimer()
//
// Description: Starts (or restarts) the timer, but does not clear 
// previously accumulated timer cycle count
//
// Inputs: syscall_data (struct)
//
// Outputs: 0: state not changed 1: started the timer
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_starttimer(struct syscall_struct &syscall_data) {
  using namespace rigel;
  int t = syscall_data.arg1.i; // timer number
  Timer_t *SystemTimer = &(Profile::SystemTimers[t]);
  assert( (t>=0 && t<NUM_TIMERS) && "Invalid Timer Number!" ) ;
  assert( SystemTimer->valid==true && "Accessing Invalid Timer Data!" );
  // only has an effect if the timer is not already enabled
  if(SystemTimer->enabled==false) {
    SystemTimer->enabled = true;
    SystemTimer->start_time = rigel::CURR_CYCLE;
    syscall_data.result.u = 1; // started
  }
  else {
    syscall_data.result.u = 0; // did nothing
  }
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_gettimer()
//
// Description: Returns cycle count for specified timer
// If timer is currently not enabled, returns last value
//
// Inputs: syscall_data (struct)
//
// Outputs: returns lower 32 bits of number of elapsed cycles (which is a 64-bit quantity)
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_gettimer(struct syscall_struct &syscall_data) {
  using namespace rigel;
  int t = syscall_data.arg1.i; // timer number
  Timer_t *SystemTimer = &(Profile::SystemTimers[t]);
  assert( (t>=0 && t<NUM_TIMERS) && "Invalid Timer Number!" ); 
  assert( SystemTimer->valid==true && "Accessing Invalid Timer Data!");
  uint64_t delta = 0;
  // NOP: int delta_instr = 0;
  if(SystemTimer->enabled==true) {
     // compute time elapsed since last starttimer call
    delta = rigel::CURR_CYCLE - SystemTimer->start_time;
  // NOP:  delta_instr = Profile::global_timing_stats.ret_num_instrs - SystemTimer->
  }
  // NOP: int elapsed_instr = delta_instr +
  //         SystemTimer->accumulated_retired_instr;
  uint64_t elapsed = delta + SystemTimer->accumulated_time; 
  syscall_data.result.u = (uint32_t)(elapsed & 0x00000000FFFFFFFFULL);
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_stoptimer()
//
// Description: Disables the timer but does not clear the accumulated cycle count
// Updates accumulated cycle count to number of elapsed cycles at the time of
// the STOP call.
//
// Inputs: syscall_data (struct)
//
// Outputs: 1: newly disabled 0: was already disabled
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_stoptimer(struct syscall_struct &syscall_data) {
  using namespace rigel;
  int t = syscall_data.arg1.i; // timer number
  Timer_t *SystemTimer = &(Profile::SystemTimers[t]);
  assert( (t>=0 && t<NUM_TIMERS) && "Invalid Timer Number!" ) ;
  assert( SystemTimer->valid==true && "Accessing Invalid Timer Data!");
  if(SystemTimer->enabled==true) {
    SystemTimer->enabled = false;
    uint64_t delta = rigel::CURR_CYCLE - SystemTimer->start_time;
    uint64_t delta_instr =  Profile::global_timing_stats.ret_num_instrs -
          SystemTimer->start_retired_instr_count;

    SystemTimer->accumulated_time += delta;
    SystemTimer->accumulated_retired_instr += delta_instr;
    syscall_data.result.u = 1; // disabled
  }
  else {
    syscall_data.result.u = 0; // was already disabled
  }
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_cleartimer()
//
// Description: Resets all timer state to 0
//
// Inputs: syscall_data (struct)
//
// Outputs: (none)
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_cleartimer(struct syscall_struct &syscall_data) {
  using namespace rigel;
  int t = syscall_data.arg1.i; // timer number
  Timer_t *SystemTimer = &(Profile::SystemTimers[t]);
  SystemTimer->valid = true; // mark this timer as valid
  assert( (t>=0 && t<NUM_TIMERS) && "Invalid Timer Number!" ); 
  assert( SystemTimer->enabled == false && "Clearing Active Timer!");
  SystemTimer->enabled = false;
  SystemTimer->start_time = 0;
  SystemTimer->start_retired_instr_count = 0;
  SystemTimer->accumulated_time = 0;
  SystemTimer->accumulated_retired_instr = 0;
  syscall_data.result.u = 0;
}

