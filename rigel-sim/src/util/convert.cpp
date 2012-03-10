#include "define.h"
#include <cassert>

namespace rigel {
  icmsg_type_t
  instr_to_icmsg(instr_t type) {
    switch(type) {
      case (I_LDW): return IC_MSG_READ_REQ;
      // TODO: Vector ops anyone?
      case (I_VLDW): return IC_MSG_READ_REQ;
      // LoadLinked should be treated as a write operation wrt the coherence
      // protocol.
      case (I_LDL): return IC_MSG_READ_REQ;
      case (I_STW): return IC_MSG_WRITE_REQ;
      case (I_VSTW): return IC_MSG_WRITE_REQ;
      case (I_STC): return IC_MSG_WRITE_REQ;
      case (I_GLDW): return IC_MSG_GLOBAL_READ_REQ;
      case (I_GSTW): return IC_MSG_GLOBAL_WRITE_REQ;
      case (I_ATOMINC): return IC_MSG_ATOMINC_REQ;
      case (I_ATOMDEC): return IC_MSG_ATOMDEC_REQ;
      case (I_ATOMCAS): return IC_MSG_ATOMCAS_REQ;
      case (I_ATOMADDU): return IC_MSG_ATOMADDU_REQ;
      case (I_ATOMMAX): return IC_MSG_ATOMMAX_REQ;
      case (I_ATOMMIN): return IC_MSG_ATOMMIN_REQ;
      case (I_ATOMOR): return IC_MSG_ATOMOR_REQ;
      case (I_ATOMAND): return IC_MSG_ATOMAND_REQ;
      case (I_ATOMXOR): return IC_MSG_ATOMXOR_REQ;
  
      case (I_ATOMXCHG): return IC_MSG_ATOMXCHG_REQ;
      case (I_PREF_L): return IC_MSG_PREFETCH_REQ;
      case (I_PREF_B_GC): return IC_MSG_PREFETCH_BLOCK_GC_REQ;
      case (I_PREF_B_CC): return IC_MSG_PREFETCH_BLOCK_CC_REQ;
      case (I_PREF_NGA): return IC_MSG_PREFETCH_NGA_REQ;
      case (I_LINE_WB): return IC_MSG_LINE_WRITEBACK_REQ;
      case (I_LINE_FLUSH): return IC_MSG_LINE_WRITEBACK_REQ;
      case (I_CC_WB): return IC_MSG_LINE_WRITEBACK_REQ;
      case (I_CC_FLUSH): return IC_MSG_LINE_WRITEBACK_REQ;
      case (I_BCAST_UPDATE): return IC_MSG_BCAST_UPDATE_REQ;
      case (I_BCAST_INV): return IC_MSG_BCAST_INV_REQ;
  
      default: assert(0 && "Unknown instruction type!");
    }
    assert(0 && "CANNOT REACH HERE!");
    return IC_MSG_NULL;
  }
  
  
  ////////////////////////////////////////////////////////////////////////////////
  // METHOD: convert()
  // Helper routine for flipping messages around.  Used at the global cache for
  // taking requests and turing them into replies.  Also used at the cluster cache
  // for comparing returned messages to requests sitting in a request buffer.
  // 
  // PARAMETERS: // t: Type to change from REQ->REPLY or REPLY->REQ
  // RETURNS: // Takes the message and returns the inverted type.
  // 
  // - Added coherence messages.  The probes are just passed through.
  ////////////////////////////////////////////////////////////////////////////////
  icmsg_type_t
  icmsg_convert(icmsg_type_t t) {
    switch(t)
    {
      case (IC_MSG_NULL): assert( 0 && "NULL case invalid!");
      // Coherence probes.
      case (IC_MSG_CC_INVALIDATE_PROBE):      return t;
      case (IC_MSG_CC_WR_RELEASE_PROBE):      return t;
      case (IC_MSG_CC_RD_RELEASE_PROBE):      return t;
      case (IC_MSG_CC_WR2RD_DOWNGRADE_PROBE): return t;
      case (IC_MSG_CC_BCAST_PROBE):           return t;
      case (IC_MSG_SPLIT_BCAST_INV_REQ):      return t;
      case (IC_MSG_SPLIT_BCAST_INV_REPLY):     return t;
      case (IC_MSG_SPLIT_BCAST_SHR_REQ):      return t;
      case (IC_MSG_SPLIT_BCAST_SHR_REPLY):     return t;
      case (IC_MSG_SPLIT_BCAST_OWNED_REPLY):     return t;
  
      case (IC_MSG_READ_REPLY): return IC_MSG_READ_REQ;
      case (IC_MSG_READ_REQ): return IC_MSG_READ_REPLY;
      case (IC_MSG_WRITE_REPLY): return IC_MSG_WRITE_REQ;
      case (IC_MSG_WRITE_REQ): return IC_MSG_WRITE_REPLY;
      // Evictions
      case (IC_MSG_EVICT_REPLY): return IC_MSG_EVICT_REQ;
      case (IC_MSG_EVICT_REQ): return IC_MSG_EVICT_REPLY;
      // Writebacks from line.wb
      case (IC_MSG_LINE_WRITEBACK_REPLY): return IC_MSG_LINE_WRITEBACK_REQ;
      case (IC_MSG_LINE_WRITEBACK_REQ): return IC_MSG_LINE_WRITEBACK_REPLY;
      // Global Access
      case (IC_MSG_GLOBAL_READ_REPLY): return IC_MSG_GLOBAL_READ_REQ;
      case (IC_MSG_GLOBAL_READ_REQ): return IC_MSG_GLOBAL_READ_REPLY;
      case (IC_MSG_GLOBAL_WRITE_REPLY): return IC_MSG_GLOBAL_WRITE_REQ;
      case (IC_MSG_GLOBAL_WRITE_REQ): return IC_MSG_GLOBAL_WRITE_REPLY;
      // Atomic Access
      case (IC_MSG_ATOMXCHG_REPLY): return IC_MSG_ATOMXCHG_REQ;
      case (IC_MSG_ATOMXCHG_REQ): return IC_MSG_ATOMXCHG_REPLY;
      case (IC_MSG_ATOMINC_REPLY): return IC_MSG_ATOMINC_REQ;
      case (IC_MSG_ATOMINC_REQ): return IC_MSG_ATOMINC_REPLY;
      case (IC_MSG_ATOMDEC_REPLY): return IC_MSG_ATOMDEC_REQ;
      case (IC_MSG_ATOMDEC_REQ): return IC_MSG_ATOMDEC_REPLY;
      case (IC_MSG_ATOMCAS_REPLY): return IC_MSG_ATOMCAS_REQ;
      case (IC_MSG_ATOMCAS_REQ): return IC_MSG_ATOMCAS_REPLY;
      case (IC_MSG_ATOMADDU_REPLY): return IC_MSG_ATOMADDU_REQ;
      case (IC_MSG_ATOMADDU_REQ): return IC_MSG_ATOMADDU_REPLY;
      case (IC_MSG_ATOMMAX_REQ): return IC_MSG_ATOMMAX_REPLY;
      case (IC_MSG_ATOMMAX_REPLY): return IC_MSG_ATOMMAX_REQ;
      case (IC_MSG_ATOMMIN_REQ): return IC_MSG_ATOMMIN_REPLY;
      case (IC_MSG_ATOMMIN_REPLY): return IC_MSG_ATOMMIN_REQ;
      case (IC_MSG_ATOMOR_REQ): return IC_MSG_ATOMOR_REPLY;
      case (IC_MSG_ATOMOR_REPLY): return IC_MSG_ATOMOR_REQ;
      case (IC_MSG_ATOMAND_REQ): return IC_MSG_ATOMAND_REPLY;
      case (IC_MSG_ATOMAND_REPLY): return IC_MSG_ATOMAND_REQ;
      case (IC_MSG_ATOMXOR_REQ): return IC_MSG_ATOMXOR_REPLY;
      case (IC_MSG_ATOMXOR_REPLY): return IC_MSG_ATOMXOR_REQ;
      // Prefetch Access
      case (IC_MSG_PREFETCH_REPLY): return IC_MSG_PREFETCH_REQ;
      case (IC_MSG_PREFETCH_BLOCK_GC_REPLY): return IC_MSG_PREFETCH_BLOCK_GC_REQ;
      case (IC_MSG_CCACHE_HWPREFETCH_REPLY): return IC_MSG_CCACHE_HWPREFETCH_REQ;
      case (IC_MSG_GCACHE_HWPREFETCH_REPLY): return IC_MSG_GCACHE_HWPREFETCH_REQ;
      case (IC_MSG_PREFETCH_REQ): return IC_MSG_PREFETCH_REPLY;
      case (IC_MSG_PREFETCH_BLOCK_GC_REQ): return IC_MSG_PREFETCH_BLOCK_GC_REPLY;
      case (IC_MSG_PREFETCH_BLOCK_CC_REQ): return IC_MSG_PREFETCH_BLOCK_CC_REPLY;
      case (IC_MSG_CCACHE_HWPREFETCH_REQ): return IC_MSG_CCACHE_HWPREFETCH_REPLY;
      case (IC_MSG_GCACHE_HWPREFETCH_REQ): return IC_MSG_GCACHE_HWPREFETCH_REPLY;
      case (IC_MSG_PREFETCH_NGA_REPLY): return IC_MSG_PREFETCH_NGA_REQ;
      case (IC_MSG_PREFETCH_NGA_REQ): return IC_MSG_PREFETCH_NGA_REPLY;
      // Broadcast operations
      case (IC_MSG_BCAST_INV_REQ): return IC_MSG_BCAST_INV_REPLY;
      case (IC_MSG_BCAST_INV_REPLY): return IC_MSG_BCAST_INV_REQ;
      case (IC_MSG_BCAST_UPDATE_REQ): return IC_MSG_BCAST_UPDATE_REPLY;
      case (IC_MSG_BCAST_UPDATE_REPLY): return IC_MSG_BCAST_UPDATE_REQ;
      // NOTIFY is never a request, it is inserted and then eventually recognized
      // by the interconnect and send down to the clusters.  It loiters about like
      // it is a request for a bit though.
      case (IC_MSG_BCAST_UPDATE_NOTIFY): return IC_MSG_BCAST_UPDATE_NOTIFY;
      // Instruction Fetch
      case (IC_MSG_INSTR_READ_REQ): return IC_MSG_INSTR_READ_REPLY;
      case (IC_MSG_INSTR_READ_REPLY): return IC_MSG_INSTR_READ_REQ;
      case (IC_MSG_CC_RD_RELEASE_REQ): return IC_MSG_CC_RD_RELEASE_REPLY;
      case (IC_MSG_CC_RD_RELEASE_REPLY): return IC_MSG_CC_RD_RELEASE_REQ;
  
      default: assert( 0 && "Unknown type!");
    }
    assert(0 && "CANNOT REACH HERE!");
    return IC_MSG_NULL;
  }
}
