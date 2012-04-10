#include "instr/pipe_packet.h"
#include <tr1/unordered_map>

std::tr1::unordered_map<uint32_t,StaticDecodeInfo> PipePacket::decodedInstructions;

const char isa_reg_names[NUM_ISA_OPERAND_REGS][8] = {
  "SREG_S",
  "SREG_T",
  "DREG",
};

