//////////////////////////////////////////////////////////////////////////////// 
//  directory.h
////////////////////////////////////////////////////////////////////////////////
//
//  Contains definition of classes needed for the hardware coherence mechanism.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __DIRECTORY_H__
#define __DIRECTORY_H__
#include "sim.h"
#include "interconnect.h"
#include "memory/address_mapping.h"
#include <set>
#include <list>
#include "util/debug.h"
#include "util/dynamic_bitset.h"
#include "profile/profile.h"
#include <ext/hash_map>
#include <queue>
using namespace __gnu_cxx; //For hash_map


class OverflowCacheDirectory;
class CacheDirectory;
namespace directory 
{
  // Structure with configuration information for the entire directory.  It is a
  // static member of the directory.
  struct dir_config_t 
  {
    // Number of sharers before broadcast/shootdown/exception occurs.  If
    // BITVEC_SIZE == NUM_CLUSTERS, it is a full directory.
    size_t bitvec_size;
    // Number of entries in this directory, i.e., how many lines can be in
    // flight at one time.  TODO: If we overflow the directory...what do we do!?
    size_t num_dir_entries;
    // Set whether W->W transfers updated the G$.  This is a performance issue.
    bool w2w_updates_gcache;
    // How associative is the directory?
    size_t num_ways;
    // How many sets are there?  Note sets*wasy == size.
    size_t num_sets;
    // How many head pointers for overflow linked lists are there?
    size_t num_overflow_lists;
    // How many pointers do we allow in the limited directory scheme before
    // resorting to a broadcast?
    size_t num_limited_pointers;
  };

  // States that a directory entry can be in.
  typedef enum {
    DIR_STATE_INVALID = 0,
    // Entry is held in the READ state by one or more caches.
    DIR_STATE_READ_SHARED,
    // Entry is writeable by a single cache.  Data in G$ is assumed dirty.
    DIR_STATE_WRITE_PRIV,
    // Read request in progress.  Waiting on invalidates to get back from
    // private caches.
    DIR_STATE_PENDING_READ,
    // Write request in progress.  Waiting on write update if it was held as
    // WRITE.  Waiting on invalidates if held as read.
    DIR_STATE_PENDING_WRITE,
    // Waiting on an invalidate/writeback for a line held be a cluster in the
    // write/private state.
    DIR_STATE_PENDING_WB,
    // Pending evictions get locked.  No new readers or writers can access the
    // line until it is replaced.
    DIR_STATE_EVICTION_LOCK_READ,
    DIR_STATE_EVICTION_LOCK_WRITE,
    // States used for insertion/removal from the overflow direectory.
    DIR_STATE_OVERFLOW_INSERT_PENDING,
    DIR_STATE_OVERFLOW_LOOKUP_PENDING,
    // States used for broadcast transactions.
    DIR_STATE_PENDING_READ_BCAST,
    DIR_STATE_PENDING_WRITE_BCAST,
    DIR_STATE_EVICTION_LOCK_READ_BCAST
  } dir_state_t;

  const char *const dir_state_string[] = 
  {
    "DIR_STATE_INVALID",
    "DIR_STATE_READ_SHARED",
    "DIR_STATE_WRITE_PRIV",
    "DIR_STATE_PENDING_READ",
    "DIR_STATE_PENDING_WRITE",
    "DIR_STATE_PENDING_WB",
    "DIR_STATE_EVICTION_LOCK_READ",
    "DIR_STATE_EVICTION_LOCK_WRITE",
    "DIR_STATE_OVERFLOW_INSERT_PENDING",
    "DIR_STATE_OVERFLOW_LOOKUP_PENDING",
    "DIR_STATE_PENDING_READ_BCAST",
    "DIR_STATE_PENDING_WRITE_BCAST",
    "DIR_STATE_EVICTION_LOCK_READ_BCAST",
  };

  // Each element in a directory is tracked by a dir_entry_t.
  struct dir_entry_t 
  {
    // Default constructor.  Note the max_sharers is initialized to -1 so we can
    // hook reset() to do stats gathering without muddying the waters with this
    // initial reset() calls or needing to pull out a third init-type function.
    dir_entry_t() : sharer_vec(rigel::NUM_CLUSTERS), valid_bits(NULL), max_sharers(-1) { } 
    //reset() must go in init
    // Overloaded assignment operator.
    dir_entry_t& operator=(const dir_entry_t &other);

