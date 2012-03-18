#include "directory.h"
#include "util/debug.h"
#include "profile/profile.h"
#include "overflow_directory.h"
#include <list>
#include <bitset>

#if 0
#define __DEBUG_ADDR 0x04a1c800
#ifdef __DEBUG_CYCLE
#undef __DEBUG_CYCLE
#endif
#define __DEBUG_CYCLE 0
#else
#undef __DEBUG_ADDR
#define __DEBUG_CYCLE 0x7FFFFFFF
#endif

using namespace rigel;

// Minimum number of allowable overflow lists.  Set to the maximum number of
// sets we want to simulate (otherwise you get weird results when changing
// associativity.)
static int MIN_OVERFLOW_LISTS = 512;

//  We need to serialize write requests.  We also need to make sure that if we
//  give write permission to a cluster, that the write occurs before any
//  invalidate/writeback message that comes after it is handled by the cluster.
//
//  We need to figure out how to handle G$ misses versus updates directly from
//  the C$ appropriately.  Maybe a W -> R/W permission updates the G$ or it does
//  not?

CacheDirectory::CacheDirectory() : valid_bits(rigel::COHERENCE_DIR_WAYS *
                                              rigel::COHERENCE_DIR_SETS)
{
  dir_array = NULL; 
}

void
CacheDirectory::init(int bank_id) 
{
  using namespace rigel;
  using namespace rigel::cache;

  config.bitvec_size = NUM_CLUSTERS;
  config.num_dir_entries = COHERENCE_DIR_WAYS * COHERENCE_DIR_SETS;
  //config.num_dir_entries = GCACHE_SETS * GCACHE_WAYS * GCACHE_BANKS_PER_MC;
  config.w2w_updates_gcache = true;
  config.num_ways = COHERENCE_DIR_WAYS;
  config.num_sets = COHERENCE_DIR_SETS;
  // Force overflow directory to always have at least some minimum number of
  // entries.
  //config.num_overflow_lists = COHERENCE_DIR_SETS < MIN_OVERFLOW_LISTS ? 
  //  MIN_OVERFLOW_LISTS : COHERENCE_DIR_SETS;
  // You end up with counter-intuitive results when you scale the number of ways
  // with overflow directories enabled.  The best thing to do is just keep a
  // fixed sized number of lists to make everything equal.  128 is sufficient
  // for me.
  config.num_overflow_lists = MIN_OVERFLOW_LISTS;

  // Uncomment the following two lines to make directory fully associative:
  //
  // config.num_ways = config.num_dir_entries;
  // size_t sets = 1; // config.num_dir_entries / config.num_ways;

  dir_array = new directory::dir_entry_t[config.num_dir_entries]; 
  // Ensure that all entries are clear.
  for (size_t i = 0; i < config.num_dir_entries; i++) {
    dir_array[i].init(&valid_bits, i);
    //We don't need to reset() the entry here because init() takes care of it.
  }
  dir_pending_replacement_bitvec = new bool [config.num_sets];
  dir_pending_replacement_way = new int [config.num_sets];
  dir_pending_replacement_aggressor_address = new uint32_t [config.num_sets];
  for (size_t i = 0; i < config.num_sets; i++) {
    dir_pending_replacement_bitvec[i] = false;
    dir_pending_replacement_aggressor_address[i] = 0xDEADBEEF;
  }

  // Add thyne self to the global list of directories for debugging.
  rigel::GLOBAL_debug_sim.directory.push_back(this);

  // Initialize the overflow directory.
  overflow_directory = new OverflowCacheDirectory();
  overflow_directory->init(bank_id, config.num_overflow_lists);
  // Set bank ID
  gcache_bank_id = bank_id;

  // Initialize the per-cluster/bank counters.
  seq_num = new seq_num_t [NUM_CLUSTERS];
  // Initially there are no broadcast operations pending.
  outstanding_bcasts = 0;
}

void CacheDirectory::PerCycle()
{
  gather_statistics();
}

directory::dir_entry_t &
CacheDirectory::get_entry(size_t set, size_t way) const
{
  if ( config.num_dir_entries <= (set * way ) ) {
    rigel::GLOBAL_debug_sim.dump_all();
    fprintf(stderr, "Invalid directory entry: (%zu, %zu)\n", set, way);
    assert(0);
  }
  return dir_array[(set * config.num_ways) +  way];
}

bool CacheDirectory::is_present(uint32_t req_addr) const
{

  using namespace directory;
  size_t set = get_set(req_addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (!entry.check_addr_match(req_addr))
      continue;
    return true;
  }
  return false;
}


void 
CacheDirectory::set_atomic_lock(uint32_t addr, int cluster)
{
  if (!ENABLE_EXPERIMENTAL_DIRECTORY) { return; }

  using namespace directory;
  size_t set = get_set(addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if(!entry.check_valid()) assert(0);
    if (!entry.check_addr_match(addr)) { continue; }
    entry.set_atomic_lock(cluster);
    return;
  }
  //assert(0 && "We should not set the atomic lock for invalid lines.");
}

void 
CacheDirectory::clear_atomic_lock(uint32_t addr, int cluster)
{
  if (!ENABLE_EXPERIMENTAL_DIRECTORY) { return; }

  using namespace directory;
  size_t set = get_set(addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (!entry.check_addr_match(addr)) { continue; }
    entry.clear_atomic_lock(cluster);
    return;
  }
//  assert(0 && "We should not clear the atomic lock for invalid lines.");

}

bool
CacheDirectory::check_atomic_lock(uint32_t addr, int cluster) const
{
  if (!ENABLE_EXPERIMENTAL_DIRECTORY) { return false; }

  using namespace directory;
  size_t set = get_set(addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (!entry.check_addr_match(addr)) { continue; }
    // Allow entry to proceed if it is already a sharer.
    if (entry.check_cluster_is_sharing(cluster)) return false;
    if (entry.check_atomic_lock(cluster)) return true;
    return false;
  }
  // Not found, so no.
  return false;
}

bool CacheDirectory::is_present_in_overflow(uint32_t req_addr) const
{
  return overflow_directory->is_present(req_addr);
}

void
CacheDirectory::gather_statistics() {
  // If the cycle is right, sample the directory to profile the number of valid
  // entries for each region of memory.
  if ((rigel::CURR_CYCLE % 1024) == 0)
  {
    uint64_t code, stack, other, invalid;
    get_num_valid_entries(code, stack, other, invalid);
    profiler::stats[STATNAME_DIR_ENTRIES_CODE].inc_histogram(code);
    profiler::stats[STATNAME_DIR_ENTRIES_STACK].inc_histogram(stack);
    profiler::stats[STATNAME_DIR_ENTRIES_INVALID].inc_histogram(invalid);
    profiler::stats[STATNAME_DIR_ENTRIES_OTHER].inc_histogram(other);
  }
  //Do this every cycle
  profiler::stats[STATNAME_OUTSTANDING_COHERENCE_BCASTS].inc_histogram(get_num_outstanding_bcasts());
}

