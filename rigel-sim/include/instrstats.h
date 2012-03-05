////////////////////////////////////////////////////////////////////////////////
// instrstats.h
////////////////////////////////////////////////////////////////////////////////
//
// This file contains a struct/class for tracking cycle-level stats for
// instructions. 
//
// Used from within instr.h (this code used to be there)
//
// (Document Me if you know more)
//
// Contains struct InstrCycleStats
// Contains struct InstrStats
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __INSTRSTATS_H__
#define __INSTRSTATS_H__

#include "sim.h"
#include <stdint.h>
#include <inttypes.h>
#include <cassert>

////////////////////////////////////////////////////////////////////////////////
// CLASS NAME: InstrCycleStats()
//
// Stall cycles at each stage for the instruction.
////////////////////////////////////////////////////////////////////////////////
struct InstrCycleStats {
  //A stall reason has already been specified for this instruction for this cycle.
  //This is used so that, for example, an instruction in EX which stalls due to a resource conflict
  //Doesn't also have its "stuck_behind_other_instr" counter incremented if an instruction in CC stalls as well.
  bool stall_accounted_for;
  uint64_t fetch, decode, execute, mem, fp, cc, wb;
  uint64_t instr_stall;
  uint64_t sync_waiting_for_ack, resource_conflict_alu, resource_conflict_fpu, resource_conflict_alu_shift, resource_conflict_mem, resource_conflict_branch, resource_conflict_both;
  uint64_t stuck_behind_other_instr;
  uint64_t stuck_behind_global;
  uint64_t l1d_pending, l1d_mshr;
  uint64_t l1i_pending, l1i_mshr;
  uint64_t l2d_pending, l2d_mshr, l2d_access_bit_interlock, l2d_arbitrate, l2d_access;
  uint64_t l2i_pending, l2i_mshr, l2i_arbitrate, l2i_access;
  uint64_t l1router_request, l1router_reply;
  uint64_t l2router_request, l2router_reply;
  uint64_t l2gc_request, l2gc_reply;
  uint64_t tile_requestbuffer, tile_replybuffer;
  uint64_t gnet_tilerequest, gnet_gcrequest, gnet_requestbuffer;
  uint64_t gnet_replybuffer, gnet_gcreply, gnet_tilereply;
  uint64_t sending_bcast_notifies;
  uint64_t gc_fill, gc_mshr, gc_write_allocate, gc_pending;
  void init()
  {
    fetch = 0;
    decode = 0;
    execute = 0;
    mem = 0;
    fp = 0;
    cc = 0;
    wb = 0;
    instr_stall = 0;
    sync_waiting_for_ack = 0;
    resource_conflict_alu = 0;
    resource_conflict_fpu = 0;
    resource_conflict_alu_shift = 0;
    resource_conflict_mem = 0;
    resource_conflict_branch = 0;
    resource_conflict_both = 0;
    stuck_behind_other_instr = 0;
    stuck_behind_global = 0;
    l1d_pending = 0;
    l1d_mshr = 0;
    l1i_pending = 0;
    l1i_mshr = 0;
    l2d_pending = 0;
    l2d_mshr = 0;
    l2d_access_bit_interlock = 0;
    l2d_arbitrate = 0;
    l2d_access = 0;
    l2i_pending = 0;
    l2i_mshr = 0;
    l2i_arbitrate = 0;
    l2i_access = 0;
    l1router_request = 0;
    l1router_reply = 0;
    l2router_request = 0;
    l2router_reply = 0;
    l2gc_request = 0;
    l2gc_reply = 0;
    tile_requestbuffer = 0;
    tile_replybuffer = 0;
    gnet_tilerequest = 0;
    gnet_gcrequest = 0;
    gnet_requestbuffer = 0;
    gnet_replybuffer = 0;
    gnet_gcreply = 0;
    gnet_tilereply = 0;
    sending_bcast_notifies = 0;
    gc_fill = 0;
    gc_mshr = 0;
    gc_write_allocate = 0;
    gc_pending = 0;
  }
  void add_cycles(struct InstrCycleStats other)
  {
    fetch += other.fetch;
    decode += other.decode;
    execute += other.execute;
    mem += other.mem;
    fp += other.fp;
    cc += other.cc;
    wb += other.wb;
    instr_stall += other.instr_stall;
    sync_waiting_for_ack += other.sync_waiting_for_ack;
    resource_conflict_alu += other.resource_conflict_alu;
    resource_conflict_fpu += other.resource_conflict_fpu;
    resource_conflict_alu_shift += other.resource_conflict_alu_shift;
    resource_conflict_mem += other.resource_conflict_mem;
    resource_conflict_branch += other.resource_conflict_branch;
    resource_conflict_both += other.resource_conflict_both;
    stuck_behind_other_instr += other.stuck_behind_other_instr;
    stuck_behind_global += other.stuck_behind_global;
    l1d_pending += other.l1d_pending;
    l1d_mshr += other.l1d_mshr;
    l1i_pending += other.l1i_pending;
    l1i_mshr += other.l1i_mshr;
    l2d_pending += other.l2d_pending;
    l2d_mshr += other.l2d_mshr;
    l2d_access_bit_interlock += other.l2d_access_bit_interlock;
    l2d_arbitrate += other.l2d_arbitrate;
    l2d_access += other.l2d_access;
    l2i_pending += other.l2i_pending;
    l2i_mshr += other.l2i_mshr;
    l2i_arbitrate += other.l2i_arbitrate;
    l2i_access += other.l2i_access;
    l1router_request += other.l1router_request;
    l1router_reply += other.l1router_reply;
    l2router_request += other.l2router_request;
    l2router_reply += other.l2router_reply;
    l2gc_request += other.l2gc_request;
    l2gc_reply += other.l2gc_reply;
    tile_requestbuffer += other.tile_requestbuffer;
    tile_replybuffer += other.tile_replybuffer;
    gnet_tilerequest += other.gnet_tilerequest;
    gnet_gcrequest += other.gnet_gcrequest;
    gnet_requestbuffer += other.gnet_requestbuffer;
    gnet_replybuffer += other.gnet_replybuffer;
    gnet_gcreply += other.gnet_gcreply;
    gnet_tilereply += other.gnet_tilereply;
    sending_bcast_notifies += other.sending_bcast_notifies;
    gc_fill += other.gc_fill;
    gc_mshr += other.gc_mshr;
    gc_write_allocate += other.gc_write_allocate;
    gc_pending += other.gc_pending;
  }

