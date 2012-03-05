
#include <assert.h>                     // for assert
#include <stddef.h>                     // for NULL, size_t
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, stderr
#include "memory/address_mapping.h"  // for AddressMapping
#include "define.h"         // for DEBUG_HEADER, etc
#include "directory.h"      // for dir_entry_t, etc
#include "overflow_directory.h"  // for OverflowCacheDirectory, etc
#include "profile/profile.h"        // for ProfileStat
#include "profile/profile_names.h"
#include "sim.h"            // for stats, etc
#if 0
#define __DEBUG_ADDR 0x01448ce0
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#define __DEBUG_CYCLE 0x7FFFFFFF
#endif

uint32_t OverflowCacheDirectory::rigel_target_heap_allocations_total = 0;

OverflowCacheDirectory::OverflowCacheDirectory()
{

}

void
OverflowCacheDirectory::init(uint32_t bank_id, size_t desc_head_ptr_cnt) 
{
  using namespace rigel;
  using namespace rigel::cache;
  using namespace rigel::memory;
  using namespace directory;

  num_descriptor_head_ptrs = desc_head_ptr_cnt;
  gcache_bank_id = bank_id;

  // Initialize the pointers to record lists.  Initially they are all empty
  // (valid == false)
  descriptor_head_list = new dir_of_descriptor_t *[num_descriptor_head_ptrs];
  // All lists are empty initially.
  for (size_t i = 0; i < num_descriptor_head_ptrs; i++) { descriptor_head_list[i] = NULL; }
  // Initially the free list is empty.
  free_list_head = NULL;
  // Reset heap allocator. TODO: Need valid address in target memory here.
  rigel_target_heap_allocations = 0;
  // Calculate the top of stack and set the base address of the cache directory
  // to the first address that maps to this GCache bank.
  rigel_target_top_of_heap = 0;
  uint32_t stack_size = PER_THREAD_STACK_SIZE*THREADS_PER_CLUSTER*NUM_CLUSTERS;
  rigel_target_top_of_heap -= (stack_size + DIRECTORY_MEMORY_SIZE);
  // Set the top of the heap, allocations should not violate this boundary.
  target_top_of_alloc = rigel_target_top_of_heap + DIRECTORY_MEMORY_SIZE;
  while (gcache_bank_id != AddressMapping::GetGCacheBank(rigel_target_top_of_heap)) 
  {
    // Iterate over cache lines until we find a suitable address.
    rigel_target_top_of_heap += LINESIZE;
  }
  //DEBUG_HEADER();
  //fprintf(stderr, "[DIRECTORY INIT] top of heap: 0x%08x\n", rigel_target_top_of_heap);

}