void
CacheDirectory::get_num_valid_entries(uint64_t &code, uint64_t &stack, uint64_t &other, uint64_t &invalid)
{
  code = 0;
  stack = 0;
  other = 0; 
  invalid = 0;
  using namespace directory;

  invalid = (uint64_t) valid_bits.getNumClearBits();
  // Iterate through valid directory entries and check address ranges.

  for(int i = valid_bits.findFirstSet();
      i != -1;
      i = valid_bits.findNextSet(i))
  {
      dir_entry_t &entry = dir_array[i];
			//FIXME CODEPAGE_HIGHWATER_MARK no longer exists; information of whether address are code or data
			//is held in the backing store, which is not exposed to us.  Once we expose that API everywhere in
			//the simulator, reimplement the below in a better way.  Error out on this code path for now.
			throw ExitSim("Error: See FIXME at " __FILE__ ":" STRINGIZE(__LINE__) );
#if 0
      if (entry.get_addr() < CODEPAGE_HIGHWATER_MARK) { 
        code++;
      } else 
      if (entry.get_addr() > 0x7FFFFFFF) {
        stack++;
      } else {
        other++;
      }
#endif
  }
}

void
CacheDirectory::helper_probe_filter_bcast(
  uint32_t addr, 
  size_t next_sharer, 
  directory::dir_state_t next_state)
{
  // If we are using the directory as a probe filter and we do not find the
  // entry in the directory cache, then we have to broadcast to all caches.
  using namespace directory;
  using namespace rigel::cache;
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER();
    fprintf(stderr, "[PF - START] addr: %08x next_sharer: %3d next_state: %16s\n",
      addr, next_sharer, dir_state_string[next_state]);
  }
  #endif
  // Find an invalid entry to use for the broadcast, otherwise use LRU.  Since
  // this is the probe filter, we just drop the information in the LRU entry.
  int set = get_set(addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  int invalid_index;
  bool found_invalid_entry;
  invalid_index = valid_bits.findNextClearInclusive(startOfSet);
  found_invalid_entry = (invalid_index < endOfSet && invalid_index != -1);
  
  // If we do not find an invalid entry, grab the LRU one.
  directory::dir_entry_t &entry = found_invalid_entry ? 
    dir_array[invalid_index] : get_replacement_entry(set);
  // If a BCAST is in progress, we wait.
  if (entry.get_state() == DIR_STATE_EVICTION_LOCK_READ_BCAST) { 
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[PF - LOCKED ENTRY] addr: %08x n_sharer: %3d n_state: %16s\n",
        addr, next_sharer, dir_state_string[next_state]);
    }
    #endif
    return; 
  }
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER(); fprintf(stderr, "[USING ENTRY] "); entry.dump();
  }
  #endif
  // If the entry we are evicting is in the M state, we send a writeback message
  // to the owner.
  uint32_t prev_addr = entry.get_addr();
  bool was_write_private = (DIR_STATE_WRITE_PRIV == entry.get_state());   
  size_t prev_owner = was_write_private ? entry.get_owner() : 0xFFFFFF;
  // If the next state is not a read shared state, we need to invalidate the
  // entries.  Otherwise, we rebuild the sharer list.
  icmsg_type_t bcast_msg_type = (next_state == DIR_STATE_READ_SHARED) ?
    IC_MSG_SPLIT_BCAST_SHR_REQ : IC_MSG_SPLIT_BCAST_INV_REQ;
  // We now have an entry we can use for the broadcast.
  // Add the entry to the list for tracking purposes.
  add_outstanding_bcast(&entry);
  
  entry.reset();
  // Set the address
  entry.set_addr(addr);
  // Lock the entry
  entry.set_state(DIR_STATE_EVICTION_LOCK_READ_BCAST);
  // Requestor is not added to list, but all other sharers are.
  entry.set_sharers_bcast();
  entry.dec_sharers(next_sharer);
  // Set up the state for after the broadcast completes.
  entry.set_pf_bcast_next(next_state, next_sharer);
  // Number of sharers.
  int shr_cnt = 0;
  // Send out broadcasts to all clusters except the one requesting the data.
  if (rigel::cache::ENABLE_BCAST_NETWORK)
  {
    // One message per broadcast.
    ICMsg wb_msg(bcast_msg_type, entry.get_addr(), 
      std::bitset<rigel::cache::LINESIZE>(), -356, -1, -1, -1, NULL);
    for (size_t cid = 0; cid < (size_t)rigel::NUM_CLUSTERS; cid++) {
      wb_msg.set_bcast_seq(cid, -1, seq_num[cid]);
      shr_cnt++;
    }
    // Do not send message to next sharer.
    wb_msg.add_bcast_skip_cluster(next_sharer);
    // Add check for forcing writebacks on BCASTs.
    if (was_write_private) { wb_msg.set_bcast_write_shootdown(prev_owner, prev_addr); }
    insert_pending_icmsg(wb_msg);
  } else {
    for (size_t cid = 0; cid < (size_t)rigel::NUM_CLUSTERS; cid++) 
    {
      if (cid == next_sharer) { continue; }
      ICMsg wb_msg(IC_MSG_CC_BCAST_PROBE, addr, 
        std::bitset<rigel::cache::LINESIZE>(), cid, -1, -1, -1, NULL);
      // This has no concept of a logical channel, just the current time
      // steps.
      wb_msg.set_seq(-1, seq_num[cid]);
      // Add check for forcing writebacks on BCASTs.
      if (was_write_private && cid == prev_owner) { 
        wb_msg.set_bcast_write_shootdown(prev_owner, prev_addr); 
      }
      // Throttling of messages happens at the G$.  Here we just queue them up.
      insert_pending_icmsg(wb_msg);
      shr_cnt++;
    }
  }
  profiler::stats[STATNAME_DIR_INVALIDATE_BCAST].inc(shr_cnt);
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER();
    fprintf(stderr, "[PF - SUCCESS] addr: %08x n_sharer: %3d n_state: %16s\n",
      addr, next_sharer, dir_state_string[next_state]);
    DEBUG_HEADER(); entry.dump();
  }
  #endif
  return;
}
bool 
CacheDirectory::request_read_permission( uint32_t req_addr, int req_clusterid, 
                                         directory::dir_of_timing_t &timer)
{
  using namespace directory;
  size_t set = get_set(req_addr);

  // Check pending message queue and ignore if a message to the same address is
  // already pending.
  if (true == check_request_pending(req_addr)) {
    DEBUG_HEADER();
    fprintf(stderr, "Request is pending, request_read_permission() failed.\n");

    return false;
  }


  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    // Skip over entries which do not match the address.
    if (!entry.check_addr_match(req_addr)) { continue; }
    // Valid, matching entry found.  Return result of the FSM query.
    if (helper_request_read_permission(req_addr, req_clusterid, entry))
    {
      if(was_invalidated_early(req_addr, (unsigned int)req_clusterid))
      {
        rigel::profiler::stats[STATNAME_DIRECTORY_REREQUESTS].inc_multi_time_histogram(
          1000*(rigel::CURR_CYCLE/1000), set+(gcache_bank_id*rigel::COHERENCE_DIR_SETS), 1);
        remove_eviction_invalidate(req_addr, (unsigned int)req_clusterid);
      }
      return true;
    }
    else
      return false;
  }
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == (req_addr & ~0x1F) && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) { 
    DEBUG_HEADER();
    fprintf(stderr, "[DIR ENTRY NOT FOUND] addr: %08x requester: %d\n", 
      req_addr, req_clusterid);
  }
  #endif

  // Missed in the first-level directory.  Search the overflow.
  if (ENABLE_OVERFLOW_DIRECTORY)
  {
    dir_entry_t entry;

    if (overflow_directory->find_entry_by_addr(req_addr, entry, timer))
    {
      dir_entry_t &victim_entry = get_replacement_entry(set);

      // Swap the one we found with the victim entry. If the entry is not valid, we
      // can just replace it without doing a swap.  TODO: Make sure victim entry is
      // safe to swap out, otherwise we can block.
      if ( !victim_entry.check_valid() ) {
        // victim is not valid, we can just fill into the L1 directory.
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
            DEBUG_HEADER(); fprintf(stderr, "Removing directory entry from overflow.");
            DEBUG_HEADER(); fprintf(stderr, "ENTRY: "); entry.dump();
          }
          #endif
        if (!overflow_directory->remove_dir_entry(req_addr, entry, timer)) {
          assert( 0 && "FIXME?");
        }
      } else {
        // We need to do a swap
        #ifdef __DEBUG_ADDR
        if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
          DEBUG_HEADER(); fprintf(stderr, "Swaping directory entry into overflow.");
          DEBUG_HEADER(); fprintf(stderr, "VICTIM: "); victim_entry.dump();
          DEBUG_HEADER(); fprintf(stderr, "NEW: "); entry.dump();
        }
        #endif
        if (!overflow_directory->swap_dir_entry(req_addr, victim_entry, entry, timer)) {
          // Stalling...touch the victim_entry since it needs to finish.
          victim_entry.set_last_touch();
          return false;
        }
      }
      // What was the victim entry now becomes the swapped out entry. The entry
      // named 'entry' is now safely in the overflow directory.
      victim_entry = entry;
      // Make it MRU.
      victim_entry.set_last_touch();
      // Try to obtain write permission with the now-resident entry.
      if(helper_request_read_permission(req_addr, req_clusterid, victim_entry))
      {
        if(was_invalidated_early(req_addr, (unsigned int)req_clusterid))
        {
          rigel::profiler::stats[STATNAME_DIRECTORY_REREQUESTS].inc_multi_time_histogram(
            1000*(rigel::CURR_CYCLE/1000), set+(gcache_bank_id*rigel::COHERENCE_DIR_SETS), 1);
          remove_eviction_invalidate(req_addr, (unsigned int)req_clusterid);
        }
        return true;
      }
      else
        return false;
    }
    // Not found in overflow directory either.  Proceed with normal eviction.
  }

  if (rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY) 
  {
    // Check to see if we can get a bcast network entry this cycle.
    if (!bcast_network_entry_available()) { return false; }
    helper_probe_filter_bcast(req_addr, req_clusterid, DIR_STATE_READ_SHARED);
    return false;
  }

  // At this point, the entry invalid everywhere with no requests outstanding.
  // Allocate new entry (possibly evicting an old entry).  If there are
  // no invalid entries, do an eviction.
  bool success;
  dir_entry_t &entry = replace_entry(req_addr, success, timer);
  if (success)
  {
    // Entry was replaced and we now have read permission.
    entry.reset();
    entry.set_state(DIR_STATE_READ_SHARED);
    entry.set_addr(req_addr);
    entry.inc_sharers(req_clusterid);
    entry.set_fill_pending(req_clusterid);
    entry.set_last_touch();
    return true;
  }

  // Failed to obtain permission :'(  We probably should never reach here.
  return false;
}

