#ifndef __DEFINES_H__
#define __DEFINES_H__

#include <map>
#include <iostream>
#include <stdint.h>
#include "autogen/autogen_isa_sim.h"

///////////////////////////////////////////////////////////////
// define.h
///////////////////////////////////////////////////////////////
//
// Handy #define functions
//
///////////////////////////////////////////////////////////////
#define PRINT_ADDR(addr) \
	std::cerr << "0x" << std::hex << std::setw(8) << std::setfill('0'); \
  std::cerr << addr;

#define DEBUG_HEADER() \
	std::cerr << std::dec;\
  std::cerr << rigel::CURR_CYCLE << ": ";\
  std::cerr << __FILE__ << "@" << std::dec << std::setfill(' ') << std::setw(4); \
  std::cerr << __LINE__ << ":" << __FUNCTION__ << "() ";

#define __DEBUG_HEADER(FILENAME, LINENUM, FUNCTION) \
  std::cerr << "0x" << std::hex << std::setw(12) << std::setfill('0');\
  std::cerr << rigel::CURR_CYCLE << ": ";\
  std::cerr << FILENAME << "@" << std::dec << std::setfill(' ') << std::setw(4); \
  std::cerr << LINENUM << ":" << FUNCTION << "() ";

#define TQ_DUMP_TASK(tdesc) \
  std::cerr << " { ";\
  std::cerr << "0x" << std::hex << std::setw(8) << std::setfill('0') << tdesc.v1 << ", "; \
  std::cerr << "0x" << std::hex << std::setw(8) << std::setfill('0') << tdesc.v2 << ", "; \
  std::cerr << "0x" << std::hex << std::setw(8) << std::setfill('0') << tdesc.v3 << ", "; \
  std::cerr << "0x" << std::hex << std::setw(8) << std::setfill('0') << tdesc.v4; \
  std::cerr <<  "} (core # " << std::dec << CoreNum << ") " << "\n";

#define HEX_COUT std::hex << std::setfill('0') << std::setw(8)
//
// Constant values used throughout the simulator
//
///////////////////////////////////////////////////////////////
const uint64_t U64_MAX_VAL = (uint64_t)(-1);
//
// Standard types to use throughout the simulator
//
///////////////////////////////////////////////////////////////
// Value used when reading the timestamp counter.
typedef union {
  struct {
    uint32_t lo;
    uint32_t hi;
  };
  uint64_t value;
} tsc_t;

