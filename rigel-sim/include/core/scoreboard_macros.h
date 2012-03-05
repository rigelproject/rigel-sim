////////////////////////////////////////////////////////////////////////////////
// scoreboard_macros.h
////////////////////////////////////////////////////////////////////////////////
//
//  Collection of macros that are used by execute/regfile read to get at
//  scoreboard values.  
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __SCOREBOARD_MACROS_H__
#define __SCOREBOARD_MACROS_H__

// macros for much-duped code 
// these keep scoreboard stuff from happening for functional sim as well
// FIXME : SB should pass RegisterBase around
#define IS_BYPASS_SREG0()         \
    rigel::profiler::stats[STATNAME_ONE_OPR_INSTRUCTIONS].inc(); \
    if (instr->is_bypass_sreg0()) { rigel::profiler::stats[STATNAME_ONE_OPR_BYPASSED].inc(); \
                                    sreg0.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg0.addr); } \

#define IS_BYPASS_SREG2()         \
    if (instr->is_bypass_sreg2()) { sreg2.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg2.addr); } \

#define IS_BYPASS_VEC_REG0()    \
    if (instr->is_bypass_sreg0()) { sreg0.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg0.addr); } \
    if (instr->is_bypass_sreg1()) { sreg1.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg1.addr); } \
    if (instr->is_bypass_sreg2()) { sreg2.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg2.addr); } \
    if (instr->is_bypass_sreg3()) { sreg3.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg3.addr); } \

#define IS_BYPASS_VEC_REG1()    \
    if (instr->is_bypass_sreg4()) { sreg4.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg4.addr); } \
    if (instr->is_bypass_sreg5()) { sreg5.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg5.addr); } \
    if (instr->is_bypass_sreg6()) { sreg6.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg6.addr); } \
    if (instr->is_bypass_sreg7()) { sreg7.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg7.addr); } \

// Used by VLDW and VSTW to get the base register
#define IS_BYPASS_VEC_REG_BASE()    \
    if (instr->is_bypass_sreg4()) { sreg4.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg4.addr); } \

#define UPDATE_SCOREBOARD_VEC() \
  core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg().addr, instr->get_result_reg().data.i32); \
  core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg1().addr, instr->get_result_reg1().data.i32); \
  core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg2().addr, instr->get_result_reg2().data.i32); \
  core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg3().addr, instr->get_result_reg3().data.i32);

#define IS_BYPASS_SREG0_SREG1() \
    rigel::profiler::stats[STATNAME_TWO_OPR_INSTRUCTIONS].inc(); \
    if (instr->is_bypass_sreg0() && instr->is_bypass_sreg1()) { rigel::profiler::stats[STATNAME_TWO_OPR_BYPASSED_TWO].inc(); } \
    if (instr->is_bypass_sreg0() ^  instr->is_bypass_sreg1()) { rigel::profiler::stats[STATNAME_TWO_OPR_BYPASSED_ONE].inc(); } \
    if (instr->is_bypass_sreg0()) { sreg0.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg0.addr); } \
    if (instr->is_bypass_sreg1()) { sreg1.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(sreg1.addr); } \

#define IS_BYPASS_TASK_QUEUE4() \
  using namespace rigel::task_queue;\
    if (instr->is_bypass_sreg0()) { sreg0.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(TQ_REG0); } \
    if (instr->is_bypass_sreg1()) { sreg1.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(TQ_REG1); } \
    if (instr->is_bypass_sreg2()) { sreg2.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(TQ_REG2); } \
    if (instr->is_bypass_sreg3()) { sreg3.data.i32 = core->get_scoreboard(instr->get_core_thread_id())->get_bypass(TQ_REG3); } \

#define UPDATE_SCOREBOARD() \
    core->get_scoreboard(instr->get_core_thread_id())->bypass_reg(instr->get_result_reg().addr, instr->get_result_reg().data.i32); \

#define UPDATE_SP_SCOREBOARD() \
    sp_sbs[instr->get_core_thread_id()]->bypass_reg(instr->get_result_reg().addr, instr->get_result_reg().data.i32); \

#endif