directory::dir_entry_t &
CacheDirectory::replace_entry(uint32_t addr, bool &success, directory::dir_of_timing_t &timer)
{
  using namespace directory;
  using namespace rigel::cache;

  int set = get_set(addr);

  if(dir_pending_replacement_bitvec[set] && (addr != dir_pending_replacement_aggressor_address[set]))
  {
    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[DIRECTORY] Set %d: Addr 0x%08x trying to replace, address 0x%08x already replacing way %d\n",
              set, addr, dir_pending_replacement_aggressor_address[set], dir_pending_replacement_way[set]);      
      get_entry(set, dir_pending_replacement_way[set]).dump();
    }
    #endif
    success = false;
    return dir_array[set*config.num_ways];
  }

  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER();
    fprintf(stderr, "[DIRECTORY] Set %d: Addr 0x%08x is trying to do a replacement\n", set, addr);
  }
  #endif

  #ifdef __DEBUG_ADDR
  if(dir_pending_replacement_bitvec[set])
  {
    if (__DEBUG_ADDR == get_entry(set, dir_pending_replacement_way[set]).get_addr() && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[DIRECTORY] Set %d: Address 0x%08x is trying to evict addr 0x%08x\n", set, addr, __DEBUG_ADDR);
      get_entry(set, dir_pending_replacement_way[set]).dump();
    }
  }
  #endif

  if(!dir_pending_replacement_bitvec[set])
  {
    // First try to find an invalid entry or one with no sharers.
    int startOfSet = set * config.num_ways;
    int endOfSet = startOfSet + config.num_ways;
    int i = valid_bits.findNextClearInclusive(startOfSet);
    if(i < endOfSet && i != -1)
    {
      dir_entry_t &entry = dir_array[i];
      // Ensure that we clean up entries properly on eviction.
      if (0 == entry.get_num_sharers()) {
        assert(entry.get_state() == DIR_STATE_INVALID);
      }
      if (DIR_STATE_INVALID == entry.get_state() )
      {
        #ifdef DEBUG_DIRECTORY
        DEBUG_HEADER();
        fprintf(stderr, "Found invalid entry to use. set: %d way %zu\n", set, (i-startOfSet));
        #endif

        // LRU is reset too.
        entry.reset();
        entry.set_last_touch();
        success = true;
        // With the probe filter, we use this entry to do the broadcast.
        if (ENABLE_PROBE_FILTER_DIRECTORY) { }
        else { return entry; }
      }
      else
      {
        entry.dump();
        assert(0);
      }
    }
    #ifdef DEBUG_DIRECTORY
    DEBUG_HEADER();
    fprintf(stderr, "DID NOT FIND invalid entry to use.\n");
    #endif
  }

  dir_entry_t &entry = (dir_pending_replacement_bitvec[set])  ?
    // 1.) There is a replacement pending for the set.
    get_entry(set, dir_pending_replacement_way[set]) :
    // 2.) At this point, there are no invalid or empty entries we can just nab.
    // We now have to invalidate an entry.  We must send invalidates to all
    // clusters that hold the line.  No new requests to the line can start while
    // an eviction to the line is pending.
    get_replacement_entry(set);

  if(!dir_pending_replacement_bitvec[set])
  {
    // This is the first time we are selecting an entry to evict.
    //
    // This is getting commented out since it kill runs that have a lot of sets
    // and a lot of GCache banks.  We would get about ten hours of run time at
    // eight tiles and 512 sets/8 ways without WayPoint turned on before
    // std::bad_allocs would start getting thrown.
    // I am replacing this functionality with a simple histogram of sharer
    // count.
//    rigel::profiler::stats[STATNAME_DIRECTORY_EVICTIONS].inc_multi_time_histogram(
//      1000*(rigel::CURR_CYCLE/1000), 
//      set+(gcache_bank_id*rigel::COHERENCE_DIR_SETS), 1);
//    rigel::profiler::stats[STATNAME_DIRECTORY_EVICTION_SHARERS].inc_multi_time_histogram(
//      1000*(rigel::CURR_CYCLE/1000), 
//      set+(gcache_bank_id*rigel::COHERENCE_DIR_SETS), entry.get_num_sharers());

    if(rigel::GENERATE_DIREVICTTRACE)
    {
      fprintf(rigel::memory::direvicttrace_out, "%9"PRIu64", %8x, %3u, %2d, ",
              rigel::CURR_CYCLE, entry.get_addr(), entry.get_num_touches(), (int)entry.get_state());
              entry.dump_sharer_vector_hex(rigel::memory::direvicttrace_out);
              fprintf(rigel::memory::direvicttrace_out, "\n");
    }

    for (size_t shr = 0; shr < config.bitvec_size; shr++) {
      if (entry.check_cluster_is_sharing(shr)) {
        add_eviction_invalidate(entry.get_addr(), (unsigned int)shr);
      }
    }

    //Set the bit and aggressor address to make sure nobody jumps in front of you.
    dir_pending_replacement_bitvec[set] = true;
    dir_pending_replacement_way[set] = get_way(entry.get_addr());
    dir_pending_replacement_aggressor_address[set] = addr;
  }