  void add_core(InstrCycleStats other)
  {
    if(other.fetch != 0) fetch++;
    if(other.decode != 0) decode++;
    if(other.execute != 0) execute++;
    if(other.mem != 0) mem++;
    if(other.fp != 0) fp++;
    if(other.cc != 0) cc++;
    if(other.wb != 0) wb++;
    if(other.instr_stall != 0) instr_stall++;
    if(other.sync_waiting_for_ack != 0)  sync_waiting_for_ack++;
    if(other.resource_conflict_alu != 0) resource_conflict_alu++;
    if(other.resource_conflict_fpu != 0) resource_conflict_fpu++;
    if(other.resource_conflict_alu_shift != 0) resource_conflict_alu_shift++;
    if(other.resource_conflict_mem != 0) resource_conflict_mem++;
    if(other.resource_conflict_branch != 0) resource_conflict_branch++;
    if(other.resource_conflict_both != 0) resource_conflict_both++;
    if(other.stuck_behind_other_instr != 0) stuck_behind_other_instr++;
    if(other.stuck_behind_global != 0) stuck_behind_global++;
    if(other.l1d_pending != 0) l1d_pending++;
    if(other.l1d_mshr != 0) l1d_mshr++;
    if(other.l1i_pending != 0) l1i_pending++;
    if(other.l1i_mshr != 0) l1i_mshr++;
    if(other.l2d_pending != 0) l2d_pending++;
    if(other.l2d_mshr != 0) l2d_mshr++;
    if(other.l2d_access_bit_interlock != 0) l2d_access_bit_interlock++;
    if(other.l2d_arbitrate != 0) l2d_arbitrate++;
    if(other.l2d_access != 0) l2d_access++;
    if(other.l2i_pending != 0) l2i_pending++;
    if(other.l2i_mshr != 0) l2i_mshr++;
    if(other.l2i_arbitrate != 0) l2i_arbitrate++;
    if(other.l2i_access != 0) l2i_access++;
    if(other.l1router_request != 0) l1router_request++;
    if(other.l1router_reply != 0) l1router_reply++;
    if(other.l2router_request != 0) l2router_request++;
    if(other.l2router_reply != 0) l2router_reply++;
    if(other.l2gc_request != 0) l2gc_request++;
    if(other.l2gc_reply != 0) l2gc_reply++;
    if(other.tile_requestbuffer != 0) tile_requestbuffer++;
    if(other.tile_replybuffer != 0) tile_replybuffer++;
    if(other.gnet_tilerequest != 0) gnet_tilerequest++;
    if(other.gnet_gcrequest != 0) gnet_gcrequest++;
    if(other.gnet_requestbuffer != 0) gnet_requestbuffer++;
    if(other.gnet_replybuffer != 0) gnet_replybuffer++;
    if(other.gnet_gcreply != 0) gnet_gcreply++;
    if(other.gnet_tilereply != 0) gnet_tilereply++;
    if(other.sending_bcast_notifies != 0) sending_bcast_notifies++;
    if(other.gc_fill != 0) gc_fill++;
    if(other.gc_mshr != 0) gc_mshr++;
    if(other.gc_write_allocate != 0) gc_write_allocate++;
    if(other.gc_pending != 0) gc_pending++;
  }
  void dump()
  {
    printf("fetch: %"PRIu64"\n", fetch);
    printf("decode: %"PRIu64"\n", decode);
    printf("execute: %"PRIu64"\n", execute);
    printf("mem: %"PRIu64"\n", mem);
    printf("fp: %"PRIu64"\n", fp);
    printf("cc: %"PRIu64"\n", cc);
    printf("wb: %"PRIu64"\n", wb);
    printf("instr_stall: %"PRIu64"\n", instr_stall);
    printf("sync_waiting_for_ack: %"PRIu64"\n", sync_waiting_for_ack);
    printf("resource_conflict_alu: %"PRIu64"\n", resource_conflict_alu);
    printf("resource_conflict_fpu: %"PRIu64"\n", resource_conflict_fpu);
    printf("resource_conflict_alu_shift: %"PRIu64"\n", resource_conflict_alu_shift);
    printf("resource_conflict_mem: %"PRIu64"\n", resource_conflict_mem);
    printf("resource_conflict_branch: %"PRIu64"\n", resource_conflict_branch);
    printf("resource_conflict_both: %"PRIu64"\n", resource_conflict_both);
    printf("stuck_behind_other_instr: %"PRIu64"\n", stuck_behind_other_instr);
    printf("stuck_behind_global: %"PRIu64"\n", stuck_behind_global);
    printf("l1d_pending: %"PRIu64"\n", l1d_pending);
    printf("l1d_mshr: %"PRIu64"\n", l1d_mshr);
    printf("l1i_pending: %"PRIu64"\n", l1i_pending);
    printf("l1i_mshr: %"PRIu64"\n", l1i_mshr);
    printf("l2d_pending: %"PRIu64"\n", l2d_pending);
    printf("l2d_mshr: %"PRIu64"\n", l2d_mshr);
    printf("l2d_access_bit_interlock: %"PRIu64"\n", l2d_access_bit_interlock);
    printf("l2d_arbitrate: %"PRIu64"\n", l2d_arbitrate);
    printf("l2d_access: %"PRIu64"\n", l2d_access);
    printf("l2i_pending: %"PRIu64"\n", l2i_pending);
    printf("l2i_mshr: %"PRIu64"\n", l2i_mshr);
    printf("l2i_arbitrate: %"PRIu64"\n", l2i_arbitrate);
    printf("l2i_access: %"PRIu64"\n", l2i_access);
    printf("l1router_request: %"PRIu64"\n", l1router_request);
    printf("l1router_reply: %"PRIu64"\n", l1router_reply);
    printf("l2router_request: %"PRIu64"\n", l2router_request);
    printf("l2router_reply: %"PRIu64"\n", l2router_reply);
    printf("l2gc_request: %"PRIu64"\n", l2gc_request);
    printf("l2gc_reply: %"PRIu64"\n", l2gc_reply);
    printf("tile_requestbuffer: %"PRIu64"\n", tile_requestbuffer);
    printf("tile_replybuffer: %"PRIu64"\n", tile_replybuffer);
    printf("gnet_tilerequest: %"PRIu64"\n", gnet_tilerequest);
    printf("gnet_gcrequest: %"PRIu64"\n", gnet_gcrequest);
    printf("gnet_requestbuffer: %"PRIu64"\n", gnet_requestbuffer);
    printf("gnet_replybuffer: %"PRIu64"\n", gnet_replybuffer);
    printf("gnet_gcreply: %"PRIu64"\n", gnet_gcreply);
    printf("gnet_tilereply: %"PRIu64"\n", gnet_tilereply);
    printf("sending_bcast_notifies: %"PRIu64"\n", sending_bcast_notifies);
    printf("gc_fill: %"PRIu64"\n", gc_fill);
    printf("gc_mshr: %"PRIu64"\n", gc_mshr);
    printf("gc_write_allocate: %"PRIu64"\n", gc_write_allocate);
    printf("gc_pending: %"PRIu64"\n", gc_pending);
  }
  uint64_t total_cycles()
  {
    return (fetch-1)+(decode-1)+(execute-1)+(mem-1)+(fp-1)+(cc-1)+sync_waiting_for_ack+
           resource_conflict_alu+resource_conflict_fpu+resource_conflict_alu_shift+
           resource_conflict_mem+resource_conflict_branch+resource_conflict_both+
           stuck_behind_other_instr+stuck_behind_global+l1d_pending+l1d_mshr+l1i_pending+
           l1d_mshr+l1i_pending+l1i_mshr+l2d_pending+l1i_mshr+l2d_pending+l2d_mshr
           +l2d_access_bit_interlock +l2d_arbitrate+
           l2d_access+l2i_pending+l2i_mshr+l2i_arbitrate+l2i_access+l1router_request+l1router_reply+
           l2router_request+l2router_reply+l2gc_request+l2gc_reply+tile_requestbuffer+tile_replybuffer+
           gnet_tilerequest+gnet_gcrequest+gnet_gcreply+gnet_tilereply+gnet_requestbuffer+gnet_replybuffer+
           sending_bcast_notifies+gc_fill+gc_mshr+gc_write_allocate+gc_pending;
  }
};

