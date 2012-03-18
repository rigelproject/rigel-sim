////////////////////////////////////////////////////////////////////////////////
/// broadcast_manager.cpp
////////////////////////////////////////////////////////////////////////////////
///
/// formerly, global_ops.cpp
///  BroadcastManager class member functions 
///
////////////////////////////////////////////////////////////////////////////////


#include <cassert>                     // for assert
#include <stdint.h>                     // for uint32_t
#include <cstdio>                      // for fprintf, stderr
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <string>                       // for char_traits
#include <vector>                       // for vector, vector<>::iterator
#include "broadcast_manager.h"          // for BroadcastOp, etc
#include "define.h"                     // for DEBUG_HEADER
#include "sim.h"                        // for LINE_MASK, NUM_CLUSTERS, etc
#include "memory/backing_store.h"       // for GlobalBackingStoreType definition

/// Constructor
/// Structure for holding broadcast operation info - cacheline address and cluster num
BroadcastOp::BroadcastOp(uint32_t address, uint32_t newval, int whichCluster) 
{
  for(int i = 0; i < rigel::NUM_CLUSTERS; i++) { 
    seen[i] = 0; 
  }
  // Debug state is now 0xCC since none of these seen[i] values should ever be
  // touched
  for (int i = rigel::NUM_CLUSTERS; i < rigel::MAX_CLUSTERS; i++) {
    seen[i] = 0xCC;
  }
  // The cluster that does the broadcast sees the update immediately.
  seen[whichCluster] = 0xFE;    
  numSeen = 0;
  newVal = newval;
  addr = address;
  cluster = whichCluster;
}

////////////////////////////////////////////////////////////////////////////////
/// getData()
/// 
/// Called by cluster cache on data read if there is a new broadcast on this address,
///   to retrieve the data from the Broadcast instead of L1 cache in case of Update
/// 
/// PARAMETERS
/// int address : Cacheline address of the broadcast with the data being requested
/// int whichCluster : Cluster number requesting the data
///    
/// Cluster num is only used for an error msg if the Cacheline address has no oustanding
///   broadcast, should not happen because the cluster should check before asking for data
///    
/// RETURN:
///   Data word from the broadcast for the address
/// 
////////////////////////////////////////////////////////////////////////////////
uint32_t 
BroadcastManager::getData(uint32_t address, int whichCluster) 
{
  std::vector<BroadcastOp>::iterator it;
  for(it = ops.begin(); it != ops.end(); it++) {
    if(it->addr == address) {
      // If the requesting cluster has already gotten the invalidate/update
      // message or the requesting cluster was the one that triggered the
      // initial broadcast, return the new data.
      if(it->seen[whichCluster] != 0) { return it->newVal; }
      // Otherwise, return the old data still present in memory.
      else {
        return backing_store.read_word(address);
      }
    }
  }
  // Any call to getData() should be to a line that is presently being handled
  // by a broad cast. If the search through ops doesnot return a value,
  // something went awry.
  DEBUG_HEADER();
  std::cerr << "Error!  Cluster " << whichCluster << " wants data from a BCAST";
        std::cerr << "which doesn't exist at address 0x" << std::hex << address << "\n";
  assert(0 && "Cluster wanted data from nonexistent BCAST");
  return 0; //Should never get here
}

////////////////////////////////////////////////////////////////////////////////
/// ack()
/// 
/// ack() is called by a cluster cache model when it receives a Notify message
/// from the global network
/// 
/// PARAMETERS
/// int addr:  Cacheline address of the broadcast being ack'd
/// int whichCluster :  cluster number sending the ack
////////////////////////////////////////////////////////////////////////////////
void 
BroadcastManager::ack(uint32_t addr, int whichCluster)  {
  uint32_t line_addr = addr & rigel::cache::LINE_MASK;
  std::vector<BroadcastOp>::iterator it;
  bool found = false;

  // Sanity check
  if (whichCluster >= rigel::NUM_CLUSTERS) {
    DEBUG_HEADER();
    fprintf(stderr, "Invalid cluster number found in BCAST ack().\n");
    assert(0);
  }

  for(it = ops.begin(); it != ops.end(); it++)
  {
    if((it->addr & rigel::cache::LINE_MASK) == line_addr)
    {
      // The seen[] array is initialized to zero for all clusters that need
      // to be updated.  The one that issued the broadcast has already seen
      // the update and has seen[] set to 0xFE.
      if (it->seen[whichCluster] == 0) { 
        it->seen[whichCluster] = 0xFF;
        (it->numSeen)++;
      } else if (it->seen[whichCluster] == 0xFE) {
        // This is the cluster that triggered the BCAST to begin with.
        it->seen[whichCluster] = 0xFF;
        (it->numSeen)++;
      } else {
        std::cerr << "Error!  Cluster " << whichCluster << " already saw BCAST at address 0x" 
              << std::hex << addr << "\n";
        assert(0 && "BCAST seen multiple times by same cluster");
      }
      //printf("%d clusters have ACKED bcast at 0x%x (cluster
      //%d)\n",it->numSeen, it->addr,whichCluster);
      if(it->isDone()) //Update the memory model, take it out of the vector
      {
        //printf("bcast at 0x%x is done\n",it->addr);
        backing_store.write_word(it->addr, it->newVal);
        it = ops.erase(it);
      }

      found = true;
      break;
    }
  }
  if(!found)
  {
    std::cerr << "Error!  Cluster " << whichCluster << " said it saw a BCAST "
         << "which doesn't exist at address 0x" << std::hex << addr << "\n";
    assert(0 && "Cluster saw the ghost of a BCAST");
  }
  return;
}

////////////////////////////////////////////////////////////////////////////////
/// insertBroadcast()
/// 
/// Insert a broadcast into the BroadcatManager
///
/// PARAMETERS
/// int address : Cacheline address, 
/// int newData : data for the message, 
/// int whichCluster : initiating Cluster
///    
/// Called when a new broadcast invalidate or update is initiated by a core/cluster
/// Uses the BroadcastOp structure    
///////////////////////////////////////////////////////////////////////////////////
void 
BroadcastManager::insertBroadcast(uint32_t address, uint32_t newData, int whichCluster)
{
  std::vector<BroadcastOp>::iterator it;
  if(isBroadcastOutstanding(address, it)) {
    DEBUG_HEADER();
    fprintf(stderr, "MULTIPLE OUTSTANDING BCASTS! line_addr: %08x addr: %08x "
                    "whichCluster: %d\n",
                    address & rigel::cache::LINE_MASK, address, whichCluster);
    (*it).dump();
    assert(0 && "Multiple outstanding BCASTs to the same cache line not currently supported\n");
  }  
  
  BroadcastOp newop(address, newData, whichCluster);
  ops.push_back(newop);
}
