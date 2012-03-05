////////////////////////////////////////////////////////////////////////////////
// mshr.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the MissHandlingEntry class used to track pending cache
//  misses.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MSHR_H__
#define __MSHR_H__

#include <bitset>
#include <set>
#include <queue>
#include <stdint.h>
#include <limits>
#include "sim.h"
// We should really remove this.  It creates a lot of nasty dependences between
// ICMsg, util.h, interconnect.h etc.
#include "interconnect.h"
// This is needed to let the inline functions know how to wake up cores on clear().
// Ideally we could just forward-declare Cluster and put the implementation of
// clear() in mshr.cpp, but we would like not to lose the benefit of inlining for
// common MSHR operations.
#include "cluster.h"
#include "util/debug.h"

#include "util/dynamic_bitset.h"

/// MissHandlingEntry
///
/// The MissHandlingEntry class is used to track outstanding misses at each level
/// of the cache.  They are usually refered to as MSHRs in the code for brevity.
/// Some of the fields are unnecessary at some levels of the cache and are simply
/// ignored.
template <int ways, int sets, int linesize, int MAX_OUTSTANDING_MISSES,
          int WB_POLICY, int CACHE_EVICTION_BUFFER_SIZE> class CacheBase;

class ReadyLinesEntry
{
  public:
    ReadyLinesEntry() : addr(std::numeric_limits<uint32_t>::max()),
                        ready_cycle(std::numeric_limits<uint64_t>::max()) { }
    ReadyLinesEntry(uint32_t _addr, uint64_t _ready_cycle)
     : addr(_addr), ready_cycle(_ready_cycle) { }

    ReadyLinesEntry(const ReadyLinesEntry &other) : addr(other.addr),
    ready_cycle(other.ready_cycle) { }

    ReadyLinesEntry & operator= (const ReadyLinesEntry &other) { addr = other.addr;
                                                                ready_cycle = other.ready_cycle;
                                                                return (*this); }
     
    uint32_t getAddr() const { return addr; }
    uint64_t getReadyCycle() const { return ready_cycle; }
    //Since we are now using these within a std::set, ReadyLinesEntry's < operator
    //must implement a strict weak ordering.  That is, equality of a and b is defined by
    // !(a < b || b < a).  So, we want < to always return false if the two addresses are equal.
    //However, if they're not equal, we want to sort on ready_cycle so the lowest (soonest)
    //ready_cycle comes first.
    bool operator<(const ReadyLinesEntry &other) const {
      if(addr == other.addr)
        return false;
      else
        return (ready_cycle < other.getReadyCycle());
    }
    //Match only on address
    bool operator==(const ReadyLinesEntry &other) const { return (addr == other.getAddr()); }
  private:
    //The address which will be ready
    uint32_t addr;
    //The absolute cycle number when it will be ready
    //Making it an absolute number instead of a "cycles from now" number avoids per-cycle decrements
    //And allows for easier debugging if something gets stuck.
    uint64_t ready_cycle;
};