    public:
      // Clear the directory entry.
      inline void reset();
      // Dump the state of the entry.
      inline void dump() const;
      // Initialize the entry so it knows how to modify the directory-wide valid bitset
      void init(DynamicBitset *bs, size_t id) { valid_bits = bs; valid_bit_id = id; reset(); }
      // Return the address of the entry.
      uint32_t get_addr() const { return addr & ~(rigel::cache::LINESIZE-1); }
      // Set the address for the entry.
      // FIXME For now, we hook this function to set the allocation time,
      // because currently set_addr() is only called when a new entry is
      // allocated.  In future, we might want to have a separate function
      // explicitly for allocating an entry, and set the allocation cycle there.
      void set_addr(uint32_t in_addr) { allocation_cycle = rigel::CURR_CYCLE;
                                        addr = (~(rigel::cache::LINESIZE-1) & in_addr); }
      // Return true if the input address matches the stored address.
      inline bool check_addr_match(uint32_t req_addr);
      // Dump the sharer vector to the specified stream, always in hex
      // regardless of sharer vector size
      void dump_sharer_vector_hex(FILE *stream) { sharer_vec.print(stream, true); }
      // Dump the sharer vector to the specified stream, in binary if nClusters <= 64
      void dump_sharer_vector(FILE *stream) { sharer_vec.print(stream); }
      // Return the number of sharers of a line
      uint32_t get_num_sharers() const { return sharer_vec.getNumSetBits(); }
      // Adjust the number of sharers to a line.
      void inc_sharers(int cl_id);
      void dec_sharers(int cl_id) { sharer_vec.clear(cl_id); }
      void clear_sharers() { sharer_vec.clearAll(); }
      void set_all_sharers() { sharer_vec.setAll(); }
      // A broadcast is going to occur.  Set all sharers except the requesting
      // cluster.
      void set_sharers_bcast(int cl_id) { set_sharers_bcast(); sharer_vec.clear(cl_id); }
      void set_sharers_bcast() { set_all_sharers(); }
      // Return the state of the entry.
      directory::dir_state_t get_state() const { return state; }
      // Set the state of the entry.
      void set_state(directory::dir_state_t s) {
        //Flip the valid bit if we are going to/from DIR_STATE_INVALID
        if((state == DIR_STATE_INVALID) != (s == DIR_STATE_INVALID))
          if(valid_bits != NULL)
            valid_bits->toggle(valid_bit_id);
        state = s;
      }
      // Return the last cycle for which this line was accessed.
      uint64_t get_last_touch() const { return last_touch; }
      // Return the number of touches over this entry's lifetime
      int get_num_touches() const { return num_touches; }
      // set the last touch time.
      void set_last_touch() { last_touch = rigel::CURR_CYCLE; num_touches++; }
      // Check if the cluster is currently sharing the line.
      bool check_cluster_is_sharing(int cluster) const { return sharer_vec.test(cluster); }
      // Return true if the entry is valid.
      bool check_valid() const { return (state != DIR_STATE_INVALID); }
      // Return the (write) owner of the entry.  Aborts if multiple/no owners
      // present.  Also forces the line to be write-owned when called.
      int get_owner() const;
      // Set/Return the waiting cluster
      int get_waiting_cluster() const { return waiting_cluster; }
      void set_waiting_cluster(int cid) { waiting_cluster = cid; }
      // We set a bit when access to an entry is granted that is not cleared
      // until the G$ can respond with the data.
      void clear_fill_pending(int cluster_num) { 
        if (0 == fill_pending.erase(cluster_num)) { 
          rigel::GLOBAL_debug_sim.dump_all();
          dump();
          assert(0 && "Trying to clear fill pending bit that was never set."); 
        }
      }
      void set_fill_pending(int cluster_num) { 
        if (0 < fill_pending.count(cluster_num)) { 
          rigel::GLOBAL_debug_sim.dump_all();
          dump();
          assert(0 && "Trying to set fill pending bit that was set."); 
        }
        fill_pending.insert(cluster_num) ; 
      }
      bool check_fill_pending() const { return !fill_pending.empty(); }
      bool check_bcast() const { return bcast_set; }
      // Lock the entry until the pending request gets data.
      void set_atomic_lock(int cluster) { lock_bit = true; }
      void clear_atomic_lock(int cluster) { lock_bit = false; }
      bool check_atomic_lock(int cluster) const { return lock_bit; }
      // When the probe filter is enabled, we need to track the state and sharer
      // to update the directory entry to when the bcasts finish.
      void set_pf_bcast_next(dir_state_t t, int h) { pf_bcast_next_state=t; pf_bcast_next_sharer= h;}
      dir_state_t get_pf_bcast_next_state() const { return pf_bcast_next_state; }
      int get_pf_bcast_next_sharer() const { return pf_bcast_next_sharer; }
      // Called either at the end of the simulation or when the entry is evicted.
      void gather_end_of_life_stats() const;
    public: /* XXX: Be sure to update overloaded assignment operator!! */
      // Address of the line being tracked.  The LINESIZE_ORDER low-order bits
      // are always assumed to be zeroed.
      uint32_t addr;
      // Array of sharers
      DynamicBitset sharer_vec;
      // Each entry can be of a configurable size to allow for region tracking.
      // The size is measured in bytes.
      size_t entry_size;
      // The last cycle the entry was accessed.
      uint64_t last_touch;
      // The cycle the entry was allocated.
      uint64_t allocation_cycle;
      // Save the cluster that is waiting for the line. 
      int waiting_cluster;
      // Save the list of NAKs for matching NAKS/RELEASE messages on an
      // invalidate request from the directory
      std::set<int> nak_waitlist;
      std::set<int> fill_pending;
      // Track the sharers as part of the BCAST reply that hold the line.  Used
      // by the probe filter directory.
      std::set<int> bcast_valid_sharers;
      // Set when a broadcast must occur.
      bool bcast_set;
      // Denotes perm->access window is open.
      bool lock_bit;
      // Next state and sharer after the bcast finishes for probe filtering.
      dir_state_t pf_bcast_next_state;
      int pf_bcast_next_sharer;
    private:
      // State of the line INV/READ/WRITER.  We want this to be private so we can hook
      // set_state() and get_state() to manipulate the dense bit array
      directory::dir_state_t state;
      // This is a directory-wide bitset.  We want a way to call into it to set/clear our valid bit
      // when set_state() or reset() is called on us directly.  When we are initialized, we should be
      // passed an ID (valid_bit_id) to use as our bit number when we call valid_bits functions.
      DynamicBitset *valid_bits;
      size_t valid_bit_id;
      // Number of times this entry has been touched over its lifetime.  May be
      // useful in the replacement policy, for now we want it for eviction
      // dumping.  Update: Now used in LFU and LU replacement policies.
      int num_touches;
      // Maximum number of sharers this entry has had over its lifetime.
      int max_sharers;
  };
  // Defined in overflow_directory.h
  struct dir_of_timing_t;
}
// End of namespace 'directory'.