// Types for accessing simulator memory. (used universally)
typedef uint32_t RIGEL_ADDR_T ;
typedef uint32_t RIGEL_REGVAL_T;
typedef uint32_t RIGEL_MEMVAL_T;
typedef uint32_t RIGEL_FLOAT_T;
// Arbitration policies for arbiters.  (used in arbiter.[h|cpp])
enum ARB_POLICY {
  ARB_ROUND_ROBIN = 1,
  ARB_RANDOM
};
// For determining port resource ids for arbitration. (used in arbiter.[h|cpp])
enum RES_TYPE {
  NONE,
  // Command bus for requests leaving the core L1 -> L2
  L2_READ_CMD, 
  // Reply bus from the L2 -> L1
  L2_READ_RETURN,
  // Write bus for writebacks L1 -> L2
  L2_WRITE,   
  GC_READ, GC_WRITE
};
// DRAM row state information (used in dram_model.[h|cpp])
enum DRAMState_t {
  DRAM_IDLE = 0,  // Since CurrCmd is cleared after every cycle, default to IDLE
  DRAM_ROW_ACTIVE,
  DRAM_PRECHARGE,
  DRAM_READ,
  DRAM_WRITE,
  DRAM_ERROR
};
// DRAM commands sent from the memory controller to the DRAM banks (used in
// dram_model.[h|cpp] and memory_model.[h|cpp]
enum DRAMCommand_t {
  DRAM_CMD_INVALID,   // Used for intialization.  Should never get executed
  DRAM_CMD_NOP = 0,
  DRAM_CMD_ACTIVATE,  // RAS (on), CAS (off), WE (off)
  DRAM_CMD_READ,      // RAS (off), CAS (on), WE (off)
  DRAM_CMD_WRITE,     // RAS (off), CAS (on), WE (on)
  DRAM_CMD_READA,     // RAS (off), CAS (on), WE (off), auto precharge
  DRAM_CMD_WRITEA,    // RAS (off), CAS (on), WE (on), auto precharge
  DRAM_CMD_PRECHARGE  // RAS (on), CAS (off), WE (on)
};
// Message types used between cluster caches and the global cache/memory
// controllers.  (used in all cache files and interconnect files)
typedef enum {
  // Empty message
  IC_MSG_NULL = -1,                   // -1
  // NEVER USE THIS MESSAGE!
  IC_INVALID_DO_NOT_USE = 0,          // 0
  // Local Access
  IC_MSG_READ_REQ,                    // 1
  IC_MSG_READ_REPLY,                  // 2
  IC_MSG_WRITE_REQ,                   // 3
  IC_MSG_WRITE_REPLY,                 // 4
  // Writebacks from line.wb
  IC_MSG_LINE_WRITEBACK_REQ,          // 5
  IC_MSG_LINE_WRITEBACK_REPLY,        // 6
  // Global Access
  IC_MSG_GLOBAL_READ_REQ,             // 7
  IC_MSG_GLOBAL_READ_REPLY,           // 8
  IC_MSG_GLOBAL_WRITE_REQ,            // 9
  IC_MSG_GLOBAL_WRITE_REPLY,          // 10
  // Atomic Access
  IC_MSG_ATOMXCHG_REQ,                // 11
  IC_MSG_ATOMXCHG_REPLY,              // 12
  IC_MSG_ATOMDEC_REQ,                 // 13
  IC_MSG_ATOMDEC_REPLY,               // 14
  IC_MSG_ATOMINC_REQ,                 // 15
  IC_MSG_ATOMINC_REPLY,               // 16
  IC_MSG_ATOMCAS_REQ,                 // 17
  IC_MSG_ATOMCAS_REPLY,               // 18
  IC_MSG_ATOMADDU_REQ,                // 19
  IC_MSG_ATOMADDU_REPLY,              // 20
  // Prefetch Access
  IC_MSG_PREFETCH_REQ,                // 21
  IC_MSG_PREFETCH_REPLY,              // 22
  // Prefetch w/o GCache Allocate
  IC_MSG_PREFETCH_NGA_REQ,            // 23
  IC_MSG_PREFETCH_NGA_REPLY,          // 24
  // Bulk Prefetch to G$
  IC_MSG_PREFETCH_BLOCK_GC_REQ,            // 25
  IC_MSG_PREFETCH_BLOCK_GC_REPLY,          // 26
  // Bulk Prefetch to C$
  IC_MSG_PREFETCH_BLOCK_CC_REQ,            // 27
  IC_MSG_PREFETCH_BLOCK_CC_REPLY,          // 28
  // Broadcast operations
  IC_MSG_BCAST_UPDATE_REQ,            // 29
  IC_MSG_BCAST_UPDATE_REPLY,          // 30
  IC_MSG_BCAST_UPDATE_NOTIFY,         // 31
  IC_MSG_BCAST_INV_REQ,               // 32
  IC_MSG_BCAST_INV_REPLY,             // 33
  IC_MSG_BCAST_INV_NOTIFY,            // 34
  // Instruction Fetch
  IC_MSG_INSTR_READ_REQ,              // 35
  IC_MSG_INSTR_READ_REPLY,            // 36
  // Hardware based CCache prefetch
  IC_MSG_CCACHE_HWPREFETCH_REQ,       // 37
  IC_MSG_CCACHE_HWPREFETCH_REPLY,     // 38
  IC_MSG_GCACHE_HWPREFETCH_REQ,       // 39
  IC_MSG_GCACHE_HWPREFETCH_REPLY,     // 40
  IC_MSG_EVICT_REQ,                   // 41
  IC_MSG_EVICT_REPLY,                 // 42
  // Cache coherence messages.  Note that we use PROBE/ACK which is distinct
  // from REQ/REPLY.  The NAK messages are an acknoledgement that the message
  // was recieved, but they line was not cached/held in the state so no change
  // was made.
  // Force an invalidate.  May be dropped if line is not cached.
  IC_MSG_CC_INVALIDATE_PROBE,           // 43
  IC_MSG_CC_INVALIDATE_ACK,             // 44
  IC_MSG_CC_INVALIDATE_NAK,             // 45
  // Release permission of th eline from the cluster cache holding it.  Write
  // reply comes with data.
  IC_MSG_CC_WR_RELEASE_PROBE,           // 46
  IC_MSG_CC_WR_RELEASE_ACK,             // 47
  IC_MSG_CC_WR_RELEASE_NAK,             // 48
  IC_MSG_CC_RD_RELEASE_PROBE,           // 49
  IC_MSG_CC_RD_RELEASE_ACK,             // 50
  IC_MSG_CC_RD_RELEASE_NAK,             // 51
  // Line is held in a writeable state.  Downgrade to read-only.
  IC_MSG_CC_WR2RD_DOWNGRADE_PROBE,      // 52
  IC_MSG_CC_WR2RD_DOWNGRADE_ACK,        // 53
  IC_MSG_CC_WR2RD_DOWNGRADE_NAK,        // 54
  // Check the clean data back in with the directory
  IC_MSG_CC_RD_RELEASE_REQ,             // 55
  IC_MSG_CC_RD_RELEASE_REPLY,           // 56
  // Directory requests to the GCache use special IC_MSG's
  IC_MSG_DIRECTORY_WRITE_REQ,           // 57
  IC_MSG_DIRECTORY_READ_REQ,            // 58
  // Directory request/reply for broadcast messages
  IC_MSG_CC_BCAST_PROBE,                // 59
  IC_MSG_CC_BCAST_ACK,                  // 60
  // TODO: The NAK is useful here for handling the case where something is not
  // yet in the cluster cache when an invalidation is sent, but it will be soon.
  // New instructions 
  IC_MSG_ATOMMAX_REQ,                   // 61
  IC_MSG_ATOMMAX_REPLY,                 // 62
  IC_MSG_ATOMMIN_REQ,                   // 63
  IC_MSG_ATOMMIN_REPLY,                 // 64
  IC_MSG_ATOMOR_REQ,                    // 65
  IC_MSG_ATOMOR_REPLY,                  // 66
  IC_MSG_ATOMAND_REQ,                   // 67
  IC_MSG_ATOMAND_REPLY,                 // 68
  IC_MSG_ATOMXOR_REQ,                   // 69
  IC_MSG_ATOMXOR_REPLY,                 // 70
  // Messages for broadcast splitting network. The invalidates force the data to
  // be evicted from the cache.  If a cache knows it is the owner, it responds
  // with the OWNED_REPLY.
  IC_MSG_SPLIT_BCAST_INV_REQ,           // 71
  IC_MSG_SPLIT_BCAST_INV_REPLY,         // 72
  IC_MSG_SPLIT_BCAST_SHR_REQ,           // 73
  IC_MSG_SPLIT_BCAST_SHR_REPLY,         // 74
  IC_MSG_SPLIT_BCAST_OWNED_REPLY,       // 75
  // Erro
  IC_MSG_ERROR,                          // 76
  // local atomics (LDL/STC)
  IC_MSG_LDL_REQ,   // 77
  IC_MSG_LDL_REPLY, // 78
  IC_MSG_STC_REQ,   // 79
  IC_MSG_STC_REPLY_ACK,  // 80
  IC_MSG_STC_REPLY_NACK  // 81
} icmsg_type_t;
// Instruction profile information.  (used by profile.cpp)
typedef std::map<RIGEL_ADDR_T, uint64_t> InstrProfType;
// HW TaskQueue Lock states.  (used in task_queue.[cpp|h])
enum { TQ_UNLOCKED = false, TQ_LOCKED = true };
// HW TaskQueue scheduling policies.  (used in task_queue.[cpp|h])
enum {
  TQ_SYSTEM_DEFAULT, // Alias for SINGLE
  TQ_SYSTEM_BASELINE,
  TQ_SYSTEM_INTERVAL,
  TQ_SYSTEM_INT_LIFO, // Not yet supported
  TQ_SYSTEM_RECURSIVE,
  TQ_SYSTEM_RECENT,
  TQ_SYSTEM_RECENT_V1,
  TQ_SYSTEM_RECENT_V2,
  TQ_SYSTEM_RECENT_V3,
  TQ_SYSTEM_RECENT_V4,
  TQ_SYSTEM_NEAR_RANGE,
  TQ_SYSTEM_NEAR_ALL,
  TQ_SYSTEM_RAND,
  TQ_SYSTEM_TYPE_RANGE
};
// Values that can be returned by a HW TaskQueue call.  They are also defined in
// rigel/includes/rigel-tasks.h
typedef enum TQ_RetType {
  TQ_RET_SUCCESS,   // The returned task is valid

  TQ_RET_BLOCK,     // The requester must block (or in our case, try again)

  TQ_RET_END,       // A TQ_End() was signaled while we were blocking

  TQ_RET_OVERFLOW,  // There is no space in the task queue to insert entries.
                    // Handle the overflow in hardware.

  TQ_RET_UNDERFLOW, // There are tasks overflowed and the low-water mark was
                    // hit. Software should fetch tasks from memory and fill the
                    // queues again.

  TQ_RET_INIT_FAIL, // Initialization failed for some reasons.  Possibly no more
                    // hardware contexts available

  TQ_RET_SYNC       // Once all of the tasks start blocking, a synchronization
                    // point has been reached.  Returning this value tells the
                    // core that everyone else is at the same point
} TQ_RetType;
// Since we may need to drive values other than logical 1/0, we have to attach
// a state type to each output on the buses in verification. (used in
// verif.[h|cpp])
typedef enum {
  VSTATE_VALID,
  VSTATE_VALID_SET,
  VSTATE_VALID_CLEAR,
  VSTATE_HI_Z,
  VSTATE_INVALID,
  VSTATE_DO_NOT_CARE
} verif_state_t;

