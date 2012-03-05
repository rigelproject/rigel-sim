#ifndef __RIGELPRINT_H__
#define __RIGELPRINT_H__

#include "sim.h"

class regval32_t;

class PipePacket;
namespace rigel {
  void RigelPRINTREG(uint32_t pc, regval32_t regval, int coreid, int tid);
}

#endif
