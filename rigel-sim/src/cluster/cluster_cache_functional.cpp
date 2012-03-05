#include "cluster/cluster_cache_functional.h"
#include "sim/component_count.h"
#include "util/construction_payload.h"
#include "memory/backing_store.h"

/// constructor
ClusterCacheFunctional::ClusterCacheFunctional(
  rigel::ConstructionPayload cp
) : 
  ClusterCacheBase(cp.change_name("ClusterCacheFunctional")),
  LinkTable(rigel::THREADS_PER_CLUSTER) // TODO FIXME dynamic
{
}

/// component interface

int
ClusterCacheFunctional::PerCycle()  { return 0; };

void
ClusterCacheFunctional::EndSim()  {};

void
ClusterCacheFunctional::Dump()  {};

void 
ClusterCacheFunctional::Heartbeat()  {};

/// cluster cache interface

int 
ClusterCacheFunctional::sendRequest(PacketPtr ptr) {

  return 0;  
};

int 
ClusterCacheFunctional::recvResponse(PacketPtr ptr) {
  return 0;  
};

uint32_t 
ClusterCacheFunctional::doLocalAtomic(PacketPtr p, int tid) {
  switch(p->msgType()) {
    case IC_MSG_LDL_REQ: {
      if(tid >= LinkTable.size() ) { printf("invalid tid size %d in %s\n", tid, __func__); }
      assert(tid < LinkTable.size());
      LinkTable[tid].valid = true;
      LinkTable[tid].addr  = p->addr();
      LinkTable[tid].size  = sizeof(uint32_t); // TODO FIXME: non-32-bit-word sizes
      uint32_t readval = mem_backing_store->read_word(p->addr());
      DPRINT(false,"LDL: tid %d read %08x at %08x\n",tid,readval,p->addr());
      return readval;
      break;
    }
    case IC_MSG_STC_REQ:
      // STC succeeds if we have a valid addr match
      // FIXME: check the addr size overlap as below
      if (LinkTable[tid].valid && LinkTable[tid].addr == p->addr()) {

        // do the store
        mem_backing_store->write_word(p->addr(),p->data());
        //printf("STC succeeded [%d]: write %08x to %08x\n",tid,p->data(),p->addr());

        // invalidate other outstanding LDL requests to same address
        for (unsigned i = 0; i < LinkTable.size(); ++i) {

          //printf("LinkTable[%d]: %08x v[%d]\n",i,LinkTable[i].addr,LinkTable[i].valid);

          // Skip the requestor
          if (tid == i) continue;
        
          if (LinkTable[i].valid) {
            uint32_t size_mask = ~(LinkTable[i].size - 1);
            if ((p->addr() & size_mask) == (LinkTable[i].addr & size_mask)) {
              LinkTable[i].valid = false;
            }
          }

        }
        return 1; // success
      } 
      // STC fails otherwise
      else {
        //printf("STC failed [%d] at %08x\n",tid, p->addr());
        for (unsigned i = 0; i < LinkTable.size(); ++i) {
          //printf("LinkTable: %08x v[%d]\n",LinkTable[i].addr,LinkTable[i].valid);
        }
        return 0; // fail
      }
      break;
    default:
      throw ExitSim("invalid message type!");
  }
  return 0;
};