template <int linesize>
class MissHandlingEntry
{  
  public: /* ACCESSORS */
    // Specify owner for cluster cache MSHRs.
    MissHandlingEntry(ClusterLegacy *owner = NULL);
    // Pass a pointer to the owning cache.
    // The MSHR will update cache-level valid bits and so forth when its methods are called.
    void init(int _id, DynamicBitset *db) { id = _id; validBits = db; }
    // Dump state for debugging purposes
    void dump() const;
    // IsRead is used to deterimine whether this is a read or a
    // write request.  All IC_MSG types are covered so that if something fishy
    // happens, we will find out about it sooner rather than just returning when
    // we do not find a type.  Atomics are all treated as writes.  Such is life.
    // Base choice on IC MSG since this is set for all valid entries.
    bool was_ccache_hwprefetch() const;
    bool was_gcache_hwprefetch() const;
    // Change the message type associated with the MSHR.
    void SetICMsgType(icmsg_type_t mt);
    // Return the message type associated with the MSHR.
    icmsg_type_t GetICMsgType() const { return _ic_msg_type; }
    // Track the first time this MSHR is valid.  Only used for stat collection.
    void set_first_attempt_cycle(uint64_t cycle) { first_attempt_cycle = cycle; }
    // Only keep the tag saved sets the members of a miss handling entry tracks
    // the address of request, number of cycles until ready, whether its a read
    // or write, and whether latency is variable
    void set(uint32_t addr, int ready_cycles, bool vlatency,
      icmsg_type_t ic_type, int core_id, int pindex, int tid);
    void set(std::set<uint32_t> const & _addrs, int ready_cycles, bool vlatency,
             icmsg_type_t ic_type, int core_id, int pindex, int tid);
    // Check the tag to see if it is valid in this entry
    bool check_valid(uint32_t addr) const;
    bool match(uint32_t addr) const;
    bool all_lines_ready() const;
    // Fast check to see if entry is valid at all regardless of tag
    bool check_valid() const { return valid; }
    // Make the MSHR available, but do not reset it since the request only *might* have completed.
    // As a corollary, don't clear the pending_cores bit vector because we may do an unclear() later
    // and we don't want that information to be wiped out.
    void clear(bool mustBeDone = true);
    // Clear the value after checking the address.
    void clear(uint32_t addr) { if (check_valid(addr)) { clear(); } }
    // Note: When doing a writeback, we may want to invalidate the MSHR
    // temporarily so that we can reuse it.
    void unclear();
    // Note: When doing a writeback, we may want to invalidate the MSHR
    // temporarily so that we can reuse it.
    void unclear(uint32_t addr) { if (check_valid(addr)) { unclear(); } }
    // Next-level cache system has acknowledged this level cache's request
    void SetRequestAcked() { done_cache_ack = true; }
    bool IsRequestAcked() const { return done_cache_ack; }
    // Return true if the MSHR is going to read from memory (non exclusive of write).
    bool IsRead() const { return (ICMsg::check_is_read(GetICMsgType())); }
    // Return the address of the miss.
    //FIXME not sure if we want this interface to be here,
    //but there are a lot of places where the MSHR really should only have one address, and it doesn't
    //make sense to have to do *(get_addrs().begin()) all over the place
    uint32_t get_addr() const;
    std::set<uint32_t>::const_iterator get_pending_addrs_begin() const { return addrs.begin(); }
    std::set<uint32_t>::const_iterator get_pending_addrs_end() const { return addrs.end(); }
    std::set<uint32_t> &get_addrs() { return addrs; }
    // Return the address to be filled.
    // On a BulkPF, this is the front of the priority queue of ready lines.
    // Otherwise, it is just normal get_addr()
    uint32_t get_fill_addr() const;
    // Are one or more lines waiting to be filled into the cache?
    bool has_ready_line() const;
    // Tell the MSHR that a fill has occurred for the specified address
    // TODO: We might not want to perform fills in FIFO order, to prevent HOLB in
    // the ready lines queue.  For instance, if two requests come in A, B, and
    // B's set has a free way but A's doesn't, we might
    // want to fill B first.  For now, enforce FIFO order.
    void notify_fill(uint32_t addr);
    bool IsDone() const { return (addrs.empty() && ready_lines.empty()); }
    // Set the new address of the miss.
    // FIXME figure out a cleaner way for bulk prefetches to poke in here and
    // set some state in the MSHR
    // to tell whatever is looking at them that the reason they are "ready" now
    // is because of address X.
    // Return the requesting core number.
    int GetCoreID() const { return _core_id; }
    // Return the requesting thread ID
    int GetThreadID() const { return _tid; }
    // get the requesting L1DCache ID
    // this is the same as the core ID for one L1D per core
    // this is the same as the ThreadID for one L1D per thread
    int GetL1DCacheID() {
      if( rigel::L1DS_PER_CORE == 1 )
        return _core_id;
      if( rigel::L1DS_PER_CORE > 1 )
        return _tid;
      assert(0 && "invalid cache configuration");
      return -1;
    }

