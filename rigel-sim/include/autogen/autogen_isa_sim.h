#ifndef _AUTOGEN_ISA_SIM_H_ 
#define _AUTOGEN_ISA_SIM_H_ 


namespace instr {
const char * const instr_string[] = {
  "I_NULL",  "I_PRE_DECODE",  "I_REP_HLT",  "I_DONE",  "I_BRANCH_FALL_THROUGH",  "I_CMNE_NOP",  "I_CMEQ_NOP", "I_ADD", "I_SUB", "I_ADDU", "I_SUBU", "I_ADDI", "I_SUBI", "I_ADDIU", "I_SUBIU",
 "I_ADDC", "I_ADDG", "I_ADDCU", "I_ADDGU", "I_AND", "I_OR", "I_XOR", "I_NOR",
 "I_ANDI", "I_ORI", "I_XORI", "I_SLL", "I_SRL", "I_SRA", "I_SLLI", "I_SRLI",
 "I_SRAI", "I_CEQ", "I_CLT", "I_CLE", "I_CLTU", "I_CLEU", "I_CEQF", "I_CLTF",
 "I_CLTEF", "I_CMEQ", "I_CMNE", "I_MFSR", "I_MTSR", "I_MVUI", "I_BEQ", "I_BNE",
 "I_BE", "I_BN", "I_BLT", "I_BGT", "I_BLE", "I_BGE", "I_LJ", "I_LJL",
 "I_JMP", "I_JAL", "I_JMPR", "I_JALR", "I_LDL", "I_STC", "I_PREF_L", "I_PREF_B_GC",
 "I_PREF_B_CC", "I_PREF_NGA", "I_PAUSE_LDW", "I_BCAST_INV", "I_BCAST_UPDATE", "I_LDW", "I_STW", "I_GLDW",
 "I_GSTW", "I_ATOMDEC", "I_ATOMINC", "I_ATOMXCHG", "I_ATOMCAS", "I_ATOMADDU", "I_ATOMXOR", "I_ATOMOR",
 "I_ATOMAND", "I_ATOMMAX", "I_ATOMMIN", "I_TBLOFFSET", "I_NOP", "I_BRK", "I_HLT", "I_SYNC",
 "I_UNDEF", "I_CC_WB", "I_CC_INV", "I_CC_FLUSH", "I_IC_INV", "I_MB", "I_RFE", "I_ABORT",
 "I_SYSCALL", "I_PRINTREG", "I_START_TIMER", "I_STOP_TIMER", "I_STRTOF", "I_FLUSH_BCAST", "I_LINE_WB", "I_LINE_INV",
 "I_LINE_FLUSH", "I_TQ_ENQUEUE", "I_TQ_DEQUEUE", "I_TQ_LOOP", "I_TQ_INIT", "I_TQ_END", "I_FADD", "I_FSUB",
 "I_FMUL", "I_FMADD", "I_FMSUB", "I_FRCP", "I_FRSQ", "I_FABS", "I_FMRS", "I_FMOV",
 "I_FA2R", "I_FR2A", "I_F2I", "I_I2F", "I_MUL", "I_MUL32", "I_MUL16", "I_MUL16C",
 "I_MUL16G", "I_CLZ", "I_ZEXTB", "I_SEXTB", "I_ZEXTS", "I_SEXTS", "I_VADD", "I_VSUB",
 "I_VADDI", "I_VSUBI", "I_VFADD", "I_VFSUB", "I_VFMUL", "I_VLDW", "I_VSTW", "I_EVENT",
 "I_PRIO", "I_SLEEP", "I_MAX_INSTR"
};
};
enum instr_t {
            I_NULL,      I_PRE_DECODE,         I_REP_HLT,            I_DONE,  I_BRANCH_FALL_THROUGH,        I_CMNE_NOP,        I_CMEQ_NOP,            I_ADD,            I_SUB,           I_ADDU,           I_SUBU,           I_ADDI,           I_SUBI,          I_ADDIU,          I_SUBIU,
           I_ADDC,           I_ADDG,          I_ADDCU,          I_ADDGU,            I_AND,             I_OR,            I_XOR,            I_NOR,
           I_ANDI,            I_ORI,           I_XORI,            I_SLL,            I_SRL,            I_SRA,           I_SLLI,           I_SRLI,
           I_SRAI,            I_CEQ,            I_CLT,            I_CLE,           I_CLTU,           I_CLEU,           I_CEQF,           I_CLTF,
          I_CLTEF,           I_CMEQ,           I_CMNE,           I_MFSR,           I_MTSR,           I_MVUI,            I_BEQ,            I_BNE,
             I_BE,             I_BN,            I_BLT,            I_BGT,            I_BLE,            I_BGE,             I_LJ,            I_LJL,
            I_JMP,            I_JAL,           I_JMPR,           I_JALR,            I_LDL,            I_STC,         I_PREF_L,      I_PREF_B_GC,
      I_PREF_B_CC,       I_PREF_NGA,      I_PAUSE_LDW,      I_BCAST_INV,   I_BCAST_UPDATE,            I_LDW,            I_STW,           I_GLDW,
           I_GSTW,        I_ATOMDEC,        I_ATOMINC,       I_ATOMXCHG,        I_ATOMCAS,       I_ATOMADDU,        I_ATOMXOR,         I_ATOMOR,
        I_ATOMAND,        I_ATOMMAX,        I_ATOMMIN,      I_TBLOFFSET,            I_NOP,            I_BRK,            I_HLT,           I_SYNC,
          I_UNDEF,          I_CC_WB,         I_CC_INV,       I_CC_FLUSH,         I_IC_INV,             I_MB,            I_RFE,          I_ABORT,
        I_SYSCALL,       I_PRINTREG,    I_START_TIMER,     I_STOP_TIMER,         I_STRTOF,    I_FLUSH_BCAST,        I_LINE_WB,       I_LINE_INV,
     I_LINE_FLUSH,     I_TQ_ENQUEUE,     I_TQ_DEQUEUE,        I_TQ_LOOP,        I_TQ_INIT,         I_TQ_END,           I_FADD,           I_FSUB,
           I_FMUL,          I_FMADD,          I_FMSUB,           I_FRCP,           I_FRSQ,           I_FABS,           I_FMRS,           I_FMOV,
           I_FA2R,           I_FR2A,            I_F2I,            I_I2F,            I_MUL,          I_MUL32,          I_MUL16,         I_MUL16C,
         I_MUL16G,            I_CLZ,          I_ZEXTB,          I_SEXTB,          I_ZEXTS,          I_SEXTS,           I_VADD,           I_VSUB,
          I_VADDI,          I_VSUBI,          I_VFADD,          I_VFSUB,          I_VFMUL,           I_VLDW,           I_VSTW,          I_EVENT,
           I_PRIO,          I_SLEEP, I_MAX_INSTR
};
const int INSTR_LAT[] = {
 -1, -1, -1, -1, -1, -1, -1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 3, 3,
 3, 1, 1, 1, 1, 1, 2, 2,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 3, 3,
 3, 3, 3, 10, 10, 1, 1, 1,
 3, 3, 1, 3, 2, 2, 2, 2,
 2, 2, 1, 1, 1, 1, 4, 4,
 4, 4, 4, 4, 4, 1, 1, 1,
 1, 1,
};

