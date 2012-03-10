
////////////////////////////////////////////////////////////////////////////////
// icmsg.h
////////////////////////////////////////////////////////////////////////////////


#ifndef __ICMSG_H__
#define __ICMSG_H__

#include "seqnum.h"
// Necessary evil for timers.  util.h is copesetic otherwise.
#include "util/util.h"
#include "gcache_request_state_machine.h" //Have to include since we have a set<> of them :(
#include <bitset>
#include <set>
#include <vector>
#include <string>

namespace directory {
  struct dir_of_timing_t;
}

////////////////////////////////////////////////////////////////////////////////
// ICMsg
////////////////////////////////////////////////////////////////////////////////
// Contains the information necessary to send a cache request from the cluster
// cache up to the global caches and back down again.  These are what get passed
// from level to level in the network.  Data moves in cache-line sized chunks.
// For request-reply, the packet is sent up to the GCache and then sent back
// down all as one message.
////////////////////////////////////////////////////////////////////////////////
struct ICMsg 
{
  // Constructors
  ICMsg() : 
    type(IC_MSG_NULL), 
    cluster_num(0xdeadbeef), 
    dir_timer(NULL), 
    incoherent(false), 
    dir_access(false), 
    dir_hit(false),
    global_cache_access(false),
    bcast_write_shootdown_required(false) { }

  ICMsg(
       icmsg_type_t type_in,
       uint32_t addr_in,
       std::bitset<rigel::cache::LINESIZE> dirty_in,
       int cluster,
       int core,
       int tid,
       int pindex,
       InstrLegacy *spawning_instr
  ) :
      gcache_dir_proxy_accesses_complete(false),
      gcache_dir_proxy_accesses_pending(false),
      type(type_in),
      dirtybits(dirty_in),
      cluster_num(cluster),
      _core_id(core),
      _tid(tid),
      pending_index(pindex),
      memaccess_done(false),
      bcast_count(0),
      pend_set_cycle(0),
      dir_timer(NULL),
      seq_channel(0),
      incoherent(false),
      ready_cycle(0x7FFFFFFF),
      dir_access(false),
      dir_hit(false),
      global_cache_access(false),
      global_cache_pended(false),
      bcast_was_valid(false),
      bcast_write_shootdown_required(false)
  {
    cycle_inserted = rigel::CURR_CYCLE;
    gcache_last_touch = 0;
    msg_number = GLOBAL_MSG_NUMBER++;
    add_addr(addr_in);
    bcast_resp_list.clear();
  }
  public: /* METHODS */
    // Set the cycle where the gcache pend()'s the request
    void set_pend_cycle() { pend_set_cycle = rigel::CURR_CYCLE; }
    void set_gcache_last_touch_cycle() { pend_set_cycle = rigel::CURR_CYCLE; }

    // Print out the message for debugging purposes
    void dump(std::string callee_file, int callee_line) const;

    // Return the type of the message.
    icmsg_type_t get_type() const { return type;     }

    // Return the core that spawned the request.  May be an invalid core number.
    int GetCoreID()        const { return _core_id; }

    // Return the thread that spawned the request.  May be an invalid thread ID.
    int GetThreadID()      const { return _tid;     }

    // The pending index in the L2 cache associated with this message.
    int GetPendingIndex() const { return pending_index; }

    // Change the type of the request.
    void set_type(icmsg_type_t t) { type = t; }

    // Return the cluster number for this message
    int get_cluster() const { return cluster_num; }

    // Set the cluster number for the message.
    void set_cluster_num(int c) { cluster_num = c; }

    // Return the address of the message
    uint32_t get_addr() const;

    // Return the unique message number identifier.
    uint64_t get_msg_number() const { return msg_number; }

    // Set the value of all the sequence numbers.
    void set_seq(int seq_chan, seq_num_t seq_num) { seq_number = seq_num; seq_channel = seq_chan; }

    // Return the sequence number associated with the request.
    int get_seq_channel() const { return seq_channel; }
    seq_num_t get_seq_number() const { return seq_number; }

    // Returns true if this request does not need to be valid in the GCache but has completed.
    bool check_memaccess_done() const { return memaccess_done; };