    // Accessors for getting pending miss information for MSHR.
    int GetPendingIndex() const { return pending_index; }
    bool GetFillBlocked() const { return fill_blocked; }
    void SetFillBlocked() { fill_blocked = true; }
    void ClearFillBlocked() { fill_blocked = false; }
    // Allow operations to not cache at the global cache when bit is set
    bool GetAllowGlobalAlloc() const { return ICMsg::check_can_gcache_fill(GetICMsgType()); }
    // We must keep a bit around for outstanding requests that get probed while
    // they are pending at the Cluster Cache.  Right now this only occurs on
    // read->write upgrades.  We service the probe and taint the MSHR.  When the
    // MSHR is finally filled, we simply remove it and do not Fill.  This is
    // slightly less optimal than I would like since it keeps a new request from
    // being sent out after the MSHR is poisoned, but it is simpler.
    bool get_probe_poison_bit() const { return probe_poison_bit; }
    void set_probe_poison_bit() { probe_poison_bit = true; }
    void clear_probe_poison_bit() { probe_poison_bit = false; }
    bool get_probe_nak_bit() const { return probe_nak_bit; }
    void set_probe_nak_bit() { probe_nak_bit = true; }
    void clear_probe_nak_bit() { probe_nak_bit = false; }
    // Used for cluster cache MSHRs to indicate that another core (besides the one which
    // originally initiated the request) is waiting on the result.  This call will put the
    // core to sleep, and it will be woken up on a clear() operation.
    void AddCore(int i) {
      if ((_ic_msg_type == IC_MSG_READ_REQ) ||
          (_ic_msg_type == IC_MSG_WRITE_REQ) ||
          (_ic_msg_type == IC_MSG_GLOBAL_READ_REQ) ||
          (_ic_msg_type == IC_MSG_GLOBAL_WRITE_REQ))
      {
        pending_cores.set(i); owning_cluster->SleepCore(i);
      }
    }
    // Return/set the logical channel associated with the C$/G$ pair for this mshr
    void set_seq(int chan, seq_num_t seq) { seq_channel = chan; seq_num = seq; }
    int get_seq_channel() const { return seq_channel; }
    seq_num_t get_seq_number() const { return seq_num; }
    void wake_cores_all();
    // Check if the request generating this MSHR was a non blocking atomic,
    // i.e., it should not set the GlobalMemOp state.
    bool get_was_nonblocking_atomic() const { return was_nonblocking_atomic; }
    void set_nonblocking_atomic() { was_nonblocking_atomic = true; }
    // Notify the MSHR that the line at addr will be ready for a fill at cycle #ready_cycle
    void set_line_ready(uint32_t addr, uint64_t ready_cycle);
    void remove_addr(uint32_t addr) { addrs.erase(mask_addr(addr)); }
    static uint32_t mask_addr(uint32_t addr) { return addr & ~((uint32_t)linesize-1); }
    // Manipulate the incoherent bit.
    bool get_incoherent() const { return incoherent; }
    void set_incoherent() { incoherent = true; }
    // Interface for accessing the broadcast valid bit to signal the directory
    // that a broadcast occured to a valid line.
    bool get_bcast_resp_valid() const { return bcast_was_valid; }
    void set_bcast_resp_valid() { bcast_was_valid = true; }
    void clear_bcast_resp_valid() { bcast_was_valid = false; }
    // There is going to need to be a writeback associated with this MSHR.
    void set_bcast_write_shootdown(int o, uint32_t a);
    void clear_bcast_write_shootdown_required() { bcast_write_shootdown_required = false; }
    bool get_bcast_write_shootdown_required() const { return bcast_write_shootdown_required; }
    uint32_t get_bcast_write_shootdown_addr() const { return bcast_write_shootdown_addr; }
    int get_bcast_write_shootdown_owner() const { return bcast_write_shootdown_owner; }
    // flag for arb status used by mshr, similar to thread_arb_complete[] at cluster
    bool IsArbComplete() { return arb_complete; }
    void SetArbComplete() { arb_complete = true; }
    // helper function to sanity check mshr contents
    void sanity_check();