typedef struct InstrCycleStats InstrCycleStats;

/////////////////////////////////////////////////////////////////////////////
// struct: InstrCacheStats
//
// an internally defined class/struct...
// Cache statistics for the instruction
// used below as a member in InstrStats 
/////////////////////////////////////////////////////////////////////////////
typedef struct InstrCacheStats
{
  // Initially no hits are done.  Set by fetch and MEM stages
  InstrCacheStats() : done_L1I_hit(false), done_L2I_hit(false),
                      done_L1D_hit(false), done_L2D_hit(false),
                      done_L3D_hit(false)
                      {}

  public:
    // Idempodent accessors.  Once hit/miss is determined, lock value.
    void set_l1i_hit(bool hit) { 
      if (!done_L1I_hit) { done_L1I_hit = true; L1I_hit = hit; }
    }
    void set_l2i_hit(bool hit) { 
      if (!done_L2I_hit) { done_L2I_hit = true; L2I_hit = hit; }
    }
    void set_l1d_hit(bool hit) { 
      if (!done_L1D_hit) { done_L1D_hit = true; L1D_hit = hit; }
    }
    void set_l2d_hit(bool hit) { 
      if (!done_L2D_hit) { done_L2D_hit = true; L2D_hit = hit; }
    }
    void set_l3d_hit(bool hit) { 
      if (!done_L3D_hit) { done_L3D_hit = true; L3D_hit = hit; }
    }
    // Returns hit/miss status and does check to ensure that they were
    // properly tabulated.
    bool l1i_hit() { assert(done_L1I_hit); return L1I_hit; }
    bool l2i_hit() { assert(done_L2I_hit); return L2I_hit; }
    bool l1d_hit() { assert(done_L1D_hit); return L1D_hit; }
    bool l2d_hit() { assert(done_L2D_hit); return L2D_hit; }
    bool l3d_hit() { assert(done_L3D_hit); return L3D_hit; }
  private:
    bool L1I_hit, L2I_hit, L1D_hit, L2D_hit, L3D_hit;
    // Set once a hit/miss is calculated to lock value.
    bool done_L1I_hit, done_L2I_hit, done_L1D_hit, done_L2D_hit, done_L3D_hit;
} InstrCacheStats;
/////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CLASS NAME: InstrStats
//
// This is a class used for tracking statistics on a per-instruction basis.
// Having it in the InstrBase class ensures that we do not double count
// anything mistakenly.
////////////////////////////////////////////////////////////////////////////////
struct InstrStats {

