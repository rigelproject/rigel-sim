#include "instr/pipe_packet.h"

std::map<uint32_t,StaticDecodeInfo> PipePacket::decodedInstructions;

const char isa_reg_names[NUM_ISA_OPERAND_REGS][8] = {
  "SREG_S",
  "SREG_T",
  "DREG",
};

