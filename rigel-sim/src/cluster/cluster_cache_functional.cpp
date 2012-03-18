#include "cluster/cluster_cache_functional.h"
#include "sim/component_count.h"
#include "util/construction_payload.h"
#include "memory/backing_store.h"
#include "port.h"
#include "util/callback.h"

#include "isa/rigel_isa.h"

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
ClusterCacheFunctional::PerCycle()  { 

  Packet* p;
  if (!outpackets.empty()) {
    p = outpackets.front();
    outs[p->local_coreid()]->sendMsg(p);
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
void 
ClusterCacheFunctional::FunctionalMemoryRequest(Packet* p) {
  doMemoryAccess(p);
  assert(p!=0);
  // send immediately
  //outs[p->local_coreid()]->sendMsg(p);
  outpackets.push(p);
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

void
ClusterCacheFunctional::doGlobalAtomic(PacketPtr p) {
  uint32_t result = RigelISA::execGlobalAtomic(p, mem_backing_store);
  // convert the request to a reply
  p->msgType( rigel::icmsg_convert(p->msgType()) );
  p->data( result );
  p->setCompleted();
}
#if 0
#include "core/regfile.h" // TODO FIXME REMOVE ME, bad, for regval32_t
void 
ClusterCacheFunctional::doGlobalAtomic(PacketPtr p) {

  uint32_t target_addr = p->addr();
  uint32_t temp_result;
  regval32_t operand = p->gAtomicOperand();
  regval32_t swapval = p->data();

  switch (p->msgType()) {
    // atomics that return a copy of the OLD value before atomic update
    case IC_MSG_ATOMCAS_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr);
      uint32_t newval = swapval.u32();
      if (oldval == operand.u32()) { 
        mem_backing_store->write_word(target_addr, newval); 
      };
      temp_result = oldval;
      DPRINT(false,"cas target_addr: %08x \n",target_addr);
      break;
    }
    case IC_MSG_ATOMXCHG_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr); 
      uint32_t newval = swapval.u32();
      mem_backing_store->write_word(target_addr, newval);
      temp_result = oldval;
      break;
    }
    case IC_MSG_ATOMADDU_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr); 
      uint32_t newval = oldval + operand.u32();
      mem_backing_store->write_word(target_addr, newval);
      temp_result = oldval;
      break;
    }
    // atomics that return the NEW value after atomic update
    // arithmetic
    case IC_MSG_ATOMINC_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr); 
      uint32_t newval = oldval + 1;
      mem_backing_store->write_word(target_addr, newval);
      temp_result = newval;
      break;
    }
    case IC_MSG_ATOMDEC_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr); 
      uint32_t newval = oldval - 1;
      mem_backing_store->write_word(target_addr, newval);
      temp_result = newval;
      break;
    }
    // min,max
    case IC_MSG_ATOMMIN_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr);
      uint32_t newval = std::min(oldval,operand.u32());
      mem_backing_store->write_word(target_addr, newval);
      temp_result = newval;
      break;
    }
    case IC_MSG_ATOMMAX_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr);
      uint32_t newval = std::max(oldval,operand.u32());
      mem_backing_store->write_word(target_addr, newval);
      temp_result = newval;
      break;
    }
    // logical
    case IC_MSG_ATOMAND_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr);
      uint32_t newval = oldval & operand.u32();
      mem_backing_store->write_word(target_addr, newval);
      temp_result = newval;
      break;
    }
    case IC_MSG_ATOMOR_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr);
      uint32_t newval = oldval | operand.u32();
      mem_backing_store->write_word(target_addr, newval);
      temp_result = newval;
      break;
    }
    case IC_MSG_ATOMXOR_REQ: {
      uint32_t oldval = mem_backing_store->read_data_word(target_addr);
      uint32_t newval = oldval ^ operand.u32();
      mem_backing_store->write_word(target_addr, newval);
      temp_result = newval;
      break;
    }
    default:
      p->Dump();
      throw ExitSim("unknown or unimplemented Global ATOMICOP");
      break;
  }
  // convert the request to a reply
  p->msgType( rigel::icmsg_convert(p->msgType()) );
  p->data( temp_result );
  p->setCompleted();
}
#endif

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
      uint32_t readval = mem_backing_store->read_data_word(p->addr());
      DPRINT(DB_CC,"LDL: tid %d read %08x at %08x\n",tid,readval,p->addr());
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