  ///////////////////////////////////////////////////////////////////////////// 
  // public methods
  ///////////////////////////////////////////////////////////////////////////// 
  public:

    // constructor
    InstrStats() : is_memory_op(false), is_branch_op(false), is_alu_op(false),
                 is_fpu_op(false), decoded_instr_type(I_NULL), mem_polling_retry_attempts(0)
                  {cycles.init();}

    // InstrStats defininitions
    // Return type information based on decoded type, not current type in instr
    bool is_local_store() ;
    bool is_local_memaccess() ;
    bool is_global_atomic() ;
    bool is_global();
    bool is_atomic();
    bool is_local_atomic();

  ///////////////////////////////////////////////////////////////////////////// 
  // public data members
  ///////////////////////////////////////////////////////////////////////////// 
  public:

    // Op type.  Only one should be set.
    bool is_memory_op, is_branch_op, is_alu_op, is_fpu_op;

    // InstrStats needs to keep its own version of the instr type since it is lost
    // at commit, on branches, on conditionals, and on nullified ops.
    instr_t decoded_instr_type;

    InstrCacheStats cache;

    InstrCycleStats cycles;

    // Number of cycles the instruction stalled while reissuing (only for memory
    // operations)
    uint64_t mem_polling_retry_attempts;
};
#endif