// If the entry has not filled into the global cache yet, we cannot begin to
// invalidate the line.
if (entry.check_fill_pending()) {
  #ifdef __DEBUG_ADDR
  if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
    DEBUG_HEADER();
    fprintf(stderr, "[DIRECTORY] Set %d: addr 0x%08x would replace, but victim addr 0x%08x still has fill pending bit set\n",
            set, addr, entry.get_addr());
    entry.dump();
  }
  #endif  
  success = false;
  return entry;
}

#ifdef __DEBUG_ADDR
if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
  DEBUG_HEADER();
  fprintf(stderr, "[DIRECTORY] Set %d: addr 0x%08x wants to replace victim addr 0x%08x, and victim does not have fill pending bit set\n",
          set, addr, entry.get_addr());
  entry.dump();
}
#endif

  // If there were zero sharers, we would not have to send any messages nor
  // track it, but since we are here, there must be more than zero sharers.
  // Send out the messages and free the entry.  TODO: We may need check for all
  // entries being locked.  If that is the case, we will not be able to handle
  // the replacement this cycle and we will have to return false.
  //assert(entry.get_state() != DIR_STATE_INVALID);

  if(entry.get_state() == DIR_STATE_INVALID)
  {
    #ifdef DEBUG_DIRECTORY
    DEBUG_HEADER();
    fprintf(stderr, "Victim entry for set %d is now invalid, replace_entry() can complete\n", set);
    #endif

    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER();
      fprintf(stderr, "[DIRECTORY] Set %d: Victim addr 0x%08x is now invalid, replace_entry() can complete\n",
              set, entry.get_addr());
    }
    #endif

    // LRU is reset too.
    entry.reset();
    entry.set_last_touch();
    success = true;
    dir_pending_replacement_bitvec[set] = false;
    dir_pending_replacement_aggressor_address[set] = 0xDEADBEEF;
    // With the probe filter, we use this entry to do the broadcast.
    if (ENABLE_PROBE_FILTER_DIRECTORY) { }
    else { return entry; }
  }

  // At this point we know that we have to remove 'entry'.  If the overflow
  // directory is enabled, we add it to the list and invalidate the current
  // entry.  If the overflow directory is not available, we send invalidations
  // out.
  if (ENABLE_OVERFLOW_DIRECTORY) 
  {
    // Disallow pending requests from being kicked if they are already pending.
    switch (entry.get_state()) 
    {
      case DIR_STATE_PENDING_READ:
      case DIR_STATE_PENDING_WRITE:
      case DIR_STATE_PENDING_WB:
      case DIR_STATE_EVICTION_LOCK_WRITE:
      case DIR_STATE_EVICTION_LOCK_READ:
      case DIR_STATE_PENDING_READ_BCAST:
      case DIR_STATE_PENDING_WRITE_BCAST:
      case DIR_STATE_EVICTION_LOCK_READ_BCAST:
      {
        success = false;
        return entry;
      }
      default: break;
    }

    #ifdef __DEBUG_ADDR
    if (__DEBUG_ADDR == addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
      DEBUG_HEADER(); fprintf(stderr, "Inserting directory entry into overflow.");
      DEBUG_HEADER(); entry.dump();
    }
    #endif
    // Entry is inserted into the overflow.
    overflow_directory->insert_dir_entry(entry.get_addr(), entry, timer);
    // FIXME  For now, this takes zero cycles.
    // Remove pending eviction.
    dir_pending_replacement_way[set] = -1;
    dir_pending_replacement_bitvec[set] = false;
    dir_pending_replacement_aggressor_address[set] = 0xDEADBEEF;
    entry.reset();
    entry.set_last_touch();
    success = true;
    return entry;
  } else {
    switch (entry.get_state()) 
    {
      case DIR_STATE_READ_SHARED:
      {
        // If the broadcast bit is set and the number of sharers is greater than
        // zero, we will have to broadcast.
        //FIXME we might want to check for how many times this predicate fails because # sharer == 0,
        // which is an indication of how good software is about "checking in"
        // unused data.
        if (entry.check_bcast() && entry.get_num_sharers() > 0) 
        {
          // Lock the entry
          entry.set_state(DIR_STATE_EVICTION_LOCK_READ_BCAST);
          // Add the entry to the list of outstanding broadcasts
          add_outstanding_bcast(&entry);
          // Requestor is not added to list, but all other sharers are.
          entry.set_sharers_bcast();
          int shr_cnt = 0;
          // Send out broadcasts to all clusters.
          if (rigel::cache::ENABLE_BCAST_NETWORK)
          {
            // One message per broadcast.
            ICMsg wb_msg(IC_MSG_SPLIT_BCAST_INV_REQ, entry.get_addr(), 
              std::bitset<rigel::cache::LINESIZE>(), -357, -1, -1, -1, NULL);
            for (size_t cid = 0; cid < (size_t)rigel::NUM_CLUSTERS; cid++) {
              wb_msg.set_bcast_seq(cid, -1, seq_num[cid]);
              shr_cnt++;
            }
            insert_pending_icmsg(wb_msg);
          } else {
            for (size_t cid = 0; cid < (size_t)rigel::NUM_CLUSTERS; cid++) 
            {
              ICMsg wb_msg(IC_MSG_CC_BCAST_PROBE, entry.get_addr(), 
                std::bitset<rigel::cache::LINESIZE>(), cid, -1, -1, -1, NULL);
              // This has no concept of a logical channel, just the current time
              // steps.
              wb_msg.set_seq(-1, seq_num[cid]);
              // TODO: Throttle how fast we can insert these messages?
              insert_pending_icmsg(wb_msg);
              // Keep count of sharers for profiling.
              shr_cnt++;
            }
          }
          // Add the number of sharers to the histogram for assessing the number of
          // invalidates sent on a state change.  We have this in the eviction code
          // and in the state change code.
          assert(shr_cnt <= rigel::NUM_CLUSTERS && "too many sharers!?");
          profiler::stats[STATNAME_DIR_SHARER_COUNT].inc_multi_histogram(shr_cnt);
          profiler::stats[STATNAME_DIR_INVALIDATE_BCAST].inc(shr_cnt);
        } else {
          int shr_cnt = 0;
          for (size_t shr = 0; shr < config.bitvec_size; shr++) {
            if (entry.check_cluster_is_sharing(shr)) {
              ICMsg invalidate_msg(IC_MSG_CC_INVALIDATE_PROBE, entry.get_addr(), 
                std::bitset<LINESIZE>(), shr, -1, -1, -1, NULL);
              // TODO: Throttle how fast we can insert these messages?
              insert_pending_icmsg(invalidate_msg);
              // Keep count of sharers for profiling.
              shr_cnt++;
            }
          }
          // Add the number of sharers to the histogram for assessing the number of
          // invalidates sent on a state change.  We have this in the eviction code
          // and in the state change code.
          assert(shr_cnt <= rigel::NUM_CLUSTERS && "too many sharers!?");
          profiler::stats[STATNAME_DIR_SHARER_COUNT].inc_multi_histogram(shr_cnt);

          profiler::stats[STATNAME_COHERENCE_EVICTS].inc();
          entry.set_state(DIR_STATE_EVICTION_LOCK_READ);
        }
        // We need to set the last touch to make sure we do not trigger a spurious
        // WDT.  This was a very FML bug.
        entry.set_last_touch();

        // Insert pending eviction.
        dir_pending_replacement_way[set] = get_way(entry.get_addr());
        dir_pending_replacement_bitvec[set] = true;
        dir_pending_replacement_aggressor_address[set] = addr;
        break;
      }
      case DIR_STATE_WRITE_PRIV:
      {
        int curr_owner = entry.get_owner();

        // Send out invalidate to the writer holding the line.
        ICMsg wb_msg(IC_MSG_CC_WR_RELEASE_PROBE, entry.get_addr(), 
          std::bitset<rigel::cache::LINESIZE>(), curr_owner, -1, -1, -1, NULL);
        // TODO: Throttle how fast we can insert these messages?
        insert_pending_icmsg(wb_msg);
        // Add the number of sharers to the histogram for assessing the number of
        // invalidates sent on a state change.  We have this in the eviction code
        // and in the state change code.
        profiler::stats[STATNAME_DIR_SHARER_COUNT].inc_multi_histogram(1);

        profiler::stats[STATNAME_COHERENCE_EVICTS].inc();
        entry.set_state(DIR_STATE_EVICTION_LOCK_WRITE);
        // We need to set the last touch to make sure we do not trigger a spurious
        // WDT.  This was a very FML bug.
        entry.set_last_touch();

        // Insert pending eviction.
        dir_pending_replacement_way[set] = get_way(entry.get_addr());
        dir_pending_replacement_bitvec[set] = true;
        dir_pending_replacement_aggressor_address[set] = addr;
        break;
      }
      // If the entry is being evicted and its count drops to zero, return the
      // entry now.
      case DIR_STATE_EVICTION_LOCK_READ_BCAST:
      case DIR_STATE_EVICTION_LOCK_WRITE:
      case DIR_STATE_EVICTION_LOCK_READ:
      {
        if (0 == entry.get_num_sharers()) {
          //For BCASTs, also remove the entry from the outstanding bcast set, then fall through.
          if(entry.get_state() == DIR_STATE_EVICTION_LOCK_READ_BCAST) {
            remove_outstanding_bcast(&entry);
          }
          // Remove pending eviction.
          dir_pending_replacement_way[set] = -1;
          dir_pending_replacement_bitvec[set] = false;
          dir_pending_replacement_aggressor_address[set] = 0xDEADBEEF;
          entry.reset();
          entry.set_last_touch();
          success = true;
          return entry;
        }

        break;
      }
      default:
      {
        // If it is pending or locked, we wait.
      }
    }
  }

  success = false;
  return entry;
}