bool 
OverflowCacheDirectory::update_dir_entry( uint32_t addr,
                                          directory::dir_entry_t &entry, 
                                          directory::dir_of_timing_t &dir_timer)
{
  using namespace directory;

  uint32_t touches = 0;

  switch (entry.get_state()) {
    case DIR_STATE_INVALID:
      DEBUG_HEADER();
      fprintf(stderr, "[BUG] Doing update with invalid entry into overflow directory!\n");
      entry.dump();
      assert(0);
    default: break;
  }

  // Index into the table.  May be a subset of GC sets.
  size_t rec_list_idx = get_list_index(addr);
  dir_of_descriptor_t *list_p = descriptor_head_list[rec_list_idx];
  // Empty list.
  if (NULL == list_p) { 
    assert(0 && "The entry correpsonding to 'addr' is not present in the overflow directory!");
  }

  do {
    touches++;

    for (int idx = 0; idx < NUM_RECORDS_PER_DESC; idx++) {
      // TIMING: Assume that we can do compare of all entries in a single
      // cycle.  We still need to burn a cycle to access the record and we
      // need to get the record from target memory.  NOTE: update is usually to
      // a value that is already cached by the directory controller and thus the
      // timer value that is returned can just be ignored.
      dir_timer.inc_cycle_count();
      dir_timer.insert_target_addr(
        dir_of_timing_entry_t(list_p->rigel_target_addr, IC_MSG_DIRECTORY_READ_REQ));

      for (int ent = 0; ent < NUM_DIR_ENTRIES_PER_RECORD; ent++) {
        if (list_p->entry_ptrs[idx]->valid[ent]) {
          // Sanity check.
          if (!list_p->entry_ptrs[idx]->entry[ent].check_valid())
          {
            DEBUG_HEADER();
            fprintf(stderr, "List entry is valid in overflow, but the actual entry is not!\n");
            DEBUG_HEADER(); list_p->entry_ptrs[idx]->entry[ent].dump();
            assert(0);
          }

          if (addr == list_p->entry_ptrs[idx]->entry[ent].get_addr()) {
            // TIMING:  We found the entry.  Pay the cycle for grabbing the
            // address from target memory and writing it back.
            dir_timer.inc_cycle_count();
            dir_timer.insert_target_addr( 
              dir_of_timing_entry_t( list_p->entry_ptrs[idx]->rigel_target_addr, 
                                     IC_MSG_DIRECTORY_WRITE_REQ));

            rigel::profiler::stats[STATNAME_OVERFLOW_DIRECTORY_TOUCHES].inc_mem_histogram(touches, 1);
            #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              dir_entry_t old = list_p->entry_ptrs[idx]->entry[ent];
              DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --UPDATE A-- OLD: "); old.dump();
              DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --UPDATE B-- NEW: "); entry.dump();
            }
            #endif
            // Update the entry.
            list_p->entry_ptrs[idx]->entry[ent] = entry;
            assert(0 != list_p->entry_ptrs[idx]->rigel_target_addr && "Found invalid target addr!");
            return true; 
          }
        }
      }
    }
  } while ((list_p = list_p->next) && (NULL != list_p) && list_p->is_valid());

  assert(0 && "The entry correpsonding to 'addr' is not present in the overflow directory!");
}

// swap_dir_entry() assumes that the addr already exits in the overflow
// directory.
bool 
OverflowCacheDirectory::swap_dir_entry(uint32_t addr, directory::dir_entry_t &insert_entry, 
                        directory::dir_entry_t &remove_entry, 
                        directory::dir_of_timing_t &dir_timer)
{
  using namespace directory;

  // Do not allow entries that are pending on a fill to be inserted into the
  // overflow directory list.
  if (insert_entry.check_fill_pending()) { return false; }

  switch (insert_entry.get_state()) {
    // Disallow pending requests from being inserted.
    case DIR_STATE_PENDING_READ:
    case DIR_STATE_PENDING_WRITE:
    case DIR_STATE_PENDING_WB:
    case DIR_STATE_EVICTION_LOCK_WRITE:
    case DIR_STATE_EVICTION_LOCK_READ:
    // Adding these to keep transient state from being moved to the OF
    case DIR_STATE_PENDING_READ_BCAST:
    case DIR_STATE_PENDING_WRITE_BCAST:
    case DIR_STATE_EVICTION_LOCK_READ_BCAST:
      return false;
    // Invalid entries should have already been reclaimed without calling
    // swap_dir_entry().
    case DIR_STATE_INVALID:
      DEBUG_HEADER();
      fprintf(stderr, "[BUG] Swapping invalid entry into overflow directory!\n");
      insert_entry.dump();
      assert(0);
    default: break;
  }


  // Index into the table.  May be a subset of GC sets.
  size_t rec_list_idx = get_list_index(addr);
  size_t insert_list_idx = get_list_index(insert_entry.get_addr());
  size_t remove_list_idx = get_list_index(remove_entry.get_addr());
  // If the two lists do not match, we cannot perform a simple swap.  Instead we
  // do an insert and a removal.
  if (remove_list_idx != insert_list_idx) {
    insert_dir_entry(insert_entry.get_addr(), insert_entry, dir_timer);
    if (!remove_dir_entry(remove_entry.get_addr(), remove_entry, dir_timer)) {
      DEBUG_HEADER();
      assert(0 && "Entry to remove does not exist!");
    }
    return true;
  }

  dir_of_descriptor_t *list_p = descriptor_head_list[rec_list_idx];
  // Empty list.
  if (NULL == list_p) { 
    assert(0 && "The entry correpsonding to 'addr' is not present in the overflow directory!");
  }

  // Iterate over records and entries...TODO: Will require multiple cache
  // accesses...Fix by finding request then issuing cache requests to get
  // timing.
  do {
    for (int idx = 0; idx < NUM_RECORDS_PER_DESC; idx++) {
      // TIMING: Assume that we can do compare of all entries in a single
      // cycle.  We still need to burn a cycle to access the record and we
      // need to get the record from target memory.
      dir_timer.inc_cycle_count();
      dir_timer.insert_target_addr( 
        dir_of_timing_entry_t(list_p->rigel_target_addr, IC_MSG_DIRECTORY_READ_REQ));

      for (int ent = 0; ent < NUM_DIR_ENTRIES_PER_RECORD; ent++) {
        assert(0 != list_p->entry_ptrs[idx]->rigel_target_addr && "Found invalid target addr!");
        if (list_p->entry_ptrs[idx]->valid[ent]) {
          // Sanity check.
          if (!list_p->entry_ptrs[idx]->entry[ent].check_valid())
          {
            DEBUG_HEADER();
            fprintf(stderr, "List entry is valid in overflow, but the actual entry is not!\n");
            DEBUG_HEADER(); list_p->entry_ptrs[idx]->entry[ent].dump();
            assert(0);
          }

          if (addr == list_p->entry_ptrs[idx]->entry[ent].get_addr()) {
            // TIMING:  We found the entry.  Pay the cycle for grabbing the
            // address from target memory.
            dir_timer.inc_cycle_count();
            dir_timer.insert_target_addr( 
              dir_of_timing_entry_t(list_p->entry_ptrs[idx]->rigel_target_addr, 
              IC_MSG_DIRECTORY_WRITE_REQ));
            // Get the entry that currently exists in the overflow
            remove_entry = list_p->entry_ptrs[idx]->entry[ent];
            // Insert the new entry where the old one used to be.
            list_p->entry_ptrs[idx]->entry[ent] = insert_entry;
            #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --SWAP A-- REMOVE: "); remove_entry.dump();
              DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --SWAP B-- INSERT: "); insert_entry.dump();
            }
            #endif
            return true; 
          }
        }
      }
    }
  } while ((list_p = list_p->next) && (NULL != list_p) && list_p->is_valid());

  assert(0 && "The entry correpsonding to 'addr' is not present in the overflow directory!");
}