enum funit_t {
    FU_NONE,
    FU_ALU,
    FU_ALU_VECTOR,
    FU_FPU,
    FU_FPU_VECTOR,
    FU_FPU_SPECIAL,
    FU_ALU_SHIFT,
    FU_SPFU,
    FU_COMPARE,
    FU_MEM,
    FU_BRANCH,
    FU_BOTH,
    FU_COUNT,

};
const funit_t INSTR_FUNIT[] = {
 FU_NONE, FU_NONE, FU_NONE, FU_NONE, FU_NONE, FU_NONE, FU_NONE,
 FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU,
 FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU,
 FU_ALU, FU_ALU, FU_ALU, FU_ALU_SHIFT, FU_ALU_SHIFT, FU_ALU_SHIFT, FU_ALU_SHIFT, FU_ALU_SHIFT,
 FU_ALU_SHIFT, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_FPU, FU_FPU,
 FU_FPU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_BRANCH, FU_BRANCH,
 FU_BRANCH, FU_BRANCH, FU_BRANCH, FU_BRANCH, FU_BRANCH, FU_BRANCH, FU_BRANCH, FU_BRANCH,
 FU_BRANCH, FU_BRANCH, FU_BRANCH, FU_BRANCH, FU_MEM, FU_MEM, FU_MEM, FU_MEM,
 FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_MEM,
 FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_MEM,
 FU_MEM, FU_MEM, FU_MEM, FU_MEM, FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH,
 FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH,
 FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH, FU_MEM, FU_MEM, FU_MEM,
 FU_MEM, FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH, FU_BOTH, FU_FPU, FU_FPU,
 FU_FPU, FU_FPU, FU_FPU, FU_FPU, FU_FPU, FU_FPU, FU_ALU, FU_FPU,
 FU_FPU, FU_FPU, FU_FPU, FU_FPU, FU_ALU, FU_ALU, FU_ALU, FU_ALU,
 FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU, FU_ALU_VECTOR, FU_ALU_VECTOR,
 FU_ALU_VECTOR, FU_ALU_VECTOR, FU_FPU_VECTOR, FU_FPU_VECTOR, FU_FPU_VECTOR, FU_MEM, FU_MEM, FU_ALU,
 FU_ALU, FU_BOTH,
};

const char * const INSTR_FUNIT_STRING[] = {
 "FU_NONE", "FU_NONE", "FU_NONE", "FU_NONE", "FU_NONE", "FU_NONE", "FU_NONE",
 "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU",
 "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU",
 "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU_SHIFT", "FU_ALU_SHIFT", "FU_ALU_SHIFT", "FU_ALU_SHIFT", "FU_ALU_SHIFT",
 "FU_ALU_SHIFT", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_FPU", "FU_FPU",
 "FU_FPU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_BRANCH", "FU_BRANCH",
 "FU_BRANCH", "FU_BRANCH", "FU_BRANCH", "FU_BRANCH", "FU_BRANCH", "FU_BRANCH", "FU_BRANCH", "FU_BRANCH",
 "FU_BRANCH", "FU_BRANCH", "FU_BRANCH", "FU_BRANCH", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM",
 "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM",
 "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM",
 "FU_MEM", "FU_MEM", "FU_MEM", "FU_MEM", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH",
 "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH",
 "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_MEM", "FU_MEM", "FU_MEM",
 "FU_MEM", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_BOTH", "FU_FPU", "FU_FPU",
 "FU_FPU", "FU_FPU", "FU_FPU", "FU_FPU", "FU_FPU", "FU_FPU", "FU_ALU", "FU_FPU",
 "FU_FPU", "FU_FPU", "FU_FPU", "FU_FPU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU",
 "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU", "FU_ALU_VECTOR", "FU_ALU_VECTOR",
 "FU_ALU_VECTOR", "FU_ALU_VECTOR", "FU_FPU_VECTOR", "FU_FPU_VECTOR", "FU_FPU_VECTOR", "FU_MEM", "FU_MEM", "FU_ALU",
 "FU_ALU", "FU_BOTH",
};

#endif