  public: /* DATA */
    // The message was taken by the next level of the cache.
    bool done_cache_ack;
    // The tag is the canonical comparison value.  The addr is the actual
    // address that generated the miss.  It is not used and should likely be
    // replaced by the bit vector representing words valid in the MSHR.
    uint32_t _tag;
    // How many cycles before line can be accessed.  Decremented in PerCycle()
    // for each level of the cache.
    // The type of request injected into the network
    icmsg_type_t _ic_msg_type;
    // The instruction which brought about the allocation of this MSHR.
    InstrLegacy *instr;
    // SIM_CYCLE where various actions occured for debugging.
    uint64_t first_attempt_cycle;
    uint64_t watchdog_last_set_time;
    // The core ID associated with the miss.  It may be invalid for non-core
    // requests, e.g., HW prefetches.
    int _core_id;
    // ditto, the ThreadID
    int _tid;
    // Set when the MSHR does not fill into the cluster cache on ACK
    int pending_index;
    // If the MSHR holds valid data, but it was blocked due to an eviction, it
    // will need to be tried again, but this gets it out of the network at
    // least.
    bool fill_blocked;
    // WDTs for scheduling and ack timeouts.
    int sched_WDT;
    int ack_WDT;
    // Set when a coherence probe comes in for the line.
    bool probe_poison_bit;
    bool probe_nak_bit;
    // Set on a write that finds the line in the cache, but read-only.  When the
    // read_release comes back, the controller will generate a write request.
    // Generally this should only be set in cache_model_write when a line is
    // found valid on a write attempt.
    bool read_to_write_upgrade;
    // Pointer to the cluster of which this MSHR is a part.
    // If the MSHR is not part of the cluster cache, this will be NULL.
    // This pointer is used to wake and sleep cores while they wait on a C$ miss.
    ClusterLegacy *owning_cluster;
    // Bitvector of all cores in the cluster waiting on this MSHR to complete.
    // This vector is only meaningful for cluster cache MSHRs.
    // It is used to wake up all pending cores when the MSHR is clear()ed
    std::bitset<rigel::CORES_PER_CLUSTER> pending_cores;
    // Addresses this MSHR is waiting on
    std::set<uint32_t> addrs;
    std::set<ReadyLinesEntry> ready_lines;


  private: /* Data */
    // Which MSHR am I?
    int id;
    DynamicBitset *validBits;
    // Set if there is a valid entry in this MHSR
    bool valid;
    // Sequence  numbers used for limited directories.  
    int seq_channel;
    seq_num_t seq_num;
    // Should the value that is outstanding for this request be kept incoherent
    // when it fills into (presumably) the cluster cache?
    bool incoherent;
    bool was_nonblocking_atomic;
    // CacheBase is our friend.  Everyone else needs to keep their mitts off
    // our private data.
    template <int ways, int sets, int cachelinesize, int MAX_OUTSTANDING_MISSES,
    int WB_POLICY, int CACHE_EVICTION_BUFFER_SIZE> friend class CacheBase;
    // Bit is set if the request should have the bcast_valid bit set in the ICMsg.
    bool bcast_was_valid;
    // Writeback required for the line.
    bool bcast_write_shootdown_required;
    uint32_t bcast_write_shootdown_addr;
    int bcast_write_shootdown_owner;
    // arbitration status flag for next level
    bool arb_complete;
    
};
/*********************** BEGIN MissHandlingEntry definitions ******************/

////////////////////////////////////////////////////////////////////////////////
// MissHandlingEntry() Constructor
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline
MissHandlingEntry<linesize>::MissHandlingEntry(ClusterLegacy *owner) :
      //valid(false),
      done_cache_ack(false),
      _tag(0xdeadbeef),
      _ic_msg_type(IC_MSG_ERROR),
      instr((InstrLegacy *)NULL),
      watchdog_last_set_time(rigel::CURR_CYCLE),
      _core_id(-1),
      _tid(-1),
      // Holds the index for the pendingMisses[] array
      pending_index(-1),
      fill_blocked(false),
      sched_WDT(1),
      ack_WDT(1),
      probe_poison_bit(false),
      probe_nak_bit(false),
      read_to_write_upgrade(false),
      owning_cluster(owner),
      pending_cores(0UL),
      validBits(NULL),
      valid(false),
      seq_channel(-1),
      incoherent(false),
      was_nonblocking_atomic(false),
      bcast_was_valid(false),
      arb_complete(false)
{
  // NADA
}