//Returns the depth in the overflow entry of the record corresponding to the given address.
//The depth is expressed in terms of the number of descriptors that must be touched.
//This will be 0 if we can obviate the linked list traversal by observing that it is 0-length.
//If we reach the end of the list, the address is not found, and all descriptors are full
bool OverflowCacheDirectory::is_present(uint32_t addr) 
{
  using namespace directory;

  // Index into the table.  May be a subset of GC sets.
  size_t rec_list_idx = get_list_index(addr);

  dir_of_descriptor_t *list_p = descriptor_head_list[rec_list_idx];
  // Empty list.
  if (NULL == list_p) { return false; }

  // Iterate over records and entries...TODO: Will require multiple cache
  // accesses...Fix by finding request then issuing cache requests to get
  // timing.
  do {
    for (int idx = 0; idx < NUM_RECORDS_PER_DESC; idx++) {
      for (int ent = 0; ent < NUM_DIR_ENTRIES_PER_RECORD; ent++) {
        assert(0 != list_p->entry_ptrs[idx]->rigel_target_addr && "Found invalid target addr!");
        if (list_p->entry_ptrs[idx]->valid[ent]) {
          // Sanity check.
          if (!list_p->entry_ptrs[idx]->entry[ent].check_valid())
          {
            DEBUG_HEADER();
            fprintf(stderr, "List entry is valid in overflow, but the actual entry is not!\n");
            DEBUG_HEADER(); list_p->entry_ptrs[idx]->entry[ent].dump();
            assert(0);
          }
          if (addr == list_p->entry_ptrs[idx]->entry[ent].get_addr()) {
            return true; 
          }
        }
      }
    }
  } while ((list_p = list_p->next) && (NULL != list_p) && list_p->is_valid());
  
  return false;
}