/// CacheDirectory
/// Base class for directory.  There is one directory per global cache bank which
/// services coherence requests from that bank.  Re
class CacheDirectory
{
  public:
    // Default constructor.
    CacheDirectory();
    // Initialize the directory.  Bank ID of the GCache associated with the
    // directory.
    void init(int bank_id);
    // Called every cycle.  For now, just use it to gather statistics.
    void PerCycle();
    // Attempt to gain read permission to a line.  Returns true when read
    // permission is obtained.
    bool request_read_permission(uint32_t req_addr, int req_clusterid, 
      directory::dir_of_timing_t &timer);
    // Helper that handles the state machine for request_read permission.
    bool helper_request_read_permission(uint32_t req_addr, int req_clusterid, 
      directory::dir_entry_t &entry);
    // Return true if read permsion is held by the cluster.
    bool check_read_permission(uint32_t req_addr, int req_clusterid, 
      directory::dir_of_timing_t &timer);
    // Return true if there is already a request for the addr/cluster pair
    // pending.
    bool check_request_pending(uint32_t req_addr);
    // Attempt to gain write permission to a line.  If read permission is held,
    // this is an upgrade.  Returns true when write permission is obtained.
    bool request_write_permission(uint32_t req_addr, int req_clusterid, 
      directory::dir_of_timing_t &timer);
    // Downgrade from write-only permission to read permission.
    void write_to_read_perm_downgrade(uint32_t req_addr, int req_clusterid, 
      directory::dir_of_timing_t &timer);
    // Helper that handles the state machine for write_request permission.
    bool helper_request_write_permission(uint32_t req_addr, int req_clusterid, 
      directory::dir_entry_t &entry);
    // Return true if write permsion is held by the cluster.
    bool check_write_permission(uint32_t req_addr, int req_clusterid, 
      directory::dir_of_timing_t &timer);
    // Relinquish read ownership of a line.  Invalidates generate this.
    void release_read_ownership(uint32_t rel_addr, int rel_clusterid, icmsg_type_t ic_type, 
      directory::dir_of_timing_t &timer, bool bcast_found_valid_line = false);
    void helper_release_read_ownership(uint32_t rel_addr, int rel_clusterid, icmsg_type_t ic_type,
      directory::dir_entry_t &entry, bool bcast_found_valid_line = false);
    // Relinquish write ownership with data.  Writebacks generate this.  Only
    // call once data is resident in the G$ (FIXME: maybe not if we want W->W to
    // bypass the G$)
    void release_write_ownership(uint32_t rel_addr, int rel_clusterid, icmsg_type_t ic_type, 
      directory::dir_of_timing_t &timer);
    void helper_release_write_ownership(uint32_t rel_addr, int rel_clusterid, icmsg_type_t ic_type,
      directory::dir_entry_t &entry);
    // Initiate a probe filter broadcast.
    void helper_probe_filter_bcast( uint32_t addr, size_t next_sharer, directory::dir_state_t next_state);
    // Get the next message waiting to be injected into the network from the
    // directory toward the clusters.  Generally this includes invalidation
    // requests.  Returns false if no messages are pending.
    bool get_next_pending_icmsg(ICMsg &msg);
    // Remove the last entry returned by get_next_pending_icmsg().  The two
    // should be executed in pairs.
    void remove_next_pending_icmsg();
    // Add a message to the list.  Returns false if message could not be inserted.
    bool insert_pending_icmsg(ICMsg &msg);
    // Print the state of the directory (DEBUG)
    void dump();
    // Print the sharing vector for a line for debugging purposes.
    void DEBUG_print_sharing_vector(uint32_t addr, FILE *stream);
    // Clear the fill_pending bit for the line.  Only called after a reply is
    // successfully injected into the network.  The fill_pending bit is set on
    // a successful ownership request. 
    void clear_fill_pending(uint32_t req_addr, int req_clusterid);
    // Is this line being tracked in the directory?
    bool is_present(uint32_t req_addr) const;
    // Is this line present in the overflow directory?
    // Will always return false is overflow is not enabled
    bool is_present_in_overflow(uint32_t req_addr) const;
    // Update the logical channel information used to order requests through the
    // network in the limited directory scheme.
    void update_logical_channel(int cluster_id, int channel_num);
    // Open a permission -> access window and ensure atomicity.
    void set_atomic_lock(uint32_t addr, int cluster);
    void clear_atomic_lock(uint32_t addr, int cluster);
    bool check_atomic_lock(uint32_t addr, int cluster) const;
    size_t get_set(uint32_t addr) const;
    size_t get_way(uint32_t addr) const;
    // Gather statistics on the entries left in the directory at the end of the simulation
    void gather_end_of_simulation_stats() const;
    // Return true if we have a free entry in the tile-level bcast structures.
    bool bcast_network_entry_available() const;

