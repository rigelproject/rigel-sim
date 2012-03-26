#include "cluster/cluster_cache_functional.h"
#include "sim/component_count.h"
#include "util/construction_payload.h"
#include "memory/backing_store.h"
#include "port/port.h"
#include "util/callback.h"

#include "isa/rigel_isa.h"

#include <sstream>

#define DB_CC 0

#define CF_FIXED_LATENCY 0

/// constructor
ClusterCacheFunctional::ClusterCacheFunctional(
  rigel::ConstructionPayload cp,
  InPortBase<Packet*>* in,
  OutPortBase<Packet*>* out
) : 
  ClusterCacheBase(cp.change_name("ClusterCacheFunctional")),
  LinkTable(rigel::THREADS_PER_CLUSTER), // TODO FIXME dynamic
  //coreside_ins(rigel::CORES_PER_CLUSTER), // FIXME: make this non-const
  //coreside_outs(rigel::CORES_PER_CLUSTER), // FIXME: make this non-const
  outpackets(99), // make this really big functional, TODO make it unsized
  fixed_latency(CF_FIXED_LATENCY)
{

  // TODO: make this an internal class of the port, so we don't have to specify port_status_t ?
  CallbackWrapper<Packet*,port_status_t>* mcb
    = new MemberCallbackWrapper<
            ClusterCacheFunctional,
            Packet*,
            port_status_t,
            &ClusterCacheFunctional::FunctionalMemoryRequest>(this);

  for (int i = 0; i < coreside_ins.size(); i++) {
    std::string n = PortName( name(), id(), "coreside_in", i );
    coreside_ins[i] = new InPortCallback<Packet*>(n, mcb);
  }

  for (int i = 0; i < coreside_outs.size(); i++) {
    std::string n = PortName( name(), id(), "coreside_out", i );
    coreside_outs[i] = new OutPortBase<Packet*>(n);
  }

}

/// component interface

int
ClusterCacheFunctional::PerCycle()  { 

  // return all ready responses
  // functional sim does not bandwith limit responses right now (we could if we wanted to)
  // this assumes packets in front are ready first, only true for silly fixed latency model
  // otherwise, ready responses after waiting responses will be blocked here

  while (!outpackets.empty() && outpackets.front().first <= rigel::CURR_CYCLE) {
    Packet* p;
    p = outpackets.front().second;

    doMemoryAccess(p);
    assert(p!=0);

    coreside_outs[p->local_coreid()]->sendMsg(p);
    outpackets.pop(); 
  }

  return 0; 
};

void
ClusterCacheFunctional::EndSim()  {

};

void
ClusterCacheFunctional::Dump()  {
  
};

void 
ClusterCacheFunctional::Heartbeat()  {};


/// CCFunctional specific 
port_status_t 
ClusterCacheFunctional::FunctionalMemoryRequest(Packet* p) {

  if (fixed_latency == 0) { // send immediately
    doMemoryAccess(p);
    assert(p!=0);
    coreside_outs[p->local_coreid()]->sendMsg(p);
    return ACK;
  } else {
    if ( outpackets.push( std::make_pair(rigel::CURR_CYCLE + fixed_latency, p) ) ) {
      return ACK;
    } else {
      throw ExitSim("unhandled fifo overrun in cluster_cache_functional!");
    }
  }
}

void
ClusterCacheFunctional::doMemoryAccess(PacketPtr p) {

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
ClusterCacheFunctional::doGlobalAtomic(PacketPtr p) {
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
ClusterCacheFunctional::doLocalAtomic(PacketPtr p) {
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
      p->Dump();
      throw ExitSim("invalid message type in packet!");
  }
  p->setCompleted(); // access handled, return to core
  return;
};