bool 
OverflowCacheDirectory::find_entry_by_addr( uint32_t addr, 
                                            directory::dir_entry_t &entry,
                                            directory::dir_of_timing_t &dir_timer) 
{
  using namespace directory;

  uint32_t touches = 0; //Number of memory system accesses needed to find this entry

  // Index into the table.  May be a subset of GC sets.
  size_t rec_list_idx = get_list_index(addr);

  dir_of_descriptor_t *list_p = descriptor_head_list[rec_list_idx];
  // Empty list.
  if (NULL == list_p)
  {
    rigel::profiler::stats[STATNAME_OVERFLOW_DIRECTORY_TOUCHES].inc_mem_histogram(0, 1);
    return false;
  }

  // Iterate over records and entries...TODO: Will require multiple cache
  // accesses...Fix by finding request then issuing cache requests to get
  // timing.
  do {
    touches++;
    for (int idx = 0; idx < NUM_RECORDS_PER_DESC; idx++) {
      // TIMING: Assume that we can do compare of all entries in a single
      // cycle.  We still need to burn a cycle to access the record and we
      // need to get the record from target memory.
      dir_timer.inc_cycle_count();
      dir_timer.insert_target_addr( 
        dir_of_timing_entry_t(list_p->rigel_target_addr, 
        IC_MSG_DIRECTORY_READ_REQ));

      for (int ent = 0; ent < NUM_DIR_ENTRIES_PER_RECORD; ent++) {
        assert(0 != list_p->entry_ptrs[idx]->rigel_target_addr && "Found invalid target addr!");
        if (list_p->entry_ptrs[idx]->valid[ent]) {
          // Sanity check.
          if (!list_p->entry_ptrs[idx]->entry[ent].check_valid())
          {
            DEBUG_HEADER();
            fprintf(stderr, "List entry is valid in overflow, but the actual entry is not!\n");
            DEBUG_HEADER(); list_p->entry_ptrs[idx]->entry[ent].dump();
            assert(0);
          }

          if (addr == list_p->entry_ptrs[idx]->entry[ent].get_addr()) {
            // TIMING:  We found the entry.  Pay the cycle for grabbing the
            // address from target memory.
            dir_timer.inc_cycle_count();
            dir_timer.insert_target_addr( 
              dir_of_timing_entry_t(list_p->entry_ptrs[idx]->rigel_target_addr, 
              IC_MSG_DIRECTORY_READ_REQ));

            entry = list_p->entry_ptrs[idx]->entry[ent];
            #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --SEARCH-- (FOUND) "); entry.dump();
            }
            #endif
            rigel::profiler::stats[STATNAME_OVERFLOW_DIRECTORY_TOUCHES].inc_mem_histogram(touches, 1);
            return true; 
          }
        }
      }
    }
  } while ((list_p = list_p->next) && (NULL != list_p) && list_p->is_valid());
  
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --SEARCH-- (NOT FOUND) "); entry.dump();
  }
  #endif
  rigel::profiler::stats[STATNAME_OVERFLOW_DIRECTORY_TOUCHES].inc_mem_histogram(touches, 1);
  return false;
}

