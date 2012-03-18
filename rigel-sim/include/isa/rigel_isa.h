#ifndef __RIGEL_ISA__
#define __RIGEL_ISA__

#include <cmath>

#include "define.h"
#include "instr.h"
#include "core/regfile.h"
#include "packet/packet.h"
#include "memory/backing_store.h" // because atomic operations directly write memory

// implement the functional aspects of the Rigel ISA
class RigelISA {

  public:

  static void execALU(PipePacket* instr);

  static void execShift(PipePacket* instr);

  static void execBranch(PipePacket* instr);

  static void execBranchTarget(PipePacket* instr);

  static void execBranchPredicate(PipePacket* instr);

  static void execCompare(PipePacket* instr);

  static void execFPU(PipePacket* instr);

  static void execAddressGen(PipePacket* instr);

  /// execute a packetized global atomic request
  /// directly perform the atomic operation on memory
  static uint32_t execGlobalAtomic(PacketPtr p, rigel::GlobalBackingStoreType *mem_backing_store);

};

#endif