//
// Utility functions that are inlined for better performance.  We use processor
// intrinsics with Visual C++ and inline assembly for all other compilers.
//

#ifdef _MSC_VER
#include <intrin.h>

#pragma intrinsic(__rdtsc)
#pragma intrinsic(_BitScanReverse)



inline uint32_t rigel_log2(const uint32_t x) {
  uint32_t y;
  _BitScanReverse((unsigned long*)&y, x);
  return y;
}

inline void RDTSC(tsc_t *tsc) {
	tsc->value = __rdtsc();
}

//
// Visual C++ does not define log2 so define it as rigel_log2
//

#define log2 rigel_log2

#else

inline uint32_t rigel_log2(const uint32_t x) {
  uint32_t y;

  asm ( "\tbsr %1, %0\n"
      : "=r"(y)
      : "r" (x)
  );

  return y;
}

inline uint32_t rigel_bsf(const uint32_t x) {
    uint32_t y;

    asm ( "\tbsf %1, %0\n"
        : "=r"(y)
        : "r" (x)
    );

    return y;
}

inline uint64_t rigel_bsf(const uint64_t x) {
  uint64_t y;
  
  asm ( "\tbsf %1, %0\n"
  : "=r"(y)
  : "r" (x)
  );
  
  return y;
}

inline void RDTSC(tsc_t *tsc) {
  __asm__ ( "rdtsc" :
          "=a" (tsc->lo), "=d" (tsc->hi)
        );
}

