////////////////////////////////////////////////////////////////////////////////
// btb.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _BTB_H_
#define _BTB_H_

#include <ext/hash_map>
#include <stdint.h>
#include "sim.h"

using __gnu_cxx::hash_map;

////////////////////////////////////////////////////////////////////////////////
// class BranchTargetBuffer
////////////////////////////////////////////////////////////////////////////////
// Branch Target Buffer.  The four methods can be reimplemented however one
// likes.
////////////////////////////////////////////////////////////////////////////////
class BranchTargetBufferBase 
{
  public:
    BranchTargetBufferBase() : btb(32) {}
		virtual ~BranchTargetBufferBase() { }
    struct btb_entry_t 
    {
      uint32_t target;
      int state;
      btb_entry_t() { target = 0; valid = false; state = 0; }
      bool valid;
      uint64_t last_access_cycle;
      uint64_t replaced_cycle;
    };
    // Check for a BTB hit
    virtual bool hit(uint32_t pc) = 0;
    // Return BTB hit address
    virtual uint32_t access(uint32_t pc) = 0;
    // Set BTB address
    virtual void set(uint32_t pc, uint32_t target) = 0;
    // Initialize the BTB
    virtual void init() = 0;
    
    // Set of branch targets for address. The BTB is infinitely-sized for now.
    hash_map<uint32_t, btb_entry_t> btb;
};

////////////////////////////////////////////////////////////////////////////////
// BranchTargetBuffer
////////////////////////////////////////////////////////////////////////////////
// derived class
////////////////////////////////////////////////////////////////////////////////
class BranchTargetBuffer : public BranchTargetBufferBase
{
  public:
    // Check for a BTB hit
    bool hit(uint32_t pc);
    // Return BTB hit address
    uint32_t access(uint32_t pc);
    // Set BTB address
    void set(uint32_t pc, uint32_t target);
    // Initialize the BTB
    void init();

    ~BranchTargetBuffer() { }
};

////////////////////////////////////////////////////////////////////////////////
// hit()
////////////////////////////////////////////////////////////////////////////////
// Check for a BTB hit
inline bool 
BranchTargetBuffer::hit(uint32_t pc) 
{ 
  if (btb.find(pc) == btb.end()) return false;
  return btb[pc].valid; 
}

////////////////////////////////////////////////////////////////////////////////
// access()
////////////////////////////////////////////////////////////////////////////////
// Return BTB hit address
inline uint32_t 
BranchTargetBuffer::access(uint32_t pc) 
{ 
 // assert(hit(pc)); 
  btb[pc].valid = true;
  // Saturating counter.
  if (btb[pc].state <= 1) { btb[pc].state++; }
  btb[pc].last_access_cycle = rigel::CURR_CYCLE;
  return btb[pc].target; 
}

////////////////////////////////////////////////////////////////////////////////
// set()
////////////////////////////////////////////////////////////////////////////////
// Set BTB address
inline void 
BranchTargetBuffer::set(uint32_t pc, uint32_t target) 
{ 
  // Short circuit for BTB disabled.
  if (0 == rigel::NUM_BTB_ENTRIES) { return; }
  // Found valid entry, but hysteresis kicks in.
  if (btb.find(pc) != btb.end() && btb[pc].state > 0) { btb[pc].state--; return; }
  // Do a replacement.  If we have filled up the BTB, remove the oldest entry.  
  if (btb.find(pc) == btb.end() && btb.size() >= rigel::NUM_BTB_ENTRIES)
  {
    hash_map<uint32_t, btb_entry_t>::iterator iter;
    uint64_t min_time = UINT64_MAX;
    uint32_t min_pc = 0;
    for (iter = btb.begin(); iter != btb.end(); iter++) 
    {
      if ((*iter).second.last_access_cycle < min_time) {
        min_pc = (*iter).first;
        min_time = (*iter).second.last_access_cycle;
      }
    }
    btb.erase(min_pc);
  }
  // Insert the entry.
  btb[pc].target = target; 
  btb[pc].state = 1;
  btb[pc].valid = true;
  btb[pc].replaced_cycle = rigel::CURR_CYCLE;
  btb[pc].last_access_cycle = rigel::CURR_CYCLE;
}

#endif