    // Set the access as being done at memory.
    void set_memaccess_done() { memaccess_done = true; };

    // Was this message found to be incoherent by the directory? False by default.
    void set_incoherent() { incoherent = true; }
    bool get_incoherent() const { return incoherent; }

    // Set the number of cycles in the future when message is ready to move on to
    // next cycle.
    void set_ready_cycle(uint32_t off) { ready_cycle = rigel::CURR_CYCLE + off; }

    // Returns true if the message can be routed to the next level.
    bool is_ready_cycle() const { return (ready_cycle <= rigel::CURR_CYCLE); }

    // Return the cycle when this request last became ready.
    uint64_t get_ready_cycle() const { return ready_cycle; }

    // Set the directory timing pointer.
    void set_dir_timer(directory::dir_of_timing_t *d) { dir_timer = d; }

    // Return the directory timing pointer.
    directory::dir_of_timing_t * get_dir_timer() const { return dir_timer; }

    // Return the number of broadcasts left to inject on behalf of this message.
    int get_bcast_count() const { return bcast_count; }

    // Increment the number of completed injections into the GNet for the bcast.
    int inc_bcast_count() { return bcast_count++; }

    // Set the message number for the message and increment the global message number
    void update_msg_number() { msg_number = GLOBAL_MSG_NUMBER++; }

    // Return true if the request was an L3 hit.
    bool get_gcache_hit() const { return gcache_hit; }
    void set_gcache_hit() { gcache_hit = true; }
    void set_gcache_miss() { gcache_hit = false; }
    void set_gcache_pended() { global_cache_pended = true; }
    bool check_gcache_pended() const { return global_cache_pended; }

    //Copy set of addresses over from e.g. MSHR.
    //Takes two arguments which are iterators (probably to some STL container) representing
    //the beginning and end of the sequence of addresses to add.
    void copy_addresses(std::set<uint32_t>::const_iterator begin, std::set<uint32_t>::const_iterator end);
    void copy_addresses(std::vector<GCacheRequestStateMachine>::const_iterator begin, std::vector<GCacheRequestStateMachine>::const_iterator end);
    std::vector<GCacheRequestStateMachine>::iterator get_sms_begin();
    std::vector<GCacheRequestStateMachine>::iterator get_sms_end();
    std::vector<GCacheRequestStateMachine>::const_iterator get_sms_begin() const;
    std::vector<GCacheRequestStateMachine>::const_iterator get_sms_end() const;
    void add_addr(uint32_t addr);
    void remove_addr(std::vector<GCacheRequestStateMachine>::iterator it);
    void remove_all_addrs();
    void remove_all_addrs_except(uint32_t addr);
    bool all_addrs_done() const;

    // Keep track of directory hit/miss status.
    void set_directory_cache_hit(bool s) { if (!dir_access) { dir_access = true; dir_hit = s; } }
    bool check_directory_access() const { return dir_access; }
    bool directory_access_hit() const { return dir_hit; }

    // Keep track of global cache hit/miss status.
    void set_global_cache_hit(bool s) { if (!global_cache_access) { global_cache_access = true; global_cache_hit= s; } }
    bool check_global_cache_access() const { return global_cache_access; }
    bool global_cache_access_hit() const { return global_cache_hit; }

    // For broadcasts, we need the sequence number for each cluster.
    void set_bcast_seq(int cluster, int seq_chan, seq_num_t seq_num);

    // Return the sequence channel and number for the operation.  Only used in
    // broadcast splits for now. 
    int get_bcast_seq_channel(int cluster) { return bcast_seq_channel[cluster]; }
    seq_num_t get_bcast_seq_number(int cluster) { return bcast_seq_number[cluster]; }

    // List of clusters to skip on a bcast.  Nominally this is just one, but we
    // can iterate thorugh it and mask out arbitrary clusters.
    const std::set<int> &get_bcast_skip_list() const { return bcast_skip_list; }
    void add_bcast_skip_cluster(int cluster) { bcast_skip_list.insert(cluster); }