  private:  /* PRIVATE DATA */
    // Configuration information for the directory.  TODO: May make static?
    directory::dir_config_t config;
    // Array of directory entries for the directory. Initialized in the
    // constructor to config.num_dir_entries.
    directory::dir_entry_t *dir_array;
    // Queue of ICMsg's that need to be sent back to the clusters.  Accessed
    // strictly in-order.
		std::queue<ICMsg> pending_msg_q;
    // Bitvector and list of entries that are currently undergoing eviction.
    // Evictions are atomic at each directory in that only one is in progress at
    // any given time.
    bool *dir_pending_replacement_bitvec;
    int *dir_pending_replacement_way;
    uint32_t *dir_pending_replacement_aggressor_address;
    uint32_t gcache_bank_id;
    // The L2 Overflow directory.
    OverflowCacheDirectory *overflow_directory;
    // The set of invalidates that have been performed as a result of evictions
    // (to track rerequests). Holds a uint64_t which is composed of {address, cluster number}.
    // Only use add_eviction_invalidate() to add to this set, and remove_eviction_invalidate()
    // to remove from it.
    hash_map<uint32_t, DynamicBitset *> eviction_invalidates;
    // The sequence numbers for cluster/gcache pairings.
    seq_num_t *seq_num;
    // Separate valid bits for all directory entries.
    // The densification should help out cache behavior quite a bit.
    DynamicBitset valid_bits;
    // A set of pointers to directory entries with an outstanding
    // coherence-related broadcast.
    // For now, only used for statistics gathering.
    std::set<directory::dir_entry_t *> pending_bcasts;
    // The current count of outstanding broadcasts.
    int outstanding_bcasts;

