#include "cluster/cluster_cache_structural.h"
#include "sim/component_count.h"
#include "util/construction_payload.h"
#include "memory/backing_store.h"
#include "port/port.h"
#include "util/callback.h"

#include "isa/rigel_isa.h"

#include <sstream>

#define DB_CC 0

/// constructor
ClusterCacheStructural::ClusterCacheStructural(
  rigel::ConstructionPayload cp,
  InPortBase<Packet*>* in,
  OutPortBase<Packet*>* out
) : 
  ClusterCacheBase(cp.change_name("ClusterCacheStructural")),
  LinkTable(rigel::THREADS_PER_CLUSTER), // TODO FIXME dynamic
  // FIXME: resize these later
  coreside_requests(numcores, fifo<Packet*>(rigel::THREADS_PER_CLUSTER)),
  coreside_replies(numcores, fifo<Packet*>(rigel::THREADS_PER_CLUSTER)),
  ccache_requests(99),
  ccache_replies(99),
  memside_requests(99),
  memside_replies(99)
{

  for (unsigned i = 0; i < coreside_ins.size(); i++) {
    std::string pname = PortName( name(), id(), "coreside_in", i );
    coreside_ins[i] = new InPortBase<Packet*>(pname);
  }

  for (unsigned i = 0; i < coreside_outs.size(); i++) {
    std::string pname = PortName( name(), id(), "coreside_out", i );
    coreside_outs[i] = new OutPortBase<Packet*>(pname);
  }

}

/// component interface

int
ClusterCacheStructural::PerCycle()  { 

  //
  handleCoreSideReplies();

  //
  handleCCacheReplies();
  
  // should consume memory-side inports...
  handleMemorySideReplies();

  // handleMemsideRequests?
  memsideBypass();

  // 
  handleCCacheRequests();

  //
  handleCoreSideRequests();
  
  // consume core-side inports
  handleCoreSideInputs();

  return 0; 

};


/// handleCoreSideInputs
///
/// check each core's port
/// if the L1 hits, then
void
ClusterCacheStructural::handleCoreSideInputs() {
  DPRINT(DB_CC,"%s\n",__func__);

  for (int i = 0; i < numcores; i++) {
    Packet* p;
    if (p = coreside_ins[i]->read()) {
      DRIGEL(DB_CC,p->Dump();)
      if (false) { //CheckL1(i, p)) { // HIT!
        // put in the L1 response queue  
        coreside_replies[i].push(p);
      } else { // Miss
        // put in the core's L1 miss queue
        coreside_requests[i].push(p);
      }
    }
  }

}

void
ClusterCacheStructural::handleCoreSideRequests() {
  DPRINT(DB_CC,"%s\n",__func__);

  for (unsigned i = 0; i < coreside_requests.size(); i++) {
    if (!coreside_requests[i].empty()) {
      Packet* p = coreside_requests[i].front();
      DRIGEL(DB_CC,p->Dump();)
      ccache_requests.push(p);
      coreside_requests[i].pop();
    }
  }

}

void 
ClusterCacheStructural::handleCCacheRequests() {
  DPRINT(DB_CC,"%s\n",__func__);

  while (!ccache_requests.empty()) {
    Packet* p = ccache_requests.front();
    DRIGEL(DB_CC,p->Dump();)
    memside_requests.push(p);
    ccache_requests.pop();
  }

}

void
ClusterCacheStructural::handleMemorySideReplies() {
  DPRINT(DB_CC,"%s\n",__func__);

  while (!memside_replies.empty()) {
    Packet* p = memside_replies.front();
    DRIGEL(DB_CC,p->Dump();)
    ccache_replies.push(p);
    memside_replies.pop();
  }
  
}

void
ClusterCacheStructural::handleCCacheReplies() {
  DPRINT(DB_CC,"%s\n",__func__);

  while (!ccache_replies.empty()) {
    Packet* p = ccache_replies.front();
    DRIGEL(DB_CC,p->Dump();)
    int map = p->local_coreid();
    coreside_replies[map].push(p);
    ccache_replies.pop();
  }

}

void
ClusterCacheStructural::memsideBypass() {
  DPRINT(DB_CC,"%s\n",__func__);

  while (!memside_requests.empty()) {
    Packet* p = memside_requests.front();
    DRIGEL(DB_CC,p->Dump();)
    doMemoryAccess(p);
    memside_replies.push(p);
    memside_requests.pop();
  }

}