    // Set/get the valid bit for a broadcast response.  If the line was valid in
    // the L2 when the bcast got there, the bit is sent by the response.
    // Otherwise, the bit is clear and the line is not present in the
    // responder's L2.
    bool get_bcast_resp_valid() const { return bcast_was_valid; }
    void set_bcast_resp_valid() { bcast_was_valid = true; }
    void clear_bcast_resp_valid() { bcast_was_valid = false; }

    // The list of bcast_resp's should replace the bcast_resp bit.  Currently,
    // when using splitting BCAST's, this is what will track whether the new
    // sharing std::vector should include this cluster or not.
    void add_bcast_resp_sharer(int cid) { bcast_resp_list.insert(cid); }
    bool get_bcast_resp_valid_list(int cid) { return (bcast_resp_list.count(cid) > 0); }

    uint32_t get_bcast_write_shootdown_addr() const { return bcast_write_shootdown_addr; }
    uint32_t get_bcast_write_shootdown_owner() const { return bcast_write_shootdown_owner; }
    void set_bcast_write_shootdown(int o, uint32_t a); 
    int get_bcast_write_shootdown_required() const { return bcast_write_shootdown_owner; }
  
  public: /* STATIC METHODS */
    // Return true if the instruction should not be cached at the C$
    static bool check_can_ccache_fill(icmsg_type_t msg_type);
    // Check if a request is allowed to fill into the global cache.
    static bool check_can_gcache_fill(icmsg_type_t msg_type);
    // Return true if the message writes memory (non-exclusive of reads).
    static bool check_is_write(icmsg_type_t msg_type);
    // Return true if the message reads memory (non-exclusive of writes).
    static bool check_is_read(icmsg_type_t msg_type);
    // Return true if message is a coherence probe/ack/nack.
    static bool check_is_coherence(icmsg_type_t msg_type) ;
    static bool check_is_global_operation(icmsg_type_t msg_type);
    static bool check_is_writeback_evict(icmsg_type_t msg_type);
    static bool check_is_bcast_req(icmsg_type_t msg_type);
    static icmsg_type_t bcast_notify_convert(icmsg_type_t t);

  public: /* DATA */
    // Used by overflow directory to model timing.  The bit is set when the
    // directory fills the dir_timer field with data.
    bool gcache_dir_proxy_accesses_complete;
    bool gcache_dir_proxy_accesses_pending;
    // Timers for stat collection.  These are handled outside of the ICMsg class.
    SimTimer timer_network_occupancy, timer_memory_wait;
  