////////////////////////////////////////////////////////////////////////////////
// dump()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline void
MissHandlingEntry<linesize>::dump() const
{
  fprintf(stderr, "MSHR DUMP() ");
  fprintf(stderr, "valid: %1d ", valid);
  fprintf(stderr, "incoherent: %1d ", get_incoherent());
  fprintf(stderr, "addresses pending: ( ");
  for(std::set<uint32_t>::const_iterator it = addrs.begin(), end = addrs.end(); it != end; ++it)
    fprintf(stderr, "0x%08x ", *it);
  fprintf(stderr, " ), ");
  fprintf(stderr, "addresses ready: ( ");
  for(std::set<ReadyLinesEntry>::const_iterator it = ready_lines.begin(), end = ready_lines.end();
      it != end; ++it)
  {
    fprintf(stderr, "0x%08x @ %"PRIu64" ", it->getAddr(), it->getReadyCycle());
  }
  fprintf(stderr, " ), ");
  fprintf(stderr, "IsDone(): %1d ", IsDone());
  fprintf(stderr, "IsRequestAcked: %1d ", IsRequestAcked());
  fprintf(stderr, "GetFillBlocked(): %1d ", GetFillBlocked());
  fprintf(stderr, "Probe Poisoned: %1d ", get_probe_poison_bit());
  fprintf(stderr, "Probe NAK: %1d ", get_probe_nak_bit());
  fprintf(stderr, "read_to_write_upgrade: %1d ", read_to_write_upgrade);
  fprintf(stderr, "pending_index: %3d ", pending_index);
  fprintf(stderr, "seq_channel: %3d ", get_seq_channel());
  fprintf(stderr, "coreid: %4d ", _core_id);
  fprintf(stderr, "tid: %5d ", _tid);
  fprintf(stderr, "ic_msg_type: %2d ", GetICMsgType());
  fprintf(stderr, "tag: 0x%08x ", _tag);
  fprintf(stderr, "allow_global_allocate: %1d ", GetAllowGlobalAlloc());
  if(owning_cluster != NULL)
    fprintf(stderr, "pending_cores: %02lx ", pending_cores.to_ulong());
  fprintf(stderr, "first_attempt_cycle: 0x%"PRIx64 " ", first_attempt_cycle);
  fprintf(stderr, "watchdog_last_set_time: 0x%"PRIx64 " ", watchdog_last_set_time);
  fprintf(stderr, "\n");
}

////////////////////////////////////////////////////////////////////////////////
// was_ccache_hwprefetch()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline bool
MissHandlingEntry<linesize>::was_ccache_hwprefetch() const
{
  return ((GetICMsgType() == IC_MSG_CCACHE_HWPREFETCH_REQ) ||
          (GetICMsgType() == IC_MSG_CCACHE_HWPREFETCH_REPLY));
}

////////////////////////////////////////////////////////////////////////////////
// was_gcache_hwprefetch()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline bool
MissHandlingEntry<linesize>::was_gcache_hwprefetch() const
{
  return ((GetICMsgType() == IC_MSG_GCACHE_HWPREFETCH_REQ) ||
          (GetICMsgType() == IC_MSG_GCACHE_HWPREFETCH_REPLY));
}

////////////////////////////////////////////////////////////////////////////////
// SetICMsgType()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline void
MissHandlingEntry<linesize>::SetICMsgType(icmsg_type_t mt)
{
  _ic_msg_type = mt;
  assert(GetICMsgType() != IC_INVALID_DO_NOT_USE);
}