  private: /* PRIVATE METHODS */
    // Return the set that the address would map to.
    //size_t get_set(uint32_t addr) const;
    // Return the way that the address *does* map to.  On failure, throws
    // exception.
    //size_t get_way(uint32_t addr) const;
    // Internal accessor method for returning an entry within a way.
    directory::dir_entry_t & get_entry(size_t set, size_t way) const;
    // Return an entry to replace.  'success' is true if replacement succeded with
    // the way filled in or false if it must stall.  Call handles replacement
    // policy (LRU, random, etc.).  Will return invalid entries first.
    directory::dir_entry_t & 
      replace_entry(uint32_t addr, bool &success, directory::dir_of_timing_t &timer);
    // Return a reference to the entry in the set with the fewest touches per cycle since allocation.
    directory::dir_entry_t & get_least_frequently_touched_entry(size_t set);
    // Return a reference to the entry in the set with the fewest touches since allocation.
    directory::dir_entry_t & get_least_touched_entry(size_t set);
    // Return a reference to the least recently touched entry in the set.
    directory::dir_entry_t & get_lru_entry(size_t set);
    // Return a reference to the least recently allocated in the set.
    directory::dir_entry_t & get_lra_entry(size_t set);
    // Return a reference to the entry with the fewest sharers in the set (prioritizing read-shared
    // lines above write-shared lines).
    directory::dir_entry_t & get_least_shared_entry(size_t set);
    // Return a reference to the entry selected for replacement, based on our replacement policy.
    directory::dir_entry_t & get_replacement_entry(size_t set);
    bool was_invalidated_early(uint32_t addr, unsigned int cluster);
    void add_eviction_invalidate(uint32_t addr, unsigned int cluster);
    void remove_eviction_invalidate(uint32_t addr, unsigned int cluster);
    // Calculate number of code, stack, other valid directory entries.
    void get_num_valid_entries(uint64_t &code, uint64_t &stack, uint64_t &other, uint64_t &invalid);
    // Sample the distribution of entries among different code segmenets
    void gather_statistics();
    // Get number of outstanding broadcasts
    size_t get_num_outstanding_bcasts() { return pending_bcasts.size(); }
    // Add outstanding broadcast
    void add_outstanding_bcast(directory::dir_entry_t *entry) {
      assert (outstanding_bcasts >= 0);
      assert(pending_bcasts.count(entry) == 0);
      pending_bcasts.insert(entry);
      outstanding_bcasts++;
    }
    void remove_outstanding_bcast(directory::dir_entry_t *entry) {
      assert (outstanding_bcasts > 0);
      assert(pending_bcasts.count(entry) == 1);
      pending_bcasts.erase(entry);
      outstanding_bcasts--;
    }
};

