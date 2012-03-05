#include "instr/instrbase.h"

InstrBase::InstrBase( 
  uint32_t pc, /// the program counter for this instr 
  uint32_t raw /// the raw bits for this instruction
) :
  pc_( pc ),
  raw_bits_( raw )
{
}

