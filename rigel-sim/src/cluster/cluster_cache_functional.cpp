#include "cluster/cluster_cache_functional.h"
#include "sim/component_count.h"
#include "util/construction_payload.h"
#include "memory/backing_store.h"
#include "port.h"
#include "util/callback.h"

#define DB_CC 0

/// constructor
ClusterCacheFunctional::ClusterCacheFunctional(
  rigel::ConstructionPayload cp
) : 
  ClusterCacheBase(cp.change_name("ClusterCacheFunctional")),
  LinkTable(rigel::THREADS_PER_CLUSTER), // TODO FIXME dynamic
  ins(rigel::CORES_PER_CLUSTER), // FIXME: make this non-const
  outs(rigel::CORES_PER_CLUSTER) // FIXME: make this non-const
{

  CallbackWrapper<Packet*>* mcb
    = new MemberCallbackWrapper<ClusterCacheFunctional,Packet*,
                                &ClusterCacheFunctional::FunctionalMemoryRequest>(this);
  for (int i=0; i<ins.size(); i++) {
    ins[i] = new InPortCallback<Packet*>(mcb);
  }

  for (int i=0; i<outs.size(); i++) {
    outs[i] = new OutPortBase<Packet*>();
  }

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


/// CCFunctional specific 
void 
ClusterCacheFunctional::FunctionalMemoryRequest(Packet* p) {
  //doLocalAtomic(p);
  doMemoryAccess(p);
  assert(p!=0);
  outs[p->local_coreid()]->sendMsg(p);
}

void
ClusterCacheFunctional::doMemoryAccess(PacketPtr p) {

  switch(p->msgType()) {

    // loads
    case IC_MSG_READ_REQ:
    case IC_MSG_GLOBAL_READ_REQ: {
      uint32_t data = mem_backing_store->read_word(p->addr());
      p->data(data);
      break;
    }

    // stores
    case IC_MSG_WRITE_REQ:
    case IC_MSG_GLOBAL_WRITE_REQ:
    case IC_MSG_BCAST_UPDATE_REQ:
      mem_backing_store->write_word(p->addr(), p->data()); 
      break;

    // atomics
    case IC_MSG_LDL_REQ: 
    case IC_MSG_STC_REQ: 
      doLocalAtomic(p); 
      break;

    default:
      p->Dump();
      throw ExitSim("unhandled msgType()!");
  }

}

/// side effect: updates p
void
ClusterCacheFunctional::doLocalAtomic(PacketPtr p) {
  int tid; // cluster-level thread ID
  tid = p->cluster_tid();
  if(tid >= LinkTable.size() ) { 
    printf("invalid tid size %d in %s\n", tid, __func__); 
    throw ExitSim("invalid tid size");
  }
  assert(tid < LinkTable.size());
  switch(p->msgType()) {
    case IC_MSG_LDL_REQ: {
      LinkTable[tid].valid = true;
      LinkTable[tid].addr  = p->addr();
      LinkTable[tid].size  = sizeof(uint32_t); // TODO FIXME: non-32-bit-word sizes
      uint32_t readval = mem_backing_store->read_word(p->addr());
      DPRINT(DB_CC,"LDL: tid %d read %08x at %08x\n",tid,readval,p->addr());
      //return readval;
      p->data(readval);
      p->msgType(IC_MSG_LDL_REPLY);
      break;
    }
    case IC_MSG_STC_REQ:
      // STC succeeds if we have a valid addr match
      // FIXME: check the addr size overlap as below
      if (LinkTable[tid].valid && LinkTable[tid].addr == p->addr()) {

        // do the store
        mem_backing_store->write_word(p->addr(),p->data());
        DPRINT(DB_CC,"STC succeeded [%d]: write %08x to %08x\n",tid,p->data(),p->addr());

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
        p->data(1);
        p->msgType(IC_MSG_STC_REPLY_ACK);
        return;
        //return 1; // success
      } 
      // STC fails otherwise
      else {
        //printf("STC failed [%d] at %08x\n",tid, p->addr());
        for (unsigned i = 0; i < LinkTable.size(); ++i) {
          //printf("LinkTable: %08x v[%d]\n",LinkTable[i].addr,LinkTable[i].valid);
        }
        //return 0; // fail
        p->data(0);
        p->msgType(IC_MSG_STC_REPLY_NACK);
        return;
      }
      break;
    default:
      throw ExitSim("invalid message type in packet!");
  }
  return;
};