namespace directory
{
  inline int
  dir_entry_t::get_owner() const
  { 
    // Error on accesses without ownership and >1 owner.
    if (DIR_STATE_WRITE_PRIV != get_state()) { 
      DEBUG_HEADER();
      fprintf(stderr, "Calling get_owner() when line is not held as WRITE_PRIV.\n");
      DEBUG_HEADER(); 
      dump();
      assert(0);
    }
    if (1 != get_num_sharers()) {
      DEBUG_HEADER();
      fprintf(stderr, "Calling get_owner() when line is held by != one sharer.\n");
      DEBUG_HEADER(); 
      dump();
      assert(0);
    }

    int i = sharer_vec.findFirstSet();
    if(i != -1)
      return i;
    // At least one cluster *must* be found.
    abort();
    return -1;
  }

  inline void
  dir_entry_t::dump()  const
  {
    fprintf(stderr, "dir_entry_t: ");
    fprintf(stderr, "addr: %08x ", get_addr());
    fprintf(stderr, "bcast: %01d ", check_bcast());
    fprintf(stderr, "num sharers: %4d ", get_num_sharers());
    fprintf(stderr, "waiting_cluster: %3d ", get_waiting_cluster());
    fprintf(stderr, "bitvector: "); sharer_vec.print(stderr); fprintf(stderr, " ");
    fprintf(stderr, "state: %16s ", dir_state_string[get_state()]);
    fprintf(stderr, "last_touch: 0x%" PRIx64 " ", get_last_touch());
    fprintf(stderr, "fill_pending: %1d ", check_fill_pending());
    fprintf(stderr, "pf_next_state: %16s ", dir_state_string[get_pf_bcast_next_state()]);
    fprintf(stderr, "pf_next_sharer: %3d ", get_pf_bcast_next_sharer());
    if (DIR_STATE_WRITE_PRIV == get_state()) {
      fprintf(stderr, "owner: %4d ", get_owner());
    }
    fprintf(stderr, "nak_waitlist: ");
    for (std::set<int>::iterator iter = nak_waitlist.begin(); 
          iter != nak_waitlist.end(); iter++) {
      fprintf(stderr, "%3d ", *iter);
    }
    fprintf(stderr, "\n");
  }

  inline dir_entry_t& 
  dir_entry_t::operator=(const dir_entry_t &other)
  {
    if (this == &other) return (*this);
    
    addr = other.addr;
    sharer_vec = other.sharer_vec;
    entry_size = other.entry_size;
    last_touch = other.last_touch;
    waiting_cluster = other.waiting_cluster;
    nak_waitlist = other.nak_waitlist;
    fill_pending = other.fill_pending;
    bcast_set = other.bcast_set;
    lock_bit = other.lock_bit;
    pf_bcast_next_state = other.pf_bcast_next_state;
    pf_bcast_next_sharer = pf_bcast_next_sharer;
    // DO NOT CHANGE BIT ID OR VALID_BITS POINTER
    // Flip the valid bit if we are going to/from DIR_STATE_INVALID
    set_state(other.state);

    return (*this);
  }

  inline void
  dir_entry_t::reset()
  {
    //Gather statistics if reset() is not being called by the constructor.
    if(max_sharers >= 0) { gather_end_of_life_stats(); }
    addr = 0;
    state = DIR_STATE_INVALID;
    if (valid_bits != NULL) { valid_bits->clear(valid_bit_id); }
    // Default size is the line size of the machine.
    entry_size = rigel::cache::LINESIZE;
    // Reset the directory's bit vector of sharers
    sharer_vec.clearAll();
    // Reset when the entry was last touched to this cycle.
    last_touch = rigel::CURR_CYCLE;
    // Set the waiting cluster to some invalid value.
    waiting_cluster = -1;
    // Clear the NAK set for next eviction/state change.
    nak_waitlist.clear();
    // Every entry has its fill_pending bit set until the response is offered to
    // the cluster(s) requesting.
    fill_pending.clear();
    // No broadcast by default.
    bcast_set = false;
    // Line is unlocked by default.
    lock_bit = false;
    // Clear the probe filter state.
    pf_bcast_next_sharer = -1237;
    pf_bcast_next_state = DIR_STATE_INVALID;
    // The reseter is responsible for clearing the BCAST sharers.
    bcast_valid_sharers.clear();
    // Reset number of touches to 0
    num_touches = 0;
    // Reset max sharers to -1.  This way, if we do 2 reset()s before
    // doing anything to the entry, the stats won't get double-counted.
    max_sharers = -1;
  }