////////////////////////////////////////////////////////////////////////////////
// set()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline void
MissHandlingEntry<linesize>::
set(uint32_t addr, int ready_cycles, bool vlatency,
  icmsg_type_t ic_type, int core_id, int pindex, int tid)
{
  //if(valid && _ic_msg_type != IC_MSG_CC_RD_RELEASE_REQ && owning_cluster != NULL)
  //{
  //  printf("Error!  Cluster %d MSHR is still valid (ic_msg_type %d trying to set %d, sharing vector 0x%02lx, tag %08lx -> %08lx)\n", (owning_cluster == NULL) ? 1000 : owning_cluster->GetClusterID(), _ic_msg_type, ic_type, //pending_cores.to_ulong(), _tag, (addr >> rigel_log2(linesize)));
  //  assert(0);
  //}
  // Sanity checks.
  assert(ic_type != IC_MSG_NULL);
  assert(ic_type != IC_INVALID_DO_NOT_USE);
  if(owning_cluster != NULL)
  {
    if (!valid) { pending_cores.reset(); }
    if(core_id >= 0 && core_id < (int)pending_cores.size() &&
      ((ic_type == IC_MSG_READ_REQ) ||
      (ic_type == IC_MSG_WRITE_REQ) ||
      (ic_type == IC_MSG_GLOBAL_READ_REQ) ||
      (ic_type == IC_MSG_GLOBAL_WRITE_REQ))
      )
    {
      pending_cores.set(core_id);
      owning_cluster->SleepCore(core_id);
    }
  }

  if(valid)
  {
    rigel::GLOBAL_debug_sim.dump_all(); rigel_dump_backtrace();
    dump();
    assert(0 && "Don't set() MSHRs that are already valid");
  }

  valid = true; //this needs to be before set_line_ready()
#ifdef DEBUG_CACHES
  DEBUG_HEADER();
  cerr << "\nMSHR Core " << core_id << " Thread " << tid << " Setting validBit ID " << id << std::endl;
#endif
  if(validBits != NULL)
    validBits->set(id);
  
  addrs.clear();
  addrs.insert(mask_addr(addr));

  if(!ready_lines.empty())
  {
    rigel::GLOBAL_debug_sim.dump_all(); rigel_dump_backtrace();
    dump();
    assert(0 && "ready_lines should be empty");
  }

  if(vlatency) //not acked, the cache above must ack you
  {
    done_cache_ack = false;
  }
  else //already acknowledged and fills are pending
  {
    done_cache_ack = true;
    set_line_ready(addr, rigel::CURR_CYCLE+ready_cycles);
  }
  
  _tag = (addr >> rigel_log2(linesize));
  
  // Track the message type that this request should/will/has generated.
  SetICMsgType(ic_type);
  // The core that generated the request.  May be meaningless, e.g., a negative
  // number, due to an unsolicited request from the perspective of the cores
  // such as a writeback.
  _core_id = core_id;
  _tid     = tid;
  // The index of the MSHR in the array in which it is held.  Basically,
  // pending_index is a point to itself.
  pending_index = pindex;
  // Set a watchdog so we can make sure the MSHR makes forward progress
  watchdog_last_set_time = rigel::CURR_CYCLE;
  // By default, a request is not a read-to-write upgrade (for coherence only)
  read_to_write_upgrade = false;
  was_nonblocking_atomic = false;
  // Used for maintaining invariant that dirty lines never require broadcasts.
  bcast_write_shootdown_owner = 0xFFFF;
  bcast_write_shootdown_addr = 0;
  bcast_write_shootdown_required = false;
}

template <int linesize>
inline void
MissHandlingEntry<linesize>::set_bcast_write_shootdown( int owner, uint32_t addr) 
{
  bcast_write_shootdown_owner = owner;
  bcast_write_shootdown_addr = addr;
  bcast_write_shootdown_required = true;
}