bool 
OverflowCacheDirectory::insert_dir_entry( uint32_t addr, 
                                          const directory::dir_entry_t &entry,
                                          directory::dir_of_timing_t &dir_timer) 
{
  using namespace directory;
  uint32_t touches = 0;
  // Index into the table.  May be a subset of GC sets.
  size_t rec_list_idx = get_list_index(addr);

  if (entry.check_fill_pending()) {
    DEBUG_HEADER();
    fprintf(stderr, "Trying to insert a dir entry that is still fill pending!\n");
    entry.dump();
    assert(0);
  }

  dir_of_descriptor_t *list_p = descriptor_head_list[rec_list_idx];

  switch (entry.get_state()) {
    case DIR_STATE_INVALID:
      DEBUG_HEADER();
      fprintf(stderr, "[BUG] Inserting invalid entry into overflow directory!\n");
      entry.dump();
      assert(0);
    default: break;
  }

  // Empty list, we have to allocate immediately.
  if (NULL != list_p) 
  {
    // Iterate over records and entries to find an empty spot for the new entry.
    do {
      touches++;
      for (int idx = 0; idx < NUM_RECORDS_PER_DESC; idx++) {
        // TIMING: Assume that we can do compare of all entries in a single
        // cycle.  We still need to burn a cycle to access the record and we
        // need to get the record from target memory.
        dir_timer.inc_cycle_count();
        dir_timer.insert_target_addr( dir_of_timing_entry_t(list_p->rigel_target_addr, 
                                      IC_MSG_DIRECTORY_READ_REQ));

        for (int ent = 0; ent < NUM_DIR_ENTRIES_PER_RECORD; ent++) {
          assert(0 != list_p->entry_ptrs[idx]->rigel_target_addr && "Found invalid target addr!");
          if (!list_p->entry_ptrs[idx]->valid[ent]) {
            // TIMING:  We found the entry.  Pay the cycle for grabbing the
            // address from target memory.
            dir_timer.inc_cycle_count();
            dir_timer.insert_target_addr( 
              dir_of_timing_entry_t(list_p->entry_ptrs[idx]->rigel_target_addr, 
              IC_MSG_DIRECTORY_WRITE_REQ));

            #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --INSERT-- (NO ALLOC) "); entry.dump();
            }
            #endif
            // Insert the entry into invalid list_entry.
            list_p->entry_ptrs[idx]->entry[ent] = entry;
            list_p->entry_ptrs[idx]->valid[ent] = true;

            // Done.  Inserted the entry.
            rigel::profiler::stats[STATNAME_OVERFLOW_DIRECTORY_TOUCHES].inc_mem_histogram(touches, 1);
            return true; 
          }
        }
      }
    } while ((list_p = list_p->next) && list_p->is_valid());
    // Did not find a record with an empty spot.  Allocate it and move on.
  }

  //If the list was previously null, we just need to touch the new entry we're inserting.
  //If it wasn't, we needed to traverse the entire list, and touches holds the length of the list.
  rigel::profiler::stats[STATNAME_OVERFLOW_DIRECTORY_TOUCHES].inc_mem_histogram(touches, 1);

  // TIMING:  Since we do all of the freelist and head management in registers,
  // we do not have to access memory and thus do not charge ourselves a memory
  // penalty.  We do need to do a cycle's worth of manipulation.  
  dir_timer.inc_cycle_count();

  // Save the current head of the list to put in the next ptr of the newly
  // inserted record. NOTE: LIFO list insertion for cache hotness.
  dir_of_descriptor_t *curr_head = descriptor_head_list[rec_list_idx];
  // We have to allocate an entry.
  if (NULL != free_list_head) {
    // Allocate from the free list.
    descriptor_head_list[rec_list_idx] = free_list_head;
    free_list_head = free_list_head->next;
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --INSERT-- (FREE LIST) "); entry.dump();
    }
    #endif
  } else {
    // Allocate from the heap. A record is a cache line for the recod and
    // some number of lines for the directory entries.
    uint32_t target_addr = allocate_rigel_target_addr();
    // Host-side allocation.
    descriptor_head_list[rec_list_idx] = new dir_of_descriptor_t(target_addr);
    // Fill in the target addresses for the records.
    for (int i = 0; i < NUM_RECORDS_PER_DESC; i++) {
      descriptor_head_list[rec_list_idx]->entry_ptrs[i]->set_target(allocate_rigel_target_addr());
    }
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --INSERT-- (NEW ALLOC) "); entry.dump();
    }
    #endif
  }

  // Allocation complete, insert the entry and return.  The curr_head may be
  // NULL if the list was empty or it is not, in which case we are just
  // inserting a new entry.
  descriptor_head_list[rec_list_idx]->next = curr_head;
  // Ensure that the record is totally invalidated (should be anyway).
  descriptor_head_list[rec_list_idx]->clear_valid();
  // Insert the entry into the record.  Just use zeroth record entry and
  // zeroth entry list entry since the record is clean.
  descriptor_head_list[rec_list_idx]->entry_ptrs[0]->valid[0] = true;
  descriptor_head_list[rec_list_idx]->entry_ptrs[0]->entry[0] = entry;

  return true;
}