  private: /* DATA */
    // Each message is one way and its type is constant at the time of construction
    icmsg_type_t type;
    //G$ Request state machines
    std::vector<GCacheRequestStateMachine> state_machines;
    // The data going up to the global cache/memory
    uint8_t data[rigel::cache::LINESIZE];
    // On writes, which bytes should be updated.  Zero's are ignored and ones are
    // written to the global cache/memory
		std::bitset<rigel::cache::LINESIZE> dirtybits;
    // The inserted cycle is used to track the time it takes a packet to traverse
    // the network.
    uint64_t cycle_inserted;
    // Requesting cluster number for doing routing
    int cluster_num;
    // Requesting core number used for atomic operations.  Set to -1 if invalid
    int _core_id;
    // Requesting thread id, set to -1 for invalid
    int _tid;
    // Index used to find the request in the pendingMisses array at the cluster cache
    int pending_index;
    // Each message gets a unique number for debugging purposes.  The counter is incremented
    // everytime a new ICMsg is constructed.
    static uint64_t GLOBAL_MSG_NUMBER;
    uint64_t msg_number;
    // Did this hit in the global cache (L3).
    bool gcache_hit;
    // Set when checking IsValidLine() in the G$ is not sufficient to know
    // whether or not to turn around this ICMsg.  This is the case for NGA
    // requests and writes.
    bool memaccess_done;
    // Number of cores we have sent a broadcast to so far
    int bcast_count;
    // Cycle where the GCache pends the request.
    uint64_t pend_set_cycle;
    // Cycle where the GlobalCache::PerCycle() method last touches the message.
    uint64_t gcache_last_touch;
    // Pointer imer structure used by coherence.
    directory::dir_of_timing_t *dir_timer;
    // Sequence numbers between G$/C$ pair
    seq_num_t seq_number;
    // The logical channel being used by the request.
    int seq_channel;
    // Set by the directory if the L2 should cache this as an incoherent line.
    bool incoherent;
    // Used by network model to determine when a message can move to the next
    // level of the interconnect.
    uint64_t ready_cycle;
    // Bits used to track directory cache hits and misses.
    bool dir_access, dir_hit;
    // Bits used to track global cache hits and misses.
    bool global_cache_access, global_cache_hit;
    // Used to signal that the addresses in this message have been pended at the G$.
    bool global_cache_pended;
    // Sequence numbers between G$/C$ pair for broadcasts. FIXME: Turn into arrays?
    std::map<int, seq_num_t> bcast_seq_number;
    // The logical channel being used by the request for broadcasts.
    std::map<int, int> bcast_seq_channel;
    // The list of clusters to skip on a bcast.
    std::set<int> bcast_skip_list;
    // Bit is set by a broadcast reply if the line is valid.  Only used by probe-filter.
    bool bcast_was_valid;
    // TODO: The set should replace the bit.
    std::set<int> bcast_resp_list;
    // For probe filtering directories, we maintain the invariant that dirty
    // lines are never left in an L2.  To do that, we tag along the address of
    // the line we are 'dropping' to start a BCAST with the BCAST to force the
    // owning cluster to writeback the line.  We could handle it with a
    // separate message, but this is a bit more elegant.
    uint32_t bcast_write_shootdown_addr;
    int bcast_write_shootdown_owner;
    bool bcast_write_shootdown_required;
};

inline bool
ICMsg::check_can_ccache_fill(icmsg_type_t msg_type)
{
  switch (msg_type)
  {
    case (IC_MSG_ATOMXCHG_REQ):
    case (IC_MSG_ATOMCAS_REQ):
    case (IC_MSG_ATOMADDU_REQ):
    case (IC_MSG_ATOMMAX_REQ):
    case (IC_MSG_ATOMMIN_REQ):
    case (IC_MSG_ATOMOR_REQ):
    case (IC_MSG_ATOMAND_REQ):
    case (IC_MSG_ATOMXOR_REQ):
    case (IC_MSG_ATOMINC_REQ):
    case (IC_MSG_ATOMDEC_REQ):
    // Make sure that when the cache controller sees the REPLY message
    // come back, it does not fill it into the cluster cache
    case (IC_MSG_GLOBAL_READ_REQ):
    case (IC_MSG_GLOBAL_WRITE_REQ):
    case (IC_MSG_LINE_WRITEBACK_REQ):
    case (IC_MSG_BCAST_INV_REQ):
    case (IC_MSG_INSTR_READ_REQ):
    case (IC_MSG_BCAST_UPDATE_REQ):
      // If we get a message like this, it means the C$ does NOT
      // cache instructions, and this should not fill into the C$
      return false;
    default:
      return true;
  }
}

inline bool
ICMsg::check_is_read(icmsg_type_t msg_type)
{
  switch (msg_type)
  {
    case IC_MSG_READ_REQ:                   // 1
    case IC_MSG_READ_REPLY:                 // 2
    case IC_MSG_GLOBAL_READ_REQ:            // 9
    case IC_MSG_GLOBAL_READ_REPLY:          // 10
    case IC_MSG_PREFETCH_REQ:               // 21
    case IC_MSG_CCACHE_HWPREFETCH_REQ:      // 37
    case IC_MSG_GCACHE_HWPREFETCH_REQ:      // 39
    case IC_MSG_PREFETCH_REPLY:             // 22
    case IC_MSG_PREFETCH_BLOCK_GC_REQ:      // 25
    case IC_MSG_PREFETCH_BLOCK_GC_REPLY:    // 26
    case IC_MSG_PREFETCH_BLOCK_CC_REQ:      // 27
    case IC_MSG_PREFETCH_BLOCK_CC_REPLY:    // 28
    case IC_MSG_CCACHE_HWPREFETCH_REPLY:    // 38
    case IC_MSG_GCACHE_HWPREFETCH_REPLY:    // 40
    case IC_MSG_PREFETCH_NGA_REQ:           // 23
    case IC_MSG_PREFETCH_NGA_REPLY:         // 24
    case IC_MSG_INSTR_READ_REQ:             // 35
    case IC_MSG_INSTR_READ_REPLY:           // 36
      return true;
    default:
      return false;
  }
}


