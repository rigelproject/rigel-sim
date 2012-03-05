
////////////////////////////////////////////////////////////////////////////////
// prefetch.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the includes data cache interaction with core model.
//
////////////////////////////////////////////////////////////////////////////////

//#include "prefetch.h"

#include <assert.h>                     // for assert
#include <stddef.h>                     // for NULL
#include <stdint.h>                     // for uint32_t
#include "caches/cache_base.h"  // for CacheAccess_t, etc
#include "caches/l2d.h"     // for L2Cache
#include "define.h"
#include "instr.h"          // for InstrLegacy
#include "prefetch.h"       // for HWPrefetcher, RPTEntry
#include "sim.h"            // for CMDLINE_CCPREFETCH_SIZE, etc

void 
HWPrefetcher::cache_access (InstrLegacy *instr, uint32_t addr, int core, int tid) {
  using namespace rigel::cache;

  CacheAccess_t ca( 
    0,                           // address
    core,                        // core number 
    tid,                         // thread id  
    IC_MSG_CCACHE_HWPREFETCH_REQ,// interconnect message type
    instr                        // instruction correlated to this access
  );


  for (int prefetch_count = 0; prefetch_count < CMDLINE_CCPREFETCH_SIZE; prefetch_count++) {

    uint32_t pref_addr = addr+(rigel::cache::LINESIZE*(1+prefetch_count));
    // If the line is already in the cache, do not prefetch it.
    // FIXME... we should check the prefetch queue too!
    if (L2->IsPending(pref_addr) != NULL) continue;
    if (L2->IsValidLine(pref_addr)) continue;
    // For safety sake, break on regular MSHR full too.
    if (L2->mshr_full(false)) break;
    // If the line is being evicted, don't grab an MSHR for it yet.
    if (L2->is_being_evicted(addr)) continue;
    // Insert the prefetch.
    //L2->pend(pref_addr, IC_MSG_CCACHE_HWPREFETCH_REQ, core, -1, false, instr, 0, FIXME_TID);
    ca.set_addr(pref_addr);
    L2->pend( ca, false );
  }

  // mapping function (to direct map cache)
  //shift over the offset (2 bits) and mask the set number (mask off the tag)
  if (rigel::cache::CMDLINE_CCPREFETCH_STRIDE){
		uint32_t table_index = GetTableIndex(instr->get_currPC());
    #ifdef PREFETCH_DEBUG
		//FIXME How do we index the table? Default is the PC
    fprintf(stderr, "Strided prefetching update - core:%d pc:%u (index:%u) addr:%u \n", 
        core, instr->get_currPC(), table_index, addr);
    #endif

    assert(0 && "WARNING: this function uses static mod-8 (%8) computation for index, likely needs fix for use\n");
    predictor_table[core%8][table_index].UpdateEntry(instr->get_currPC(), addr);
    uint32_t pref_addr;
    // Insert the prefetch.
    if (predictor_table[core%8][table_index].NextPrefetchAddress(pref_addr)) {
      if (!L2->mshr_full(false)) 
      {
        #ifdef PREFETCH_DEBUG
        fprintf(stderr, " L-> Inserting new prefetch for addr:%u \n", pref_addr);
        #endif
        //L2->pend(pref_addr, IC_MSG_CCACHE_HWPREFETCH_REQ, core, -1, false, instr, 0, FIXME_TID);
        ca.set_addr(pref_addr);
        L2->pend(ca, false);
      }
    }
  }    
 
}
		

