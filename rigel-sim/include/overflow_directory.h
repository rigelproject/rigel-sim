#ifndef __OVERFLOW_DIRECTORY_H__
#define __OVERFLOW_DIRECTORY_H__

#include "sim.h"
#include "interconnect.h"
#include "memory/address_mapping.h"
// Needed to reuse directory entries from the file.
#include "directory.h"

#include <set>
#include <list>
#include <queue>

namespace rigel
{
  // True if we want to enable timing for the overflow directory.
  extern bool ENABLE_OVERFLOW_MODEL_TIMING;
}

namespace directory
{
  // We place 8 directory entries into each descriptor.  Each entry has a valid
  // bit associated with it and an address that it covers.  
  // Switch to limited sizing as default.  We also need an 18b pointer to the
  // next descriptor.  Because of the hashing and the tag size, you
  // only need 32 - 5 (tag bits) - 5 (G$ bank bits) - 5 (hash bits assuming 32
  // hash buckets) = 17b/entry + valid == 256b/line / 18b/entry = 13 + next
  // desc. pointer.  We round down to eight.  
  //
  // For limited schemes, We can fit 256b / (7*4 + 1)b
  // records for each descriptor.  That means we only need one record per
  // descriptor.
  // For full schemes, we need 4 records per descriptor since we have 128b per
  // directory entry and eight directory entries per descriptor.
  const int NUM_RECORDS_PER_DESC = 1;
  const int NUM_DIR_ENTRIES_PER_RECORD = 8;

  struct dir_of_record_t
  {
    // Constructor.
    dir_of_record_t();
    // Force the controller to allocate an address for the record.
    dir_of_record_t(uint32_t addr) : rigel_target_addr(addr)  { clear_valid(); }
    // True if the record is valid.
    bool valid[NUM_DIR_ENTRIES_PER_RECORD];
    // Directory entries represented by the list entry
    dir_entry_t entry[NUM_DIR_ENTRIES_PER_RECORD];
    // Reset entry completely.
    void clear_valid() {
      for (int i = 0; i < NUM_DIR_ENTRIES_PER_RECORD; i++) { valid[i] = false; }
    }
    bool is_valid() {
      for (int i = 0; i < NUM_DIR_ENTRIES_PER_RECORD; i++) { 
        if (valid[i]) return true;
      }
      return false;
    }
    // Set the target address of the record
    void set_target(uint32_t addr) { rigel_target_addr = addr; }
    // Address of the line holding this value in target memory.
    uint32_t rigel_target_addr;
  };

  // SIZING NOTE:
  // Each record is actually a cache line for holding metadata +
  // NUM_RECORDS_PER_DESC cachelines for entries.  Nominally that is
  // eight cache lines per record.
  struct dir_of_descriptor_t
  {
    // Constructor.
    dir_of_descriptor_t();
    // Force the controller to allocate an address for the record.
    dir_of_descriptor_t(uint32_t addr);
    // Check to see if the record is still valid.
    bool is_valid();
    // Remove valid bits for all entries.
    void clear_valid();
    // List of list entries represented by this record.  Would be pointers in
    // real hardware.
    dir_of_record_t **entry_ptrs;
    // Pointer to the next record in the list.
    struct dir_of_descriptor_t *next;
    // Address of the line holding this value in target memory.
    uint32_t rigel_target_addr;
  };

  struct dir_of_timing_entry_t
  {
    dir_of_timing_entry_t(uint32_t _addr, icmsg_type_t _type) : addr(_addr), type(_type) { }
    uint32_t addr;
    icmsg_type_t type;
  };

  // STRUCT NAME: dir_of_timing_t
  //
  // Tracks the information needed to measure timing of an
  // OverflowCacheDirectory action.  It tracks the number of cycles needed to
  // clock the state machine and the set of addresses that were accessed in the
  // process. 
  struct dir_of_timing_t
  {
    public:
      // Constructor.
      dir_of_timing_t() : cycle_count(0) {}
      // Insert an address.
      void insert_target_addr(dir_of_timing_entry_t t) { 
        if (rigel::ENABLE_OVERFLOW_MODEL_TIMING)
        {
          for ( std::list<dir_of_timing_entry_t>::iterator iter = complete_addr_list.begin();
                iter != complete_addr_list.end(); iter++)
          {
            // If the entry already exists, return silently.
            if (iter->addr == t.addr && iter->type == t.type) return;
          }
          target_addr_q.push(t);
          complete_addr_list.push_back(t);
        }
      }
      // Return the number of cycles needed to clock the FSM.
      int get_cycle_count() const { return cycle_count; }
      // Add another cycle to the FSM count.
      void inc_cycle_count() { cycle_count++; }
      // Obtain the next address and remove from list.  Returns 0 when empty.
      dir_of_timing_entry_t next() { 
        if (0 == target_addr_q.size()) { return dir_of_timing_entry_t(0, IC_MSG_NULL); }
        dir_of_timing_entry_t v = target_addr_q.front();
        return v; 
      }
      // Remove top entry.  Returns true if removal occured.
      bool remove() {
        if (0 == target_addr_q.size()) { return false; }
        target_addr_q.pop();
        return true;
      }
      // Return the number of requests that need to be completed by the timing
      // model.
      size_t misses_pending() const { return target_addr_q.size(); }
      bool done() const { return (target_addr_q.size() == 0); }
    private: 
      std::queue<dir_of_timing_entry_t> target_addr_q;
      // List of all entries, done and not.
      std::list<dir_of_timing_entry_t> complete_addr_list;
      // Number of cycles used to clock FSM.
      int cycle_count;
  };

}