inline bool
ICMsg::check_is_writeback_evict(icmsg_type_t msg_type)
{
  switch (msg_type)
  {
    case IC_MSG_EVICT_REQ:            // 39
    case IC_MSG_EVICT_REPLY:          // 40
    case IC_MSG_LINE_WRITEBACK_REQ:   // 7
    case IC_MSG_LINE_WRITEBACK_REPLY: // 8
      return true;
    default:
      return false;
  }
}

inline bool
ICMsg::check_is_bcast_req(icmsg_type_t msg_type)
{
  switch (msg_type)
  {
    case IC_MSG_BCAST_UPDATE_REQ:
    case IC_MSG_BCAST_INV_REQ:
      return true;
    default:
      return false;
  }
}

inline bool
ICMsg::check_is_write(icmsg_type_t msg_type)
{
  switch (msg_type)
  {
    case IC_MSG_WRITE_REQ:            // 3
    case IC_MSG_WRITE_REPLY:          // 4
    // Note that we do not want to treat WRITEBACKS/EVICTIONS/FLUSHES as
    // "writes" from the perspective of the
//    case IC_MSG_EVICT_REQ:            // 39
//    case IC_MSG_EVICT_REPLY:          // 40
//    case IC_MSG_LINE_WRITEBACK_REQ:   // 7
//    case IC_MSG_LINE_WRITEBACK_REPLY: // 8
    case IC_MSG_GLOBAL_WRITE_REQ:     // 11
    case IC_MSG_GLOBAL_WRITE_REPLY:   // 12
    case IC_MSG_ATOMXCHG_REQ:         // 13
    case IC_MSG_ATOMXCHG_REPLY:       // 14
    case IC_MSG_ATOMDEC_REQ:          // 15
    case IC_MSG_ATOMDEC_REPLY:        // 16
    case IC_MSG_ATOMINC_REQ:          // 17
    case IC_MSG_ATOMINC_REPLY:        // 18
    case IC_MSG_ATOMCAS_REQ:          // 19
    case IC_MSG_ATOMCAS_REPLY:        // 20
    case IC_MSG_ATOMADDU_REQ:         // 21
    case IC_MSG_ATOMADDU_REPLY:       // 22
    case IC_MSG_BCAST_UPDATE_REQ:     // 27
    case IC_MSG_BCAST_UPDATE_REPLY:   // 28
    case IC_MSG_BCAST_UPDATE_NOTIFY:  // 29
    case (IC_MSG_ATOMMAX_REQ):
    case (IC_MSG_ATOMMIN_REQ):
    case (IC_MSG_ATOMOR_REQ):
    case (IC_MSG_ATOMAND_REQ):
    case (IC_MSG_ATOMXOR_REQ):
    case (IC_MSG_ATOMMAX_REPLY):
    case (IC_MSG_ATOMMIN_REPLY):
    case (IC_MSG_ATOMOR_REPLY):
    case (IC_MSG_ATOMAND_REPLY):
    case (IC_MSG_ATOMXOR_REPLY):
      return true;
    default:
      return false;
  }
}

inline void 
ICMsg::set_bcast_write_shootdown(int o, uint32_t a) 
{ 
  bcast_write_shootdown_owner = o; 
  bcast_write_shootdown_addr = a; 
  bcast_write_shootdown_required = true; 
}

inline bool
ICMsg::check_can_gcache_fill(icmsg_type_t msg_type)
{
  switch (msg_type)
  {
    case IC_MSG_PREFETCH_NGA_REQ:           // 23
    case IC_MSG_PREFETCH_NGA_REPLY:         // 24
      return false;
    default:
      return true;
  }
}