  inline bool 
  dir_entry_t::check_addr_match(uint32_t req_addr)
  {
    return (addr == (req_addr & ~(rigel::cache::LINESIZE-1)));
  }

  inline void 
  dir_entry_t::inc_sharers(int cl_id) 
  { 
    using namespace rigel::cache;

    if (ENABLE_LIMITED_DIRECTORY && sharer_vec.getNumSetBits() >= MAX_LIMITED_PTRS) {
      assert(get_state() == DIR_STATE_READ_SHARED && "BCASTS are only allowed for read data.");
      bcast_set = true; 
    }
    sharer_vec.set(cl_id);
    //If we have more sharers than ever before, increment the max count.
    if((int)sharer_vec.getNumSetBits() > max_sharers) {
      max_sharers = sharer_vec.getNumSetBits();
    }
  }

  // Called either at the end of the simulation or when the entry is evicted.
  inline void
  dir_entry_t::gather_end_of_life_stats() const
  {
    rigel::profiler::stats[STATNAME_DIR_MAX_SHARERS].inc_mem_histogram(max_sharers, 1);
  }
  
}


inline bool 
CacheDirectory::bcast_network_entry_available() const
{
  using namespace rigel;
  using namespace rigel::cache;
  // Only enabled if probe filtering and broadcasting is turned on.
  if (!ENABLE_BCAST_NETWORK) { return true; }
  // Zero means infinite.
  if (0 == CMDLINE_MAX_BCASTS_PER_DIRECTORY) { return true; }
  // We can use up a broadcast.  It will be claimed when we do remove/add
  // oustanding_bcast().
  if (outstanding_bcasts >= CMDLINE_MAX_BCASTS_PER_DIRECTORY) { return false; }
  // All else fails, we let it be.
  return true;
}

inline size_t 
CacheDirectory::get_set(uint32_t addr) const
{
  //Use the same mapping scheme as G$ sets
  return AddressMapping::GetGCacheSet(addr, config.num_sets); 
}

inline bool
CacheDirectory::was_invalidated_early(uint32_t addr, unsigned int cluster)
{
  if(eviction_invalidates.find(addr) == eviction_invalidates.end()) { return false; }
  else { return (eviction_invalidates[addr]->test(cluster)); }
}

inline void
CacheDirectory::add_eviction_invalidate(uint32_t addr, unsigned int cluster)
{
  //If entry for addr does not exist, make a new one
  if(eviction_invalidates.count(addr) == 0)
  {
    DynamicBitset *newEntry = new DynamicBitset(rigel::NUM_CLUSTERS);
    newEntry->set(cluster);
    eviction_invalidates[addr] = newEntry;
  }
  else //If it does exist, set the bit for this cluster
  {
    eviction_invalidates[addr]->set(cluster);
  }
}

inline void
CacheDirectory::remove_eviction_invalidate(uint32_t addr, unsigned int cluster)
{
  assert(eviction_invalidates.find(addr) != eviction_invalidates.end());
  DynamicBitset* &entryPtr = eviction_invalidates[addr];
  entryPtr->clear(cluster);
  //If all cluster bits are cleared, remove the entry
  if(entryPtr->allClear()) { free(entryPtr); eviction_invalidates.erase(addr); }
}

inline void
CacheDirectory::update_logical_channel(int cluster, int channel_num) 
{
  seq_num[cluster].dir_update(channel_num);  
}

inline void
CacheDirectory::gather_end_of_simulation_stats() const
{
  for(size_t i = 0; i < config.num_dir_entries; i++) {
    if(dir_array[i].check_valid()) {
      dir_array[i].gather_end_of_life_stats();
    }
  }
}

#endif //#ifndef __DIRECTORY_H__