bool 
CacheDirectory::request_write_permission(uint32_t req_addr, int req_clusterid, 
  directory::dir_of_timing_t &timer)
{
  using namespace directory;
  size_t set = get_set(req_addr);

  // Check pending message queue and ignore if a message to the same address is
  // already pending.
  if (true == check_request_pending(req_addr)) {
    #ifdef DEBUG_DIRECTORY
    DEBUG_HEADER();
    fprintf(stderr, "Request is pending, request_write_permission() failed.\n");
    #endif

    return false;
  }

  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    // Skip over entries which do not match the address.
    if (!entry.check_addr_match(req_addr)) { continue; }
    // Found valid entry that matches.  Check FSM.
    if (helper_request_write_permission(req_addr, req_clusterid, entry))
    {
      if(was_invalidated_early(req_addr, (unsigned int)req_clusterid))
      {
        rigel::profiler::stats[STATNAME_DIRECTORY_REREQUESTS].inc_multi_time_histogram(
          1000*(rigel::CURR_CYCLE/1000), set+(gcache_bank_id*rigel::COHERENCE_DIR_SETS), 1);
        remove_eviction_invalidate(req_addr, (unsigned int)req_clusterid);
      }
      return true;
    }
    else { return false; }
  }

  // Missed in the first-level directory.  Search the overflow.
  if (ENABLE_OVERFLOW_DIRECTORY)
  {
    dir_of_timing_t timer;
    dir_entry_t entry;

    if (overflow_directory->find_entry_by_addr(req_addr, entry, timer))
    {
      dir_entry_t &victim_entry = get_replacement_entry(set);

      // Swap the one we found with the victim entry. If the entry is not valid, we
      // can just replace it without doing a swap.  TODO: Make sure victim entry is
      // save to swap out, otherwise we can block.
      if ( !victim_entry.check_valid() ) {
        // Victim is not valid, we can just fill into the L1 directory.
        #ifdef __DEBUG_ADDR
        if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
          DEBUG_HEADER(); fprintf(stderr, "Removing directory entry from overflow.");
          DEBUG_HEADER(); fprintf(stderr, "VICTIM_ENTRY: "); entry.dump();
        }
        #endif
        if (!overflow_directory->remove_dir_entry(req_addr, victim_entry, timer)) {
          assert( 0 && "FIXME?"); 
        }
      } else {
        // We need to do a swap
        #ifdef __DEBUG_ADDR
        if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
          DEBUG_HEADER(); fprintf(stderr, "Swapping directory entry into overflow.");
          DEBUG_HEADER(); fprintf(stderr, "VICTIM: "); victim_entry.dump();
          DEBUG_HEADER(); fprintf(stderr, "NEW: "); entry.dump();
        }
        #endif
        if (!overflow_directory->swap_dir_entry(req_addr, victim_entry, entry, timer)) {
          // Stalling...
          return false;
        }
      }
      // What was the victim entry now becomes the swapped out entry. The entry
      // named 'entry' is now safely in the overflow directory.
      victim_entry = entry;
      // Make it MRU.
      victim_entry.set_last_touch();
      // Try to obtain write permission with the now-resident entry.
      if (helper_request_write_permission(req_addr, req_clusterid, victim_entry))
      {
        if(was_invalidated_early(req_addr, (unsigned int)req_clusterid))
        {
          rigel::profiler::stats[STATNAME_DIRECTORY_REREQUESTS].inc_multi_time_histogram(
            1000*(rigel::CURR_CYCLE/1000), set+(gcache_bank_id*rigel::COHERENCE_DIR_SETS), 1);
          remove_eviction_invalidate(req_addr, (unsigned int)req_clusterid);
        }
        return true;
      }
      else
        return false;
    }
    // Not found in overflow directory either.  Proceed with normal eviction.
  }

  #ifdef DEBUG_DIRECTORY
  DEBUG_HEADER();
  fprintf(stderr, "[WRITE] No match in directory, try to do replacement.\n");
  #endif
  if (rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY) 
  {
    // Check to see if we can get a bcast network entry this cycle.
    if (!bcast_network_entry_available()) { return false; }
    helper_probe_filter_bcast(req_addr, req_clusterid, DIR_STATE_WRITE_PRIV);
    return false;
  }

  // At this point, the entry invalid everywhere with no requests outstanding.
  // Allocate new entry (possibly evicting an old entry).  If there are
  // no invalid entries, do an eviction.
  bool success;
  dir_entry_t &entry = replace_entry(req_addr, success, timer);
  if (success) 
  {
    // Entry was replaced and we now have read permission.
    entry.reset();
    entry.set_state(DIR_STATE_WRITE_PRIV);
    entry.set_addr(req_addr);
    entry.inc_sharers(req_clusterid);
    entry.set_fill_pending(req_clusterid);
    return true;
  }


  // Failed to obtain permission :'(  We probably should never reach here.
  return false;
}