template <int linesize>
inline void
MissHandlingEntry<linesize>::
set(std::set<uint32_t> const & _addrs, int ready_cycles, bool vlatency,
    icmsg_type_t ic_type, int core_id, int pindex, int tid)
{
  assert(!_addrs.empty() && "set()ing an MSHR with an empty set of addresses!");
  set(*(_addrs.begin()), ready_cycles, vlatency, ic_type, core_id, pindex, tid);
  std::set<uint32_t>::const_iterator it = _addrs.begin(); ++it; //Skip to the second element
  std::set<uint32_t>::const_iterator end = _addrs.end();
  addrs.insert(it, end);
  
  if(!vlatency) //Need to insert the rest of the lines here
  {
    std::set<uint32_t>::const_iterator it2 = _addrs.begin(); ++it2; //Skip to the second element
    std::set<uint32_t>::const_iterator end2 = _addrs.end();
    while(it2 != end2)
    {
      set_line_ready(*it2, rigel::CURR_CYCLE + ready_cycles);
      ++it2;
    }
  }
  // Used for maintaining invariant that dirty lines never require broadcasts.
  bcast_write_shootdown_owner = 0xFFFF;
  bcast_write_shootdown_addr = 0;
  bcast_write_shootdown_required = false;
}

////////////////////////////////////////////////////////////////////////////////
// wake_cores_all()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline void
MissHandlingEntry<linesize>::wake_cores_all()
{
  // No cores are waiting.
  if(pending_cores.none()) { return; }

  for (unsigned int i = 0; i < pending_cores.size(); i++) {
    if (pending_cores[i]) { owning_cluster->WakeCore(i); }
  }
}

////////////////////////////////////////////////////////////////////////////////
// clear()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline void
MissHandlingEntry<linesize>::clear(bool mustBeDone) //default value is true
{
  if(mustBeDone)
  {
    if(!IsDone())
    {
        rigel::GLOBAL_debug_sim.dump_all(); rigel_dump_backtrace();
        dump();
        assert(0 && "Attempting to clear() an MSHR with lines pending or ready");
    }
  }
  SetICMsgType(IC_MSG_NULL);
  valid = false;
  arb_complete = false;
  if(validBits != NULL){
#ifdef DEBUG_CACHES
    DEBUG_HEADER();
    cerr << "MSHR Core " << _core_id << " Thread " << _tid << " Clearing validBit ID " << id << std::endl;
#endif
    validBits->clear(id);
  } else {
#ifdef DEBUG_CACHES
    DEBUG_HEADER();
    cerr << "MSHR Core " << _core_id << " Thread " << _tid << " NULL validBits ID " << id << std::endl;
#endif
  }
  clear_bcast_resp_valid(); //Reset the resp valid bit.
  if(owning_cluster == NULL) //not a C$ MSHR, none of the below matters
    return;
  if(pending_cores.none()) // no core is waiting
  {
    //printf("Warning, no cores pending on this request (ic_msg_type: %2d)\n", GetICMsgType());
    return;
  }
  for(unsigned int i = 0; i < pending_cores.size(); i++)
  {
    if(pending_cores[i]) //core was waiting, let's wake it up
    {
      owning_cluster->WakeCore(i);
    }
  }
}

template <int linesize>
inline uint32_t 
MissHandlingEntry<linesize>::get_fill_addr() const 
{ 
  if (ready_lines.empty()) { 
    rigel::GLOBAL_debug_sim.dump_all(); 
    rigel_dump_backtrace(); 
    dump(); 
    assert(0); 
    return 0;
  } else {
    return ready_lines.begin()->getAddr();
  }
}

template <int linesize>
inline bool 
MissHandlingEntry<linesize>::has_ready_line() const 
{ 
  if (ready_lines.empty()) { return false; }
  else { return (ready_lines.begin()->getReadyCycle() <= rigel::CURR_CYCLE); }
}

template <int linesize>
inline uint32_t 
MissHandlingEntry<linesize>::get_addr() const 
{ 
  if (addrs.size() != 1) {
    fprintf(stderr, "Error: MSHR has %zu addresses!\n", addrs.size());
    assert(0);
  }
  return (*(addrs.begin()));
}