void
ClusterCacheStructural::handleCoreSideReplies() {
  DPRINT(DB_CC,"%s\n",__func__);

  for (int i = 0; i < numcores; i++) {
    if (!coreside_replies[i].empty()) {
      Packet* p = coreside_replies[i].front();
      DRIGEL(DB_CC,p->Dump();)
      if (coreside_outs[i]->sendMsg(p) == ACK) {
        coreside_replies[i].pop();
      } else {
        printf("%s: sendMsg() failed on port %d\n", __func__, i);
      }
    }
  }

}


void
ClusterCacheStructural::doMemoryAccess(PacketPtr p) {

  //p->Dump();

  // FIXME: TODO: pre-decode the message type (Local vs. Global, atomic)
  // to avoid these switches all over...we already need one for implementing the
  // operation, don't use a switch to select the handler func
  switch(p->msgType()) {

    // loads
    case IC_MSG_READ_REQ:
    case IC_MSG_GLOBAL_READ_REQ: {
      uint32_t data = mem_backing_store->read_data_word(p->addr());
      p->data(data);
      p->setCompleted();
      break;
    }

    // stores
    case IC_MSG_WRITE_REQ:
    case IC_MSG_GLOBAL_WRITE_REQ:
    case IC_MSG_BCAST_UPDATE_REQ:
      mem_backing_store->write_word(p->addr(), p->data()); 
      p->setCompleted();
      break;

    // atomics
    case IC_MSG_LDL_REQ: 
    case IC_MSG_STC_REQ: 
      doLocalAtomic(p); 
      break;

    case IC_MSG_ATOMDEC_REQ:
    case IC_MSG_ATOMINC_REQ:
    case IC_MSG_ATOMADDU_REQ:
    case IC_MSG_ATOMCAS_REQ:
    case IC_MSG_ATOMXCHG_REQ:
    case IC_MSG_ATOMMIN_REQ:
    case IC_MSG_ATOMMAX_REQ:
    case IC_MSG_ATOMAND_REQ:
    case IC_MSG_ATOMOR_REQ:
    case IC_MSG_ATOMXOR_REQ:
      doGlobalAtomic(p);
      break;

    default:
      p->Dump();
      throw ExitSim("unhandled msgType()!");
  }

}

/// doGlobalAtomic
/// call a generic handler for Global Atomic operations
/// side effect: updates p
void
ClusterCacheStructural::doGlobalAtomic(PacketPtr p) {
  uint32_t result = RigelISA::execGlobalAtomic(p, mem_backing_store);
  // convert the request to a reply
  p->msgType( rigel::icmsg_convert(p->msgType()) );
  p->data( result );
  p->setCompleted();
}

/// doLocalAtomics
/// local atomics (LDL/STC) require local state where they are completed
/// side effect: updates p
void
ClusterCacheStructural::doLocalAtomic(PacketPtr p) {
  int tid; // cluster-level thread ID
  tid = p->cluster_tid();
  if(tid >= LinkTable.size() ) { 
    printf("invalid tid size %d in %s\n", tid, __func__); 
    throw ExitSim("invalid tid size");
  }
  assert(tid < LinkTable.size());
  switch(p->msgType()) {

    // Load Linked
    case IC_MSG_LDL_REQ: {
      LinkTable[tid].valid = true;
      LinkTable[tid].addr  = p->addr();
      LinkTable[tid].size  = sizeof(uint32_t); // TODO FIXME: non-32-bit-word sizes
      uint32_t readval = mem_backing_store->read_data_word(p->addr());
      DPRINT(DB_CC,"LDL: tid %d read %08x at %08x\n",tid,readval,p->addr());
      p->data(readval);
      p->msgType(IC_MSG_LDL_REPLY);
      break;
    }

    // Store Conditional
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
      } 
      // STC fails otherwise
      else {
        //printf("STC failed [%d] at %08x\n",tid, p->addr());
        for (unsigned i = 0; i < LinkTable.size(); ++i) {
          //printf("LinkTable: %08x v[%d]\n",LinkTable[i].addr,LinkTable[i].valid);
        }
        p->data(0);
        p->msgType(IC_MSG_STC_REPLY_NACK);
      }
      break;
    default:
      throw ExitSim("invalid message type in packet!");
  }
  p->setCompleted(); // access handled, return to core
  return;
};

void
ClusterCacheStructural::EndSim()  {

};

void
ClusterCacheStructural::Dump()  {
  
};

void 
ClusterCacheStructural::Heartbeat()  {};