inline bool
ICMsg::check_is_coherence(icmsg_type_t msg_type)
{
  switch (msg_type)
  {
    case IC_MSG_CC_INVALIDATE_PROBE:           // 41
    case IC_MSG_CC_INVALIDATE_ACK:             // 42
    case IC_MSG_CC_INVALIDATE_NAK:             // 43
    case IC_MSG_CC_WR_RELEASE_PROBE:           // 44
    case IC_MSG_CC_WR_RELEASE_ACK:             // 45
    case IC_MSG_CC_WR_RELEASE_NAK:             // 46
    case IC_MSG_CC_RD_RELEASE_PROBE:           // 47
    case IC_MSG_CC_RD_RELEASE_ACK:             // 48
    case IC_MSG_CC_RD_RELEASE_NAK:             // 49
    case IC_MSG_CC_WR2RD_DOWNGRADE_PROBE:      // 50
    case IC_MSG_CC_WR2RD_DOWNGRADE_ACK:        // 51
    case IC_MSG_CC_WR2RD_DOWNGRADE_NAK:        // 52
    case IC_MSG_CC_RD_RELEASE_REQ:             // 53
    case IC_MSG_CC_RD_RELEASE_REPLY:           // 54
    case IC_MSG_CC_BCAST_PROBE:                // 57
    case IC_MSG_CC_BCAST_ACK:                  // 58
    case IC_MSG_SPLIT_BCAST_INV_REQ:        
    case IC_MSG_SPLIT_BCAST_SHR_REQ:        
    case IC_MSG_SPLIT_BCAST_OWNED_REPLY:
    case IC_MSG_SPLIT_BCAST_INV_REPLY:
    case IC_MSG_SPLIT_BCAST_SHR_REPLY:        
        return true;
    // For coherence only:
    case IC_MSG_EVICT_REQ:            // 39
    case IC_MSG_EVICT_REPLY:          // 40
    case IC_MSG_LINE_WRITEBACK_REQ:   // 7
    case IC_MSG_LINE_WRITEBACK_REPLY: // 8
      // When we are coherent, we need to treat writebacks as coherence traffic
      // from the perspective of the network.
      if (rigel::ENABLE_EXPERIMENTAL_DIRECTORY) {
        return true;
      } else {
        return false;
      }
    default:
      return false;
  }
}

inline bool
ICMsg::check_is_global_operation(icmsg_type_t msg_type)
{
  switch (msg_type)
  {
    case IC_MSG_GLOBAL_READ_REQ:
    case IC_MSG_GLOBAL_READ_REPLY:
    case IC_MSG_GLOBAL_WRITE_REQ:
    case IC_MSG_GLOBAL_WRITE_REPLY:
    case IC_MSG_ATOMXCHG_REQ:
    case IC_MSG_ATOMXCHG_REPLY:
    case IC_MSG_ATOMDEC_REQ:
    case IC_MSG_ATOMDEC_REPLY:
    case IC_MSG_ATOMINC_REQ:
    case IC_MSG_ATOMINC_REPLY:
    case IC_MSG_ATOMCAS_REQ:
    case IC_MSG_ATOMCAS_REPLY:
    case IC_MSG_ATOMADDU_REQ:
    case IC_MSG_ATOMADDU_REPLY:
    case (IC_MSG_ATOMMAX_REQ):
    case (IC_MSG_ATOMMIN_REQ):
    case (IC_MSG_ATOMOR_REQ):
    case (IC_MSG_ATOMAND_REQ):
    case (IC_MSG_ATOMXOR_REQ):
    case (IC_MSG_ATOMMAX_REPLY):
    case (IC_MSG_ATOMMIN_REPLY):
    case (IC_MSG_ATOMOR_REPLY):
    case (IC_MSG_ATOMAND_REPLY):
    case (IC_MSG_ATOMXOR_REPLY):
      return true;
    default:
      return false;
  }
}

inline void 
ICMsg::set_bcast_seq(int cluster, int seq_chan, seq_num_t seq_num)
{
  bcast_seq_channel[cluster] = seq_chan;
  bcast_seq_number[cluster] = seq_num;
}

#endif