#endif

// Type used for priority in the network.
typedef int net_prio_t;

// Used in cache model to track stalls for profiling.
enum MemoryAccessStallReason {
  NoStall,
  L2DPending,
  L2IPending,
  L1DPending,
  L1IPending,
  L1DMSHR,
  L1IMSHR,
  L2DArbitrate,
  L2DAccess,
  L2DMSHR,
  L2IArbitrate,
  L2IAccess,
  L2IMSHR,
  StuckBehindGlobal,
  L2DAccessBitInterlock,
  L2DCoherenceStall,
  MemoryAccessStallReasonBug,
};


// Cache coherence states possible for a line.
enum coherence_state_t {
  CC_STATE_M,
  CC_STATE_O,
  CC_STATE_E,
  CC_STATE_S,
  CC_STATE_I,
  CC_STATE_ERROR
};

class InstrLegacy; // since InstrLegacy is defined below...
typedef InstrLegacy* InstrSlot;


namespace rigel {

  // return an icmsg_t corresponding to the instruction that generates it
  icmsg_type_t instr_to_icmsg(instr_t type);
  icmsg_type_t instr_to_icmsg_full(instr_t type);

  // METHOD: convert()
  // Helper routine for flipping messages around, taking requests and turing them into replies.  
  // used for comparing returned messages to requests sitting in a request buffer.
  // PARAMETERS: t: Type to change from REQ->REPLY or REPLY->REQ
  // RETURNS: Takes the message and returns the inverted type.
  // coherence probes are just passed through.
  icmsg_type_t icmsg_convert(icmsg_type_t t);
}

#endif
