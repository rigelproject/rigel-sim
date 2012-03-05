////////////////////////////////////////////////////////////////////////////////
// prefetch.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the includes data cache interaction with core model.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __PREFETCH_H__
#define __PREFETCH_H__

#include "sim.h"
#include "profile/profile.h"
#include "interconnect.h"
#include "profile/profile.h"
#include "util/event_track.h"
#include "broadcast_manager.h"
#include "instr.h"

#define NUM_PREFETCHER_ENTRIES 1024
#define CORES rigel::CORES_PER_CLUSTER

class L2Cache;

enum RPTState {
	RPT_INIT_STATE,
	RPT_TRANSIENT_STATE,
	RPT_STEADY_STATE,
	RPT_NOPRED_STATE
};

///////////////////////////////////////////////////////////////////////////////
// RPTEntry
///////////////////////////////////////////////////////////////////////////////
// Implements a single table entry for a table-based prefetcher
///////////////////////////////////////////////////////////////////////////////
class RPTEntry {
	public:
		uint32_t entry_tag; //instruction address making reference
		uint32_t entry_prev_addr; //last address that accesses
		uint32_t entry_stride; //can be positive or negative
		RPTState entry_state; //2-bit state 
		
		//updated the predictor table entry
		void UpdateEntry(uint32_t tag, uint32_t curr_addr){
		  
		  //if tag doesn't match, reset
			if (entry_tag != tag) {
        #ifdef PREFETCH_DEBUG
        fprintf(stderr, " L-> Miss in prefetch table, old tag:%u \n", entry_tag);
        #endif
				entry_tag = tag;
				entry_prev_addr = curr_addr;
				entry_stride = 0;
				entry_state = RPT_INIT_STATE;
			} else { //update the existing entry
				bool prediction_successful = true;
        #ifdef PREFETCH_DEBUG
        fprintf(stderr, " L-> Existing State pc:%u, prev_addr:%u, stride:%u, state:%u \n", entry_tag, entry_prev_addr, entry_stride, entry_state);
				#endif

				//prefetch stride-based prediction was correct
				if (entry_stride == curr_addr - entry_prev_addr){
					prediction_successful = true;
				}
				else { prediction_successful = false; }

				entry_stride = curr_addr - entry_prev_addr; 
				entry_prev_addr = curr_addr;
				
				//update the state (do a transition
				if (prediction_successful) {
					switch (entry_state){
						case RPT_INIT_STATE:
							entry_state = RPT_STEADY_STATE;
							break;
						case RPT_TRANSIENT_STATE:
							entry_state = RPT_STEADY_STATE;
							break;
						case RPT_STEADY_STATE:
							entry_state = RPT_STEADY_STATE;
							break;
						case RPT_NOPRED_STATE:
							entry_state = RPT_TRANSIENT_STATE;
							break;
					}
				}
				else {
					switch (entry_state){
						case RPT_INIT_STATE:
							entry_state = RPT_TRANSIENT_STATE;
							break;
						case RPT_TRANSIENT_STATE:
							entry_state = RPT_NOPRED_STATE;
							break;
						case RPT_STEADY_STATE:
							entry_state = RPT_INIT_STATE;
							break;
						case RPT_NOPRED_STATE:
							entry_state = RPT_NOPRED_STATE;
							break;
					}
				}	
        
        #ifdef PREFETCH_DEBUG
        fprintf(stderr, " L-> Updated State pc:%u, prev_addr:%u, stride:%u, state:%u \n", entry_tag, entry_prev_addr, entry_stride, entry_state);
        #endif		
				
			}
		}
		
		//returns true if the predictor can prefetch, and a valid prefetch address
		bool NextPrefetchAddress (uint32_t & address){
			if (entry_state== RPT_STEADY_STATE) {
				address = entry_prev_addr + entry_stride;
				return true;
			} 
			
			return false;
		}
			
			
		void clear(){
			entry_tag = 0;
			entry_prev_addr = 0;
			entry_stride = 0;
			entry_state = RPT_NOPRED_STATE;		
		}

};

class HWPrefetcher{
	public:
		L2Cache* L2;
			
		//direct mapped cache for the table
		RPTEntry predictor_table[CORES][NUM_PREFETCHER_ENTRIES];
		
		HWPrefetcher(L2Cache* my_L2){
      L2 = my_L2;
    }

    uint32_t GetTableIndex(uint32_t pc){
      return (pc >> 2) & (NUM_PREFETCHER_ENTRIES - 1);
    }

		//update the table, and possibly send out a prefetch 
		//the type of prefetch that is chosen depends on our policy/hardware
		void cache_access (InstrLegacy *instr, uint32_t addr, int core, int tid);
		
};

#endif //#ifndef __PREFETCH_H__