void 
CacheDirectory::release_read_ownership( uint32_t rel_addr, 
                                        int rel_clusterid, 
                                        icmsg_type_t ic_type, 
                                        directory::dir_of_timing_t &timer,
                                        bool bcast_found_valid_line)
{
  using namespace directory;

  size_t set = get_set(rel_addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (!entry.check_addr_match(rel_addr)) { continue; }
    // Error if this cluster is not sharing.
//    if (!entry.check_cluster_is_sharing(rel_clusterid)) { 
//        fprintf(stderr, "R %d NOT SHARING %08x\n", rel_clusterid, rel_addr); entry.dump(); 
//        break; 
//    }
    // Hand it off to the FSM helper.
    helper_release_read_ownership(rel_addr, rel_clusterid, ic_type, entry, bcast_found_valid_line);
    
    return;
  }

  // Missed in the first-level directory.  Search the overflow.
  if (ENABLE_OVERFLOW_DIRECTORY)
  {
    dir_entry_t entry;

    if (overflow_directory->find_entry_by_addr(rel_addr, entry, timer))
    {
      if (!entry.check_cluster_is_sharing(rel_clusterid)) {
        DEBUG_HEADER(); fprintf(stderr, "BUG!  Found OF directory entry that is unshared!\n");
        DEBUG_HEADER(); fprintf(stderr, "rel_addr: 0x%08x rel_clusterid: %d\n", 
                                        rel_addr, rel_clusterid);
        DEBUG_HEADER(); entry.dump();
        // Drop through to the abort() below since this should not happen. 
      } else {
        // Keep original state around to see what to do with entry after we
        // return.
        dir_state_t state_init = entry.get_state();
        // Sanity check the state.
        switch(state_init) 
        {
          case DIR_STATE_EVICTION_LOCK_WRITE:
          case DIR_STATE_EVICTION_LOCK_READ:
          case DIR_STATE_PENDING_WRITE:
          case DIR_STATE_PENDING_WB:
          case DIR_STATE_PENDING_READ:
          case DIR_STATE_PENDING_READ_BCAST:
          case DIR_STATE_PENDING_WRITE_BCAST:
          case DIR_STATE_EVICTION_LOCK_READ_BCAST:
            // These are unexpected states.  TODO: Disallow these from being
            // placed in the overflow directory.
            rigel::GLOBAL_debug_sim.dump_all();
            entry.dump();
            DEBUG_HEADER();
            fprintf(stderr, "BUG!  We should not find these states in the OF directory!\n");
            assert(0 && "Invalid state found in OF directory.");
          case DIR_STATE_READ_SHARED:
          case DIR_STATE_WRITE_PRIV:
            // These are the states that we expect.
            break;
          default:
            assert(0 && "Unhandled case.");
        }

        // Try to release the line.
        helper_release_read_ownership(rel_addr, rel_clusterid, ic_type, entry);
        // Depending on what happened in the release, we may need to take
        // different actions
        dir_state_t state_new = entry.get_state();
        if (state_new == DIR_STATE_INVALID) {
          // Delete the entry and return.
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == rel_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
            DEBUG_HEADER(); fprintf(stderr, "Removing directory entry from overflow.");
            DEBUG_HEADER(); fprintf(stderr, "ENTRY: "); entry.dump();
          }
          #endif
          overflow_directory->remove_dir_entry(rel_addr, entry, timer);
          return;
        } else if (state_init == DIR_STATE_PENDING_WRITE ||
                   state_init == DIR_STATE_PENDING_READ ||
                   state_init == DIR_STATE_PENDING_WB)
        {
          // For pending actions, if the state changes, we will want to swap
          // this entry into the HW directory.
          if (state_init != state_new)
          {
            assert(0 && "For now this is disallowed.  Add support later.");
          }
          DEBUG_HEADER();
          fprintf(stderr, "TODO: We need to swap the released entry with on in the OF.\n");
          // Update what is present in the OF directory.
          overflow_directory->update_dir_entry(rel_addr, entry, timer);
          return;
        } else {
          // The two states should probably be the same so we can just return.
          // TODO: Add assertion?
          // Update what is present in the OF directory.
          overflow_directory->update_dir_entry(rel_addr, entry, timer);
          return;
        }
      }
    }
  }

  // There may be messages that arrive at the directory that are to non-existent lines.
  if (rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY) { return; }

  // We should never reach this point.  A release message should only come from
  // clusters that had ownership to begin with.  Getting here and not finding
  // ownership means that we lost track of the correct state somewhere.
  DEBUG_HEADER();
  fprintf(stderr, "BUG!  Releasing read ownership of an unowned line! ");
  fprintf(stderr, "rel_addr: 0x%08x rel_clusterid: %d\n", rel_addr, rel_clusterid);
  dump();
  rigel::GLOBAL_debug_sim.dump_all();
  assert(0);
}