template <int linesize>
inline void 
MissHandlingEntry<linesize>::set_line_ready(uint32_t addr, uint64_t ready_cycle)
{
  if(!check_valid(addr))
  {
    fprintf(stderr, "Error: MSHR is not waiting on address 0x%08x\n", addr);
    dump();
    assert(0);
  }
  if(ready_lines.size() >= addrs.size())
  {
    fprintf(stderr, "Error: MSHR already has %zu ready lines and %zu pending, why push "
                    "another one (addr %08x)?\n", ready_lines.size(), addrs.size(), addr);
    dump();
    assert(0);
  }
  assert(ready_cycle >= rigel::CURR_CYCLE && "Setting MSHR to be ready in the past!");

  std::set<ReadyLinesEntry>::iterator it = ready_lines.find(ReadyLinesEntry(addr, 0));
  if(it != ready_lines.end())
  {
    fprintf(stderr, "Error!  Setting line %08x ready at cycle %08"PRIx64", but its "
                    "already ready (%08x) at cycle %08"PRIX64"\n",
                    addr, ready_cycle, it->getAddr(), it->getReadyCycle());
    dump();
    assert(0);
  }
  
  ReadyLinesEntry rle(addr, ready_cycle);
  ready_lines.insert(rle);
}

template <int linesize>
inline void 
MissHandlingEntry<linesize>::notify_fill(uint32_t addr) 
{ 
  assert(!ready_lines.empty() && "No lines ready to fill!");
  assert(addr == ready_lines.begin()->getAddr() && "Address mismatch!");
  ready_lines.erase(ready_lines.begin());
  if (addrs.erase(mask_addr(addr)) == 0)
  {
    printf("ERROR: MSHR Did not contain address %08x\n", mask_addr(addr)); dump(); assert(0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// unclear()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline void
MissHandlingEntry<linesize>::unclear()
{
  assert(false == valid && "Calling unclear() on an MSHR that was never cleared!");
  valid = true;
  if(validBits != NULL)
    validBits->set(id);
  if(owning_cluster == NULL) //not a C$ MSHR, none of the below matters
    return;
  if(pending_cores.none()) //no core is waiting
  {
    printf("Warning, no cores pending on this request (ic_msg_type: %2d)\n", GetICMsgType());
    return;
  }
  for(unsigned int i = 0; i < pending_cores.size(); i++)
  {
    if(pending_cores[i]) //core was waiting, let's put it back to sleep
    {
      owning_cluster->SleepCore(i);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// check_valid()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline bool
MissHandlingEntry<linesize>::check_valid(uint32_t addr) const
{
  if(!valid)
    return false;
  else
    return match(addr);
}

////////////////////////////////////////////////////////////////////////////////
// match()
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline bool
MissHandlingEntry<linesize>::match(uint32_t addr) const
{
  return (addrs.find(mask_addr(addr)) != addrs.end());
}

////////////////////////////////////////////////////////////////////////////////
// all_lines_ready()
////////////////////////////////////////////////////////////////////////////////
//FIXME might actually want to check to see if the addresses in the two sets are equal
template <int linesize>
inline bool
MissHandlingEntry<linesize>::all_lines_ready() const
{
  return (addrs.size() == ready_lines.size());
}


////////////////////////////////////////////////////////////////////////////////
// sanity_check()
// check mshr contents, called from cache models
// side effect kill simulation if anything weird
////////////////////////////////////////////////////////////////////////////////
template <int linesize>
inline void
MissHandlingEntry<linesize>::sanity_check() 
{
      //FIXME may want to remove this check
      if(get_addrs().size() == 0)
      {
        DEBUG_HEADER();
        fprintf(stderr, "NO ADDRESSES!\n");
        dump();
        assert(0);
      }
      
      if (GetICMsgType() == IC_MSG_NULL) {
        DEBUG_HEADER();
        fprintf(stderr, "Found NULL IC message in L1I::PerCycle()\n");
        dump();
        assert(0);
      }
      // Note that this used to turn the ICMsgType into a read.  I think it
      // was a terrible hack to work around a bug.  Bug #47 addresses this
      // issue.  So far, turning it back to an abort() has not been an issue.
      if (GetICMsgType() == IC_INVALID_DO_NOT_USE) {
        dump();
        assert(0 && "Invalid message type found");
      }

}

/************************ END MissHandlingEntry definitions *******************/

#endif