/* CLASS NAME: OverflowCacheDirectory
 * 
 * DESCRIPTON
 * Overflow/L2 direectory structure.  Used as a backing store for the
 * CacheDirectory.
 *
 * TIMING MODELING
 * We can model timing by performing the functional part of the operation using
 * the calls in OverflowCacheDirectory which will each return a number of steps
 * and a list of addresses that each call traversed.  The timing model can then
 * perform those operations and hold back returning the result until all of
 * those lines have been read from the cache model.
 *
 */
class OverflowCacheDirectory
{
  public:
    OverflowCacheDirectory();
    // Set up the data structures that require runtime constants.  Input is the
    // global cache bank to use for backing memory accesses into.
    void init(uint32_t bank_id, size_t desc_head_ptr_cnt);
    // Search for an entry matching addr without creating a timing structure.
    // Used only to decide a priori whether an access will hit or miss, for stats.
    bool is_present(uint32_t addr); // const
    // Search for an entry matching addr.  Return true if found with copy of
    // entry in 'entry', false otherwise.
    bool find_entry_by_addr(uint32_t addr, directory::dir_entry_t &entry,
      directory::dir_of_timing_t &dir_timer); // const
    // Find a valid list entry in the list and insert.  Allocates an entry if
    // need be from the free list or from the "heap".   Updates valid
    // bits if necessary.
    bool insert_dir_entry(uint32_t addr, const directory::dir_entry_t &entry,
      directory::dir_of_timing_t &dir_timer);
    // Remove the entry from the list based on addr. Return record to the free
    // list if completely empty.  Returns entry in 'entry'.  Returns false if
    // the entry could not be found (error?).
    bool remove_dir_entry(uint32_t addr, directory::dir_entry_t &entry,
      directory::dir_of_timing_t &dir_timer);
    // Swap 'entry' (from the HW directory) with the entry for 'addr' already in
    // the overflow directory.  Returns false if the entry could not be found.
    bool swap_dir_entry(uint32_t addr, directory::dir_entry_t &insert_entry, 
                        directory::dir_entry_t &remove_entry, 
                        directory::dir_of_timing_t &dir_timer);
    // Copy 'entry' over the old entry with a matching address in the
    // overflow_directory.  The address 'addr' must already be in the directory.
    bool update_dir_entry(uint32_t addr, directory::dir_entry_t &entry,
      directory::dir_of_timing_t &dir_timer);
    // Destructor.
    ~OverflowCacheDirectory();
    // Return total number of allocations.
    static uint32_t get_total_allocs() { return rigel_target_heap_allocations_total; }

  private:
    // Helper routine to remove curr from the list and put on the free list.  If
    // 'prev' is NULL, then stick curr's next point into the head.
    void remove_free_record( directory::dir_of_descriptor_t **head,
                             directory::dir_of_descriptor_t *prev,
                             directory::dir_of_descriptor_t *curr);
    // Helper routine to clear an entry from a record.  The entry is not
    // deallocated, however.
    void remove_free_entry(directory::dir_of_record_t * victim, int ent_idx);
    // Allocate a cache line from within the target address space which maps to
    // our bank id.
    uint32_t allocate_rigel_target_addr();
    // Return the offset into the array of linked list head poitners.
    size_t get_list_index(uint32_t addr);
    
  private: /* DATA */
    // Per-set pointers to linked-list of list records. Initialized by init().
    directory::dir_of_descriptor_t ** descriptor_head_list;
    // Number of entries in the descriptor_head_list array.
    size_t num_descriptor_head_ptrs;
    // Free list of records to use.  Initially NULL and populated as records
    // become free'ed in remove_free_record().
    directory::dir_of_descriptor_t *free_list_head;
    // Target address to start allocations from.  TODO: This nees to be set on a
    // per-GC-bank basis.
    uint32_t rigel_target_top_of_heap;
    // Number of blocks allocated so far.
    uint32_t rigel_target_heap_allocations;
    // Number of allocations across all directories.
    static uint32_t rigel_target_heap_allocations_total;
    // Top of the memory region for the OverflowDirecotry.
    uint32_t target_top_of_alloc;
    // Bank ID.
    uint32_t gcache_bank_id;

};


inline size_t 
OverflowCacheDirectory::get_list_index(uint32_t addr) 
{
  if (num_descriptor_head_ptrs > (uint32_t)rigel::cache::GCACHE_SETS) {
    assert(0 && "Cannot have more record lists than GCache sets (for now)");
  }

  // Get GCache set number.
  size_t set = AddressMapping::GetGCacheSet(addr, rigel::cache::GCACHE_SETS);
  return (set % num_descriptor_head_ptrs);
}

namespace directory 
{
  inline
  dir_of_descriptor_t::dir_of_descriptor_t(uint32_t addr) 
    : rigel_target_addr(addr)
  {
    entry_ptrs = new dir_of_record_t *[NUM_RECORDS_PER_DESC];
    // Allocate each entry.  The target address *MUST* be filled in by the
    // caller since it is the overflow directory which has access to the
    // allocator.  We could fix this by making the allocator a static memory
    // variable, but there shall be none of that for now.
    for (int i = 0; i < NUM_RECORDS_PER_DESC; i++) { 
      entry_ptrs[i] = new dir_of_record_t(0);
    }
    // Clear the entries.
    clear_valid();
    next = NULL;
  }

  inline bool 
  dir_of_descriptor_t::is_valid() 
  {
    for (int i = 0; i < NUM_RECORDS_PER_DESC; i++) { 
      if (entry_ptrs[i]->is_valid()) return true;
    }
    return false;
  }

  inline void 
  dir_of_descriptor_t::clear_valid() 
  {
    for (int i = 0; i < NUM_RECORDS_PER_DESC; i++) { 
      entry_ptrs[i]->clear_valid();
    }
  }

}
#endif