void 
CacheDirectory::release_write_ownership(uint32_t rel_addr, int rel_clusterid, icmsg_type_t ic_type, 
  directory::dir_of_timing_t &timer)
{
  using namespace directory;

  size_t set = get_set(rel_addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (!entry.check_addr_match(rel_addr)) { continue; }
    // Error if this cluster is not sharing--you can get these from PF runs
    // though.
    if (!entry.check_cluster_is_sharing(rel_clusterid)) { 
      //   fprintf(stderr, "W %d NOT SHARING %08x\n", rel_clusterid, rel_addr); 
      //   entry.dump(); 
      break; 
    }
    // Skip out when in BCAST mode since the BCASTS will handle this.
    if (entry.get_state() == DIR_STATE_EVICTION_LOCK_READ_BCAST) { 
      if (!rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY) {
        GLOBAL_debug_sim.dump_all();
        entry.dump();
        fprintf(stderr, "Is this even possible?\n");
        assert(0);
      }
      return; 
    }
    if (entry.get_state() == DIR_STATE_PENDING_WRITE_BCAST) { return; }
    // Write permission may be held by only one cluster at a time.
    if (1 != entry.get_num_sharers()) { 
      rigel::GLOBAL_debug_sim.dump_all();
      DEBUG_HEADER();
      fprintf(stderr, "More than one sharer on a line that is being write released! ");
      fprintf(stderr, " rel_addr: %08x rel_clusterid: %4d msg_type: %d\n", 
                rel_addr, rel_clusterid, ic_type);
      assert(0);
    }
    // Hand off the entry to the helper
    helper_release_write_ownership(rel_addr, rel_clusterid, ic_type, entry);
    // TODO: Does this need to generate an ACK back to the cluster?
    return;
  }

  // Missed in the first-level directory.  Search the overflow.
  if (ENABLE_OVERFLOW_DIRECTORY)
  {
    dir_entry_t entry;

    if (overflow_directory->find_entry_by_addr(rel_addr, entry, timer))
    {
      if (!entry.check_cluster_is_sharing(rel_clusterid)) {
        DEBUG_HEADER();
        fprintf(stderr, "BUG!  Found OF directory entry that is unshared!\n");
        // Drop through to the abort() below since this should not happen. 
      } else {
        // Keep original state around to see what to do with entry after we
        // return.
        dir_state_t state_init = entry.get_state();
        // Sanity check the state.
        switch(state_init)
        {
          case DIR_STATE_EVICTION_LOCK_WRITE:
          case DIR_STATE_EVICTION_LOCK_READ:
          case DIR_STATE_PENDING_WRITE:
          case DIR_STATE_PENDING_WB:
          case DIR_STATE_PENDING_READ:
          case DIR_STATE_PENDING_READ_BCAST:
          case DIR_STATE_PENDING_WRITE_BCAST:
          case DIR_STATE_EVICTION_LOCK_READ_BCAST:
            // These are unexpected states.  TODO: Disallow these from being
            // placed in the overflow directory.
            rigel::GLOBAL_debug_sim.dump_all();
            entry.dump();
            DEBUG_HEADER();
            fprintf(stderr, "BUG!  We should not find these states in the OF directory!\n");
            assert(0 && "Invalid state found in OF directory.");
          case DIR_STATE_READ_SHARED:
          case DIR_STATE_WRITE_PRIV:
            // These are the states that we expect.
            break;
          default:
            assert(0 && "Unhandled case.");
        }

        // Try to release the line.
        helper_release_write_ownership(rel_addr, rel_clusterid, ic_type, entry);
        // Depending on what happened in the release, we may need to take
        // different actions
        dir_state_t state_new = entry.get_state();
        if (state_new == DIR_STATE_INVALID) {
          // Delete the entry and return, we are done.
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == rel_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
            DEBUG_HEADER(); fprintf(stderr, "Removing directory entry from overflow.");
            DEBUG_HEADER(); fprintf(stderr, "ENTRY: "); entry.dump();
          }
          #endif
          overflow_directory->remove_dir_entry(rel_addr, entry, timer);
          return;
        } else if (state_init == DIR_STATE_PENDING_WRITE ||
                   state_init == DIR_STATE_PENDING_READ ||
                   state_init == DIR_STATE_PENDING_WB)
        {
          // For pending actions, if the state changes, we will want to swap
          // this entry into the HW directory.
          if (state_init != state_new) 
          {
            assert(0 && "For now this is disallowed.  Add support later.");
          }
          // Update what is present in the OF directory.
          overflow_directory->update_dir_entry(rel_addr, entry, timer);
          return;
        } else {
          // The two states should probably be the same so we can just return.
          // TODO: Add assertion?
          // Update what is present in the OF directory.
          overflow_directory->update_dir_entry(rel_addr, entry, timer);
          return;
        }
      }
    }
  }

  // There may be messages that arrive at the directory that are to non-existent lines.
  if (rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY) { return; }

  // We should never reach this point.  A release message should only come from
  // clusters that had ownership to begin with.  Getting here and not finding
  // ownership means that we lost track of the correct state somewhere.
  DEBUG_HEADER();
  rigel::GLOBAL_debug_sim.dump_all();
  fprintf(stderr, "BUG!  Releasing write ownership of an unowned line! ");
  fprintf(stderr, "rel_addr: 0x%08x rel_clusterid: %d\n", rel_addr, rel_clusterid);
  assert(0);
}

bool 
CacheDirectory::check_read_permission(uint32_t req_addr, int req_clusterid, 
  directory::dir_of_timing_t &timer)
{
  using namespace directory;

  size_t set = get_set(req_addr);

  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (entry.check_addr_match(req_addr)) {
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[CHECK PERM] READ num_sharers: %d addr: %08x "
                        " nak_waitlist.count(): %2zu state: %s\n", 
          entry.get_num_sharers(), req_addr, entry.nak_waitlist.count(req_clusterid), 
          dir_state_string[entry.get_state()]);
      }
      #endif
      // Should we also add write sharing? Nope.  A read permission check to a
      // write shared line is a bad thing.
      if (DIR_STATE_READ_SHARED == entry.get_state()) {
        if (entry.check_cluster_is_sharing(req_clusterid)) 
        {
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
            DEBUG_HEADER();
            fprintf(stderr, "[FILL PENDING] SET READ req_clusterid: %3d addr: %08x "
                            " state: %s\n", 
              req_clusterid, req_addr, dir_state_string[entry.get_state()]);
          }
          #endif
          if (0 == entry.fill_pending.count(req_clusterid)) { entry.set_fill_pending(req_clusterid); }
          entry.set_last_touch();
          return true;
        } else { return false; }
      } else { return false; }
    }
  }

  // Missed in the first-level directory.  Search the overflow.
  if (ENABLE_OVERFLOW_DIRECTORY)
  {
    dir_entry_t entry;

    if (overflow_directory->find_entry_by_addr(req_addr, entry, timer))
    {
      // Should we also add write sharing? Nope.  A read permission check to a
      // write shared line is a bad thing.
      if (DIR_STATE_READ_SHARED == entry.get_state()) {
        if (entry.check_cluster_is_sharing(req_clusterid)) 
        {
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
            DEBUG_HEADER();
            fprintf(stderr, "[FILL PENDING] SET READ req_clusterid: %3d addr: %08x "
                            " state: %s\n", 
              req_clusterid, req_addr, dir_state_string[entry.get_state()]);
          }
          #endif
          entry.set_fill_pending(req_clusterid);
          entry.set_last_touch();
          return true;
        }
      }
    }
  }

  // Not read sharing :'(
  return false;
}