bool 
OverflowCacheDirectory::remove_dir_entry( uint32_t addr, 
                                          directory::dir_entry_t &entry,
                                          directory::dir_of_timing_t &dir_timer)
{
  using namespace directory;

  // Index into the table.  May be a subset of GC sets.
  size_t rec_list_idx = get_list_index(addr);
  // Head of the list.
  dir_of_descriptor_t *list_p = descriptor_head_list[rec_list_idx];
  // Empty list.
  if (NULL == list_p) { return false; }
  dir_of_descriptor_t *list_prev = NULL;

  // Iterate over records and entries.
  do {
    for (int idx = 0; idx < NUM_RECORDS_PER_DESC; idx++) {
      // TIMING: Assume that we can do compare of all entries in a single
      // cycle.  We still need to burn a cycle to access the record and we
      // need to get the record from target memory.
      dir_timer.inc_cycle_count();
      dir_timer.insert_target_addr( 
        dir_of_timing_entry_t(list_p->rigel_target_addr, IC_MSG_DIRECTORY_READ_REQ));

      for (int ent = 0; ent < NUM_DIR_ENTRIES_PER_RECORD; ent++) {
        if (list_p->entry_ptrs[idx]->valid[ent]) {
          // Sanity check.
          if (!list_p->entry_ptrs[idx]->entry[ent].check_valid())
          {
            DEBUG_HEADER();
            fprintf(stderr, "List entry is valid in overflow, but the actual entry is not!\n");
            DEBUG_HEADER(); list_p->entry_ptrs[idx]->entry[ent].dump();
            assert(0);
          }

          if (addr == list_p->entry_ptrs[idx]->entry[ent].get_addr()) {
            // TIMINIG:  We found the entry.  Pay the cycle for grabbing the
            // address from target memory.
            dir_timer.inc_cycle_count();
            dir_timer.insert_target_addr( 
              dir_of_timing_entry_t(list_p->entry_ptrs[idx]->rigel_target_addr, 
                                          IC_MSG_DIRECTORY_WRITE_REQ));

            // Copy the entry to return to caller.
            entry = list_p->entry_ptrs[idx]->entry[ent];
            #ifdef __DEBUG_ADDR
            if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
              DEBUG_HEADER(); fprintf(stderr, "[OF DIR] --REMOVE-- (FREE LIST) "); entry.dump();
            }
            #endif
            // Remove the dir entry from the dir_of_record_t
            remove_free_entry(list_p->entry_ptrs[idx], ent);
            // If they are all invalid, we can free the dir_of_record_t
            if (!list_p->is_valid()) {
              remove_free_record(&descriptor_head_list[rec_list_idx], list_prev, list_p);
            }
            return true; 
          }
        }
      }
    }
    // Keep track of the last pointer for doing record removal.
    list_prev = list_p;
  } while ((list_p = list_p->next) && list_p->is_valid());
  
  // Did not find the record :'(
  return false;
}

void 
OverflowCacheDirectory::remove_free_record(directory::dir_of_descriptor_t **head, 
                                           directory::dir_of_descriptor_t *prev, 
                                           directory::dir_of_descriptor_t *curr)
{
  assert(NULL != curr && "[BUG] curr must not be NULL");
  assert(NULL != head && "[BUG] head must not be NULL");

  if (NULL == prev) {
    // Replacing head node.
    *head = curr->next;
  } else {
    assert(prev->next == curr && "prev->next must point to curr!");
    // Unlink curr from the list.
    prev->next = curr->next;
  }
  // Put curr in the free list.
  curr->next = free_list_head;
  free_list_head = curr;
  return;
}

void 
OverflowCacheDirectory::remove_free_entry(directory::dir_of_record_t * victim, int ent_idx) 
{
  // Reset the entry to avoid bugs from reading a deallocated entry.
  victim->entry[ent_idx].reset();
  // Clear it from the record.
  victim->valid[ent_idx] = false;
}


uint32_t 
OverflowCacheDirectory::allocate_rigel_target_addr() 
{
  // TODO: One thing we could do as a mini optimization is automatically fill
  // these lines into the GCache since they are writes.
  uint32_t old_toh = rigel_target_top_of_heap;
  rigel_target_heap_allocations++;
  rigel_target_heap_allocations_total++;

  do {
    // Iterate over cache lines until we find a suitible address.
    rigel_target_top_of_heap += rigel::cache::LINESIZE;
  } while (gcache_bank_id != AddressMapping::GetGCacheBank(rigel_target_top_of_heap));

  if (target_top_of_alloc < rigel_target_top_of_heap) {
    DEBUG_HEADER();
    fprintf(stderr, "[BUG] Overflowing allocation for OverflowDirectory. "
                    "rigel_target_top_of_heap: 0x%08x number of allocs: %d\n", 
                    rigel_target_top_of_heap, rigel_target_heap_allocations);
    assert(0);
  }

  return old_toh;
}
