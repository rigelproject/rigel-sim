////////////////////////////////////////////////////////////////////////////////
// broadcast_manager.h
////////////////////////////////////////////////////////////////////////////////
//
//  Header file for global operations implemented in Rigel
//  BroadcastManager class
//
////////////////////////////////////////////////////////////////////////////////

//The semantics of broadcast operations are as follows:
//Broadcast Update (bcast.u instrucion): We broadcast a single word to all cluster
//    caches by sending a single message up to the global cache, and having it
//    send down NUM_CLUSTERS messages, one to each cluster.  The new value is also
//    written into the global cache.
//    Each cluster sends back an ACK message when it gets the NOTIFY message.
//    When all NUM_CLUSTERS ACK messages have been received by the global cache,
//    it sends a REPLY message back to the core that initiated the broadcast,
//    letting it know that the broadcast operation has completed.
//    If the word (and its containing line, obviously) is present in the cluster cache,
//    the word will be updated with its new value and the line will be invalidated
//    in all L1D caches in the cluster.
//    If the word is not present, nothing happens, and the next time a thread in
//    the cluster reads that word, it will get the new value from the global cache.
//    TODO: Make sure we actually do model the timing of needing to write the new
//    value into the global cache.
//    TODO: Implement a "broadcast-entire-line" instruction.  This would be useful
//    for messages 33-256 bits long, and wouldn't cost us much in hardware since
//    our network links are already a cache line wide.  Broadcasting the entire line
//    would probably mean that we'd need to read in the rest of the line if
//    we're write-no-allocate.
//    TODO: Do we keep per-word valid bits to support being write-allocate but not
//    *reading in* the rest of the line on a write miss?  Should we?
//    TODO: If we do have per-word valid bits, we can have a "broadcast valid parts
//    of line" instruction, which subsumes the "broadcast-entire-line" instruction,
//    I think.

#ifndef __BROADCAST_MANAGER_H__
#define __BROADCAST_MANAGER_H__

#include <cstdio>
#include <stdint.h>
#include <vector>
#include <cassert>
#include "sim.h"
#include "util/construction_payload.h"

////////////////////////////////////////////////////////////////////////////////
// helper struct: BroadcastOp
////////////////////////////////////////////////////////////////////////////////
struct BroadcastOp {
  public: 
    // Constructor
    BroadcastOp(uint32_t address, uint32_t newval, int whichCluster);

    // Print contents of the bcast operations that have been seen.
    void dump() {
      fprintf(stderr, "BCAST OP DUMP: addr: 0x%08x cluster: %d\n", addr, cluster);
      for ( int i = 0; i < rigel::NUM_CLUSTERS; i++) {
        fprintf(stderr, "seen[%d]: %d\n", i, seen[i]);
      }
    }

    // Check for completion of a broadcast operation.  After the check returns
    // true, the operations is generally deleted.
    bool isDone() {
      if (numSeen > rigel::NUM_CLUSTERS) {
        DEBUG_HEADER();
        fprintf(stderr, "More bcasts have been seen for one message than "
                        "clusters!\n");
        assert(0);
      }
      return (numSeen == rigel::NUM_CLUSTERS);
    }

    uint32_t addr;
    unsigned char seen[rigel::MAX_CLUSTERS];
    uint32_t newVal;
    int numSeen;
    int cluster;
};
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CLASS: BroadcastManager
////////////////////////////////////////////////////////////////////////////////
class BroadcastManager {

  public:
    std::vector<BroadcastOp> ops;
    rigel::GlobalBackingStoreType &backing_store;
    BroadcastManager(rigel::ConstructionPayload cp) : backing_store(*(cp.backing_store)) {}

    uint32_t getData(uint32_t address, int whichCluster);
    void ack(uint32_t addr, int whichCluster);
    void insertBroadcast(uint32_t address, uint32_t newData, int whichCluster);

    bool isBroadcastOutstanding(uint32_t address) {
      std::vector<BroadcastOp>::iterator it;
      return isBroadcastOutstanding(address, it);
    }

    bool isBroadcastOutstanding(uint32_t address, std::vector<BroadcastOp>::iterator &it) {
      for(it = ops.begin(); it != ops.end(); it++) {
        if(it->addr == address) return true;
      }
      return false;
    }

    // checks for outstanding broadcast to addr
    bool isBroadcastOutstandingToLine(uint32_t address) {
      std::vector<BroadcastOp>::iterator it;
      return isBroadcastOutstandingToLine(address, it);
    }

    // checks for outstanding broadcast to addr, leaves iterator 'it' pointing to the op if found
    bool isBroadcastOutstandingToLine(uint32_t address, std::vector<BroadcastOp>::iterator &it)
    {
      for(it = ops.begin(); it != ops.end(); it++) {
        if ((it->addr & rigel::cache::LINE_MASK) == (address & rigel::cache::LINE_MASK)) {
          return true;
        }
      }
      return false;
    }
					
    void dump() {
      std::vector<BroadcastOp>::iterator it;
      for(it = ops.begin(); it != ops.end(); it++) {
        fprintf(stderr, "Broadcast Manager has a BCAST pending at "
          "0x%x from cluster %d (data %u)\n",
        it->addr, it->cluster, it->newVal);
      }
    }

};


#endif