bool 
CacheDirectory::check_request_pending(uint32_t req_addr) 
{
  using namespace directory;

  size_t set = get_set(req_addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (entry.check_addr_match(req_addr)) 
    {
      // If the owner is waiting on a fill, then subsequent requests must block.
      if (entry.check_fill_pending()) { return true; }
      // Found a valid address match.
      switch(entry.get_state()) {
        case DIR_STATE_PENDING_READ:
        case DIR_STATE_PENDING_WRITE:
        case DIR_STATE_PENDING_WB:
        case DIR_STATE_EVICTION_LOCK_WRITE:
        case DIR_STATE_EVICTION_LOCK_READ:
        case DIR_STATE_PENDING_WRITE_BCAST:
        case DIR_STATE_PENDING_READ_BCAST:
        case DIR_STATE_EVICTION_LOCK_READ_BCAST:
        {
          // Debug check to make sure we do no thave an entry locked at the
          // directory for too long. 
          if ((rigel::CURR_CYCLE - entry.get_last_touch()) > rigel::cache::DIRECTORY_STATE_WDT)
          {
            rigel::GLOBAL_debug_sim.dump_all();
            fprintf(stderr, "SHARERS: ");
            entry.sharer_vec.print(stderr);
            fprintf(stderr, "\n");
            DEBUG_HEADER();
            fprintf(stderr, "Request pending at directory for greater than: %10" PRIu64 " cycles "
                            " to address: 0x%08x\n", rigel::cache::DIRECTORY_STATE_WDT, req_addr);
            entry.dump();
            assert(0);
          }
          return true;
        }
        default:
          return false;
      }
    }
  }
  // Entry not found at all.
  return false;
}

bool
CacheDirectory::check_write_permission(uint32_t req_addr, int req_clusterid, 
  directory::dir_of_timing_t &timer) 
{
  using namespace directory;

  size_t set = get_set(req_addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (entry.check_addr_match(req_addr)) 
    {
      #ifdef __DEBUG_ADDR
      if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
        DEBUG_HEADER();
        fprintf(stderr, "[CHECK PERM] WRITE num_sharers: %d addr: %08x "
                        " nak_waitlist.count(): %2zu state: %s\n", 
          entry.get_num_sharers(), req_addr, entry.nak_waitlist.count(req_clusterid), 
          dir_state_string[entry.get_state()]);
      }
      #endif

      // Should we also add write sharing? Nope.  A read permission chcek to a
      // write shared line is a bad thing.
      if (DIR_STATE_WRITE_PRIV == entry.get_state()) {
        if (entry.check_cluster_is_sharing(req_clusterid)) 
        { 
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
            DEBUG_HEADER();
            fprintf(stderr, "[FILL PENDING] SET WRITE req_clusterid: %3d addr: %08x "
                            " state: %s\n", 
              req_clusterid, req_addr, dir_state_string[entry.get_state()]);
          }
          #endif
          if (0 == entry.fill_pending.count(req_clusterid)) { entry.set_fill_pending(req_clusterid); }
          entry.set_last_touch();
          return true; 
        } else { return false; }
      } else { return false; }
    }
  }

  // Missed in the first-level directory.  Search the overflow.
  if (ENABLE_OVERFLOW_DIRECTORY)
  {
    dir_entry_t entry;
  
    if (overflow_directory->find_entry_by_addr(req_addr, entry, timer))
    {
      // Should we also add write sharing? Nope.  A read permission check to a
      // write shared line is a bad thing.
      if (DIR_STATE_WRITE_PRIV == entry.get_state()) 
      {
        if (entry.check_cluster_is_sharing(req_clusterid)) 
        { 
          #ifdef __DEBUG_ADDR
          if (__DEBUG_ADDR == req_addr && (__DEBUG_CYCLE < rigel::CURR_CYCLE)) {
            DEBUG_HEADER();
            fprintf(stderr, "[FILL PENDING] SET WRITE req_clusterid: %3d addr: %08x "
                            " state: %s\n", 
              req_clusterid, req_addr, dir_state_string[entry.get_state()]);
          }
          #endif
          entry.set_fill_pending(req_clusterid);
          return true; 
        }
      }
    }
  }
  
  // Not write sharing :'(
  return false;
}

void
CacheDirectory::write_to_read_perm_downgrade(uint32_t req_addr, int req_clusterid, 
      directory::dir_of_timing_t &timer)
{
  using namespace directory;

  size_t set = get_set(req_addr);
  int startOfSet = set * config.num_ways;
  int endOfSet = startOfSet + config.num_ways;
  for(int i = valid_bits.findNextSetInclusive(startOfSet);
      ((i != -1) && (i < endOfSet));
      i = valid_bits.findNextSet(i))
  {
    dir_entry_t &entry = dir_array[i];
    if (entry.check_addr_match(req_addr)) 
    {
      if (DIR_STATE_WRITE_PRIV != entry.get_state()
        || !entry.check_cluster_is_sharing(req_clusterid))
      {
        rigel::GLOBAL_debug_sim.dump_all();
        DEBUG_HEADER();
        fprintf(stderr, "Entry exists: addr: 0x%08x cluster: %3d\n", req_addr, req_clusterid);
        assert(0 && "M->S downgrade on a non-M line!\n");
      }
      entry.set_state(DIR_STATE_READ_SHARED);
      return;
    }
  }

  // Missed in the first-level directory.  Search the overflow.
  if (ENABLE_OVERFLOW_DIRECTORY)
  {
    dir_entry_t entry;
  
    if (overflow_directory->find_entry_by_addr(req_addr, entry, timer))
    {
      if (DIR_STATE_WRITE_PRIV != entry.get_state()
        || !entry.check_cluster_is_sharing(req_clusterid))
      {
        rigel::GLOBAL_debug_sim.dump_all();
        DEBUG_HEADER();
        fprintf(stderr, "Entry exists: addr: 0x%08x cluster: %3d\n", req_addr, req_clusterid);
        assert(0 && "M->S downgrade on a non-M line!\n");
      }
      entry.set_state(DIR_STATE_READ_SHARED);
      return;
    }
  }
  
  // ERROR: M->S downgrade to a line not held in the directory.
  rigel::GLOBAL_debug_sim.dump_all();
  DEBUG_HEADER();
  fprintf(stderr, "Entry does not exist: addr: 0x%08x cluster: %3d\n", req_addr, req_clusterid);
  assert(0 && "M->S downgrade on an untracked line!\n");
}
