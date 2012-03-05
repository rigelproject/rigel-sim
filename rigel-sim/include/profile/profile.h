////////////////////////////////////////////////////////////////////////////////
// profile.h
////////////////////////////////////////////////////////////////////////////////
//
//  Includes definition of Profile and ProfileStat classes which are used to
//  track statistics within the simulator.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _PROFILE_H_
#define _PROFILE_H_

//#define DEBUG_MEMTRACKER

#include "sim.h"
#include "profile_names.h"
#include "instrstats.h"
#include <string>
#include <set>
#include <cstring>
#include <list>
#include <vector>
#include <utility> //For std::pair
#include <stdint.h>
#include <sys/time.h>

class KVMap;

// Track the type of ProfileStat based on accesses.  All ProfileStat instances
// start as NONE.
typedef enum {
  // Invalid type.
  PROF_STAT_NONE,
  // Simple incrementing counter.
  PROF_STAT_INCREMENT,
  // The statistic tracks hits/misses/accesses to caches.
  PROF_STAT_CACHE,
  // Predictor type statistic.  Useful for branch predictors.
  PROF_STAT_PREDICTOR,
  // Keep a histogram of accesses.  
  PROF_STAT_HISTOGRAM,
  // Keep a histogram of accesses.  Keep separate histogram for each entity that
  // is being tracked (e.g., each cluster)
  PROF_STAT_MULTI_HISTOGRAM,
  // Track the number of instructions of a particular type in simulation
  PROF_STAT_INSTR_MIX,
  // Track the number of events for a particular memory location
  PROF_STAT_MEM_HISTOGRAM,
  // Track occurrences of several events over time
  PROF_STAT_MULTI_TIME_HISTOGRAM,
  // Keep a running tally and a count of additions.  Output is the tally divided
  // by the number of times it was incremented.
  PROF_STAT_MEAN_COUNT
} profile_stat_t;

typedef enum {
  PROF_STAT_CONFIG_NONE               = 0x00,
  PROF_STAT_CONFIG_PER_CYCLE          = 0x1UL << 0,
  PROF_STAT_CONFIG_CORE               = 0x1UL << 1,
  PROF_STAT_CONFIG_CLUSTER            = 0x1UL << 2,
  PROF_STAT_CONFIG_TILE               = 0x1UL << 3,
  PROF_STAT_CONFIG_L1ROUTER           = 0x1UL << 4,
  PROF_STAT_CONFIG_L2ROUTER           = 0x1UL << 5,
  PROF_STAT_CONFIG_HITS_PER_K_INSTRS  = 0x1UL << 6,
  // Generate histogram over time for an event 
  PROF_STAT_CONFIG_TIME_HISTOGRAM     = 0x1UL << 7,
  // Track a single event every cycle.  Output a histogram of the number of cycles
  // the event happened N times.
  PROF_STAT_CONFIG_CYCLE_COUNTING_HISTOGRAM  = 0x1UL << 8,
  // Base configuration on hits/misses
  PROF_STAT_CONFIG_HITS               = 0x1UL << 9, 
  PROF_STAT_CONFIG_MISSES             = 0x1UL << 10,
  // Do not dump histograms to STDERR
  PROF_STAT_CONFIG_SUPPRESS_STDERR_DUMP = 0x1UL << 11,
  // Do not count zeros in histograms.
  PROF_STAT_CONFIG_IGNORE_ZEROS         = 0x1UL << 12
} PROF_STAT_CONFIG_T;

typedef enum {
  CLEAN,
  PRIVATE,
  DIRTY,
  SHARED,
  RACED
} share_states_t;

typedef struct share_attr_t {
  uint32_t tasknum;
  uint32_t corenum;
  uint32_t intervalnum;
} ShareAttr;

struct ProfileStat {
  // Constructor implementation moved to profile.cpp
  ProfileStat(std::string name, profile_stat_t _type, uint64_t _config ) ;

  ProfileStat();

  public:
    // Track global cache references in bins
    static rigel::profiler::GCacheOpProfType gcache_hist_stats;
    static rigel::profiler::NetLatProfType net_hist_stats;
    static rigel::profiler::DRAMOpProfType dram_hist_stats;
    // Handle clocking of any profiling statistics that must be done every cycle.
    // Called from sim.cpp every cycle.
    static void PerCycle();

    // List of statistics that should be updated each cycle.
    static std::list<ProfileStat *> per_cycle_accumulate_list;

    // Initialize any global members of the profiler.
    static void init(FILE * fp = stderr);

    // Check if the profiler is active.
    static bool is_active() { return active; }

    // Increment the counter for the statistics.  These methods also determine
    // the type.  Do not mix different accessor/type pairs.  
    void inc() { if (active && !disable_prof_stat) { count.total++; } }
    void inc(int x) { if (active && !disable_prof_stat) { count.total += x; } }
    // Used for counted increment type.
    void counted_inc(int x) { 
       if (active && !disable_prof_stat) { count.total += x; count.total_increments++; } }

    // Cache counters.
    void inc_hits() { if (active && !disable_prof_stat) { count.hits++; } }
    void inc_misses() { if (active && !disable_prof_stat) { count.misses++; } }
    void inc_prefetch_hits() { if (active && !disable_prof_stat) { count.prefetch_hits++; } }
    void inc_prefetch_misses() { if (active && !disable_prof_stat) { count.prefetch_misses++; } }

    // Branch predictor counters.
    void inc_pred_hits() { if (active && !disable_prof_stat) { count.pred_hits++; } }
    void inc_pred_misses() { if (active && !disable_prof_stat) { count.pred_misses++; } }
    void inc_pred_correct() { if (active && !disable_prof_stat) { count.pred_correct++; } }
    void inc_pred_mispredicts() { if (active && !disable_prof_stat) { count.pred_mispredict++; } }
    void inc_pred_total() { if (active && !disable_prof_stat) { count.pred_total++; } }

    // Histogram counters.
    void inc_histogram() { if (active && !disable_prof_stat) 
      { count.total++; count.this_cycle[0]++; }}
    void inc_histogram(int c) { if (active && !disable_prof_stat) 
      { count.total += c; count.this_cycle[0] += c; }}

    void inc_multi_histogram(int index) { 
      if (active && !disable_prof_stat) { 
        count.total++; 
        count.this_cycle[index]++; 
      }
    }
    // Add an instruction to the commit count
    static void inc_instr_commit_count(instr_t type) { /*if (active)*/ instr_commit_count[type]++; }
    // Return the value of the commit profiler.
    static uint64_t get_instr_commit_count(instr_t type) { return instr_commit_count[type]; }
    // Memory histogram
    void inc_mem_histogram_many(uint32_t addr, uint64_t amt) { 
      if (active && !disable_prof_stat) { mem_histogram[addr & rigel::cache::LINE_MASK]+=amt; } 
    }
    void inc_mem_histogram(uint32_t addr) {
      if (active && !disable_prof_stat) { mem_histogram[addr & rigel::cache::LINE_MASK]++; } 
    }
    // FIXME: Should have histograms like MEM_HISTOGRAM (not as a function of
    // time) but with variable size bins.  e.g. this is used for MC_LATENCY,
    // where you want to track how many requests had which latency without
    // rounding to the nearest 32 cycles.
    void inc_mem_histogram(uint32_t value, uint32_t granularity)
    {
      if(active && !disable_prof_stat) { mem_histogram[value/granularity]++; }
    }

    void inc_multi_time_histogram(uint64_t timebin, uint32_t item, uint64_t increment)
    {
      if(active && !disable_prof_stat)
      {
        //if not found, insert new vector
        std::map<uint64_t, std::vector<uint64_t> >::iterator it;
        it = multi_time_histogram.find(timebin);
        if(it == multi_time_histogram.end())
        {
          multi_time_histogram.insert(std::pair<uint64_t, std::vector<uint64_t> >(
            timebin,
            std::vector<uint64_t>(num_items)));
        }
        multi_time_histogram[timebin][item] += increment;
        count.total += increment;
      }
    }

    // Change the binning frequency for the memory histogram. Not yet
    // implemented.  For now we just do a summation of counts. Note in
    // BugTracker.
    void mem_histogram_set_interval(int count);

    // Histogram data on a per-cycle basis.  Called from the PerCycle method.
    void per_cycle_accumulate();

    // Print the statistic.  Used on profiler dump().
    void print();

    uint64_t total() {return count.total;}

    void set_num_items(uint32_t i) { num_items = i; }

    // Turn on and off the counters dynamically.
    static void activate() { active = true; }
    static void deactivate() { active = false; }
    // Histogram data structure incremented each cycle.
    std::map< uint64_t, uint64_t > stat_histogram;
    // Used to track memory histogram (addr, count)
    std::map< uint32_t, uint64_t > mem_histogram;
    // Used to track 3D time histograms (histogram multiple events over time)
    std::map< uint64_t, std::vector<uint64_t> > multi_time_histogram;
    // Used to track time-based histograms of events. (cycle, count)
    std::map< uint64_t, uint64_t > time_histogram;

    // Increment the number of retired instructions for the thread.
    static void inc_retired_instrs(int tid) {
      if (active && !disable_prof_stat) {retired_instrs[tid]++; }
    }
    static uint64_t get_retired_instrs(int tid) {
      return retired_instrs[tid];
    }
    // The number of instructions retired while the profiler is active.
    static uint64_t *retired_instrs;
    // Return the number of CPU cycles the profiler has been active.
    static uint64_t get_active_cycles() { return active_cycles; }
    // Return the number of DRAM cycles the profiler has been active.
    static uint64_t get_active_dram_cycles() { return ProfileStat::active_dram_cycles; }

    ////////////////////////////////////////////////////////////////////////////
    // for memory access tracking
    // A new WrtShareTrack element is created for each unique address tracked.
    // written in this interval means origWrtTask is valid, and reads are local/private
    //   else reads are InputRd and origWrtTask is not relevant 
    struct WrtShareTrack{
      // interval when last write occurred, output wrts can span multiple intervals
      // set to -1 if address was never written
      int  lastIntervalWritten;
      // Tasknum of the last wrt in current interval, qualify by writtenThisInterval
      // updated with each write, so multiple conflicts can be detected
      int lastWrtTask;        
      // set if written during the current interval, to differentiate Input Rd
      bool writtenThisInterval;         
      // during the current interval, find data potential races
      bool readThisInterval;            
      int lastRdTask;
      // flag whether this addr marked as output wrt; cleared by a killing wrt, 
      // set to 1 by a read if written during a previous interval
      // declared as int for an aggregating count 
      unsigned int  outputWrt; 

      // read counts
      //unsigned int inputRdCount;
      // list of readers of an output wrt
      std::set<uint32_t> sharedWrtList;  // track unique readers of program wrts by Task IDs
      // list tasks for now - do we want to track the number of reads per address also?
      std::set<uint32_t> inputRdList;  // track unique input data readers by Task IDs
      // length of this will match number of outputWrt for each addr
      std::vector<uint32_t> sharedWrtCounts; // for each output wrt, how many readers
      std::set<uint32_t> readConflictPCs;   // track conflict reader PCs
      std::set<uint32_t> writeConflictPCs;  // track conflict writer PCs
      //std::vector< std::set<uint32_t> * > sharedRdHist; // pointers to sharedRdList's

      share_states_t currShareState;
    } ;

    // address being tracked is key, struct for the various count data
    static std::map<uint32_t, WrtShareTrack*> WrtShareTable;
    // buckets are number of sharers, data is the number of writes shared by that many
    static std::map<uint32_t, uint32_t> WrtShareHistrogram;

    static std::map<uint32_t, uint32_t> InputRdHistrogram;

    // used to find barrier crossings, to update memory tracking data
    static uint64_t currentBarrierNum;

    // set of counters to hold final global run totals
    // Writes
    static uint64_t totOutputWrtCnt;    // running total of output wrts
    static uint64_t totConflictWrtCnt;    // running total of conflict wrts
    static uint64_t totPrivateWrtCnt;    // writes that are initially "private"
    static uint64_t totAllocatingWrtCnt;  // first touch of addr is write
    static uint64_t totCleanWrtCnt;    // first writes to an addr during interval (writes to clean addresses)
    static uint64_t totWrtCnt;    // running total of all writes
    // Reads
    static uint64_t totMatchedInputRdCnt;    // running total of input rd (matched to program writes)
    static uint64_t totUnMatchedInputRdCnt;    // running total of input rd (unmatched) usually kernel input data
    static uint64_t totConflictRdCnt;    // running total of conflict rd
    static uint64_t totPrivateRdCnt;    // running total of all task-private reads
    static uint64_t totAllocatingRdCnt;    // first-touch reads;special case of unmatched input rd
    static uint64_t totRdCnt;    // running total of all reads
    static uint64_t totRdOnly;    // running total of rdonly data, input data never written by kernel
    static uint64_t sharedRdrCnt;    // running total of shared readers, updated at each new interval with
    // filtering (task=-1) traffic not assigned to a given task (init/startup/enqueue traffic)
    static uint64_t bogusTaskNumRd;   
    static uint64_t bogusTaskNumWr; 
    // stack access counters (mem operations to stack region)
    static uint64_t totTaskStackRdCnt;    
    static uint64_t totTaskStackWrCnt;    
    // RTM access counters (mem operations generated by RTM)
    static uint64_t tot_RTM_RdCnt;    
    static uint64_t tot_RTM_WrCnt;    
    // global and atomic counters
    static uint64_t tot_global_reads;
    static uint64_t tot_global_writes;
    static uint64_t tot_atomics;
    // locks
    static uint64_t tot_lock_reads;
    static uint64_t tot_lock_writes;
    // Races by state
    static uint64_t totRaces;

    // compare barrier to current, clear table and lists, update aggregating counts as needed
    static void barrierWrtTrack(bool final = false) ;

    // build the tracking table with address as the key, and WrtShareTrack* struct
    // as the value.  Checks if addr is in the table
    static void MemWrtTrack(uint32_t addr, uint32_t write_pc, int task,
                            bool is_global = false, bool is_atomic = false) ;

    // updates rd counts as input or private. Input rd defines an output wrt, updates those too
    static void MemRdTrack(uint32_t addr,  uint32_t read_pc, int task,
                            bool is_global = false, bool is_atomic = false) ;

    // print out counts for all tracked addresses - may want a file arg?
    static void dumpWrtTrackStats(  );
    static void updateWrtShareHistrogram(WrtShareTrack* WT);
    static void updateInputRdHistrogram(WrtShareTrack* WT);
    static void dumpWrtShareHistrogram();
    static void dumpInputRdHistogram();
    //  end of code added for memory access tracking
    ////////////////////////////////////////////////////////////////////////////

    // print global hist stats that were migrated from Profile class to this class
    static void dumpHistStats(  );

  private:
    struct Count{
      Count()
      {
        total=0;
        total_increments=0;
        last_total=0;
        hits=0;
        misses=0;
        prefetch_hits=0;
        prefetch_misses=0;
        pred_mispredict=0;
        pred_correct=0;
        pred_total=0;
        pred_hits=0;
        pred_misses=0;
      }

      uint64_t total;
      uint64_t total_increments;
      // Keep track of the last value for time-based histogramming.
      uint64_t last_total;
      uint64_t hits;
      uint64_t misses;
      uint64_t prefetch_hits;
      uint64_t prefetch_misses;

      uint64_t pred_mispredict;
      uint64_t pred_correct;
      uint64_t pred_total;
      uint64_t pred_hits;
      uint64_t pred_misses;

      // Use a map to track per-resource histogram.  Default is zero for the
      // regular histogram.  Multi-histogram can use any number of resources.
      std::map< int, uint64_t > this_cycle;
    };
    Count count;

    // How many cycles the profile stat class have been clocked.
    static uint64_t active_cycles;
    // How many DRAM cycles have elapsed while the profiler has been active.
    static uint64_t active_dram_cycles;
		std::string stat_name;
    uint64_t cycles;
    static bool active;
    static FILE *output_file;
    profile_stat_t type;
    //  How many cycles to accumulate time histogram statistics for
    uint32_t time_hist_bin_size;

    // Configuration options
    uint64_t config;
    // For MULTI_TIME_HISTOGRAMS
    uint32_t num_items;
    // Allow the ProfSat class to be totally gated. TODO
    static const bool disable_prof_stat;
    // Track the number of commits to each instruction type.
    static uint64_t instr_commit_count[I_MAX_INSTR];

  public:
    static void increment_active_dram_cycles() { active_dram_cycles++; }
};


/* Update with each type of instruction.  Only one update per each cyle */
typedef struct instr_mix_t {
  uint32_t branch;
  uint32_t shift;
  uint32_t logic;
  uint32_t arith;
  uint32_t special;
  uint32_t fp;
  uint32_t load;
  uint32_t store;
} InstrMix;


/* Timing statistics for simulator */
typedef struct instr_stat_t {
                            /* Where updated:          */
  uint64_t tot_num_fetch_instrs;  /*   fetch_cb()            */
  uint64_t ret_num_instrs;  /*   writeback_cb()        */
  uint64_t tot_usec;        /*  end_sim()              */
  uint64_t clk_ticks_sim;    /*  main loop, per_sim()  */  
  uint64_t clk_ticks_host;  /*  end_sim()              */
  double ipc;                /*  end_sim()              */
  double sim_slowdown;      /*  end_sim()              */
  // Stalls+active+bubble == total number of cycles
  // sum(Active)/stages == total number of instructions 
  // Stalls: Instruction is in this stage but must wait due to some sort of
  //           hazard  
  // Active: The unit is doing what it is supposed to
  // Bubble: There is no valid instruction in this slot
  // halt_stall: Slots wasted waiting on halt
  // # of instructions executed must equal # retired
  uint64_t if_stall;       /*  core_system::fetch()*/
  uint64_t if_block;  
  uint64_t if_active;  
  uint64_t if_bubble;  
  uint64_t dc_stall;        /*  core_system::decode()*/
  uint64_t dc_block;  
  uint64_t dc_active;  
  uint64_t dc_bubble;  
  uint64_t ex_stall;        /*  core_system::execute()*/
  uint64_t ex_block;  
  uint64_t ex_active;  
  uint64_t ex_bubble;  
  uint64_t mc_stall;        /*  core_system::mem()*/
  uint64_t tq_stall;        /*  core_system::mem() - track the number of stall  */
                            /*  cycles separately  from mc */
  uint64_t mc_block;  
  uint64_t mc_active;  
  uint64_t mc_bubble;  
  uint64_t fp_stall;        /*  core_system::mem()*/
  uint64_t fp_block;  
  uint64_t fp_active;  
  uint64_t fp_bubble;  
  uint64_t cc_stall;        /*  core_system::mem()*/
  uint64_t cc_block;  
  uint64_t cc_active;  
  uint64_t cc_bubble;  
  uint64_t wb_stall;        /*  writeback_cb()        */
  uint64_t wb_block;  
  uint64_t wb_active;  
  uint64_t wb_bubble;  
  uint64_t halt_stall;

  uint64_t ex_badpath;      /*  core_system::execute()*/
  // Tabulate branch statistics at the end of execute()
  uint64_t branches_correct;
  uint64_t branches_mispredicted;  
  uint64_t branches_total;  
  // Track the number of hits in the BTR
  uint64_t branch_target_register_hit;

  uint64_t st_queued, ld_queued;
  // Simpler mechanism for counting IPC
  uint64_t issue_slots_total;
  uint64_t issue_slots_filled;
  // Count the number of times both issue slots are filled.
  uint64_t zero_issue_cycles;
  uint64_t one_issue_cycles;
  uint64_t two_issue_cycles;
} InstrStat;

/* Timing statistics for the task queues and tasks in general*/
typedef struct task_stat_t {
  uint64_t cycles_blocked;
  // Only track *successful* instructions here
  uint32_t tq_enqueue_count;
  uint32_t tq_dequeue_count;
  uint32_t tq_loop_count;
  uint32_t tq_init_count;
  uint32_t tq_end_count;
  // Count of times the TQ is overflowed
  uint32_t tq_overflow;

  uint32_t sched_with_cache_affinity;
  uint32_t sched_without_cache_affinity;
} TaskStat;


///////////////////////////////////////////////////////////////////////////////
// cache_stat_t
///////////////////////////////////////////////////////////////////////////////
// Collects various cache statistics
typedef struct cache_stat_t {
  // L1 I-cache stats
  uint64_t L1I_cache_hits;
  uint64_t L1I_cache_misses;

  // L1 D-cache stats
  uint64_t L1D_cache_read_hits;
  uint64_t L1D_cache_read_misses;
  uint64_t L1D_cache_write_hits;
  uint64_t L1D_cache_write_misses;

        // L2 I-cache stats
        uint64_t L2I_cache_hits;
  uint64_t L2I_cache_misses;

  // L2 cache stats
  uint64_t L2_cache_hits;
  uint64_t L2_cache_misses;
  // L2 read accesses
  uint64_t L2_cache_read_hits;
  uint64_t L2_cache_read_misses;
  // L2 write accesses
  uint64_t L2_cache_write_hits;
  uint64_t L2_cache_write_misses;
  // L2 instr / data accesses
  uint64_t L2_cache_instr_misses;
  uint64_t L2_cache_data_misses;

  uint64_t L2_cache_memory_barriers;
  // Number of cycles spent waiting to Fill() a request at the cluster cache but
  // failing because we are unable to evict anything this cycle
  uint64_t L2_cache_fill_stall_cycles;

  // GCache cache stats
  uint64_t GCache_read_hits;
  uint64_t GCache_read_misses;
  uint64_t GCache_write_hits;
  uint64_t GCache_write_misses;
  uint64_t GCache_instr_misses;
  uint64_t GCache_data_misses;

  // Stall cycles
  // XXX: Do we want to differentiate between read misses and write misses?
  // MSHR_stall_cycles happen when there are no mshrs available at the level being
  // tracked.  Usually this means that mshr_full() returned true.  It will get
  // incremented every cycle the core stalls on it since the core will keep
  // reissuing the request until it is successful.  
  //
  // The pending_misses field tells us when a miss happens that is covered by an
  // outstanding miss generated by onther core.  We deduce this from the fact
  // that there is a pending request in-flight when we miss in the cache.  It is
  // possible that we will need to augment this should we speculatively issue
  // loads to the memory system since they could be squashed, but an MSHR could
  // be inserted.  
  uint64_t L1D_cache_stall_cycles;
  uint64_t L1I_cache_stall_cycles;
  uint64_t L1I_pending_misses;      //DONE
  uint64_t L1I_MSHR_stall_cycles;    // Added after every mshr_full() that returns true
  uint64_t L1D_pending_misses;      //DONE
  uint64_t L1D_MSHR_stall_cycles;    // Added after every mshr_full() that returns true
  // L2I + L2D == Total L2 stall cycles
  uint64_t L2I_cache_stall_cycles;
  uint64_t L2D_cache_stall_cycles;
  uint64_t L2_cache_stall_cycles;
  uint64_t L2I_pending_misses;    // DONE
  uint64_t L2I_MSHR_stall_cycles;  // Added after every mshr_full() that returns true
  uint64_t L2D_pending_misses;    // DONE
  uint64_t L2D_MSHR_stall_cycles;  // Added after every mshr_full() that returns true
  // GCache stall counts
  uint64_t GCacheI_stall_cycles;
  uint64_t GCacheD_stall_cycles;
  uint64_t GCache_stall_cycles;
  uint64_t GCacheD_pending_misses;    // DONE
  uint64_t GCacheD_MSHR_stall_cycles;  // Added after every mshr_full() that returns true
  uint64_t GCacheI_pending_misses;    // DONE
  uint64_t GCacheI_MSHR_stall_cycles;  // Added after every mshr_full() that returns true

  // XXX: These should probably be moved elsewhere
  // Ever time a load is attempted and stalls, increment.  Second value is the
  // total number of accesses not cycles
  uint64_t load_stall_total, load_accesses;
  uint64_t prefetch_line_stall_total, prefetch_line_accesses;
  uint64_t store_stall_total, store_accesses;
  // Every time an atomic instruction is stalled, increment
  uint64_t atomic_stall_total, atomic_accesses;
  // Track the number of Cluster Cache misses that are to stack/frame addresses
  uint64_t L2_stack_misses;
  // Count the number of cycles that end with the cluster having no free mshr's
  uint64_t CCache_mshr_full;
  // Count the number of cycles cores spend waiting on memory barriers to complete
  uint64_t CCache_memory_barrier_stalls;

} CacheStat;

typedef struct network_stat_t {
  // How many requests were made to inject into the network from the cluster.
  // This number will be greater than or equal to the number of injects. 
  uint64_t msg_injects_reqs;
  uint64_t msg_injects;
  uint64_t msg_removes;
  uint64_t network_occupancy;
  uint64_t gcache_wait_time;
  uint64_t gcache_wait_time_gchit;
  uint64_t gcache_wait_time_gcmiss;
  uint64_t msg_removes_read;
  uint64_t network_occupancy_read;
  uint64_t gcache_wait_time_read;
} NetworkStat;

typedef struct mem_stat_t {
  void init()
  {
    waiting_cycles = new uint64_t [rigel::DRAM::CONTROLLERS];
    memset(&(waiting_cycles[0]), 0, (size_t)rigel::DRAM::CONTROLLERS*sizeof(uint64_t));
    read_bank = new uint64_t **[rigel::DRAM::CONTROLLERS];
    write_bank = new uint64_t **[rigel::DRAM::CONTROLLERS];
    activate_bank = new uint64_t **[rigel::DRAM::CONTROLLERS];
    for(unsigned int i = 0; i < rigel::DRAM::CONTROLLERS; i++)
    {
      read_bank[i] = new uint64_t *[rigel::DRAM::RANKS];
      write_bank[i] = new uint64_t *[rigel::DRAM::RANKS];
      activate_bank[i] = new uint64_t *[rigel::DRAM::RANKS];
      for(unsigned int j = 0; j < rigel::DRAM::RANKS; j++)
      {
        read_bank[i][j] = new uint64_t[rigel::DRAM::BANKS];
        write_bank[i][j] = new uint64_t[rigel::DRAM::BANKS];
        activate_bank[i][j] = new uint64_t[rigel::DRAM::BANKS];
        memset(&(read_bank[i][j][0]), 0, (size_t)rigel::DRAM::BANKS*sizeof(uint64_t));
        memset(&(write_bank[i][j][0]), 0, (size_t)rigel::DRAM::BANKS*sizeof(uint64_t));
        memset(&(activate_bank[i][j][0]), 0, (size_t)rigel::DRAM::BANKS*sizeof(uint64_t));
      }
    }
    initDone = true;
  }
  ~mem_stat_t()
  {
    if(initDone)
      {
      for(unsigned int i = 0; i < rigel::DRAM::CONTROLLERS; i++)
      {
        for(unsigned int j = 0; j < rigel::DRAM::RANKS; j++)
        {
          delete [] read_bank[i][j];
          delete [] write_bank[i][j];
          delete [] activate_bank[i][j];
        }
        delete [] read_bank[i];
        delete [] write_bank[i];
        delete [] activate_bank[i];
      }
      delete [] read_bank;
      delete [] write_bank;
      delete [] activate_bank;
      delete [] waiting_cycles;
    }
  }
  // Track how many accesses go to each bank.  Two can happen per cycle (DDR)
  uint64_t *** read_bank;
  uint64_t *** write_bank;
  uint64_t *** activate_bank;
  uint64_t * waiting_cycles;
  bool initDone;
} MemStat;

typedef struct global_cache_stat_t {
  // Tracked for each bank

  // Number of GC hits (excluding prefetches)
  uint64_t GCache_hits[rigel::MAX_GCACHE_BANKS];           
  // Number of GC misses (excluding prefetches)
  uint64_t GCache_misses[rigel::MAX_GCACHE_BANKS];         
  // Prefetches that return immediately to CCache
  uint64_t GCache_prefetch_hits[rigel::MAX_GCACHE_BANKS];  
  // Prefetches that generate DRAM accesses
  uint64_t GCache_prefetch_misses[rigel::MAX_GCACHE_BANKS];  
  
  // Track the number of requests at the GCache each cycle
  uint64_t GCache_pending_buffer_full_cycles;
  uint64_t GCache_pending_buffer_empty_cycles;
  uint64_t GCache_pending_buffer_notempty_notfull_cycles;
  // Sum of lengths of the buffer for use in finding mean length of GCache
  // RequestBuffer
  uint64_t GCache_pending_buffer_length_sum;
} GlobalCacheStat;

// Timer structures
typedef struct Timer_t {
  bool enabled;            // whether timer is active/enabled/started
  bool valid;              // whether the data in this timer is valid 
  uint64_t start_time;    // most recent start time
  uint64_t start_retired_instr_count;    // For tracking instructions committed
                                        //  across all cores
  // total time accumulated since last timer was cleared
  // does NOT include time since last starttimer
  // total elapsed time = accumulated_time + (CURR_CYCLE-start_time)
  uint64_t accumulated_time;  
  uint64_t accumulated_retired_instr;
} Timer_t;

// Data structure for tracking stall time on a per-PC basis
typedef struct profile_instr_stall_t {
  public:
    // Constructor
    profile_instr_stall_t() : 
      dynamic_count(0), 
      stall_cycle_count(0),
      l1d_hits(0), l2d_hits(0), l3d_hits(0),
      l1i_hits(0), l2i_hits(0) 
      {cycles.init(); counts.init();}
    // Add an occurance of the instruction retiring.
    void add_latency(uint32_t latency) {
      dynamic_count++;
      // Accumulate for only this PC.
      stall_cycle_count += latency;
      // Accumulate for all instructions.
      total_stall_cycle_count += latency;
    }
    // Add memory hit information.
    void add_icache(bool l1i_hit, bool l2i_hit) {
      if (l1i_hit) l1i_hits++;
      if (l2i_hit) l2i_hits++;
    }
    // Add memory hit information.
    void add_mem(bool l1d_hit, bool l2d_hit, bool l3d_hit) {
      if (l1d_hit) l1d_hits++;
      if (l2d_hit) l2d_hits++;
      if (l3d_hit) l3d_hits++;
    }

    // Add per-instruction stall cycle information.
    void add_stalls(InstrCycleStats stalls) {
      cycles.add_cycles(stalls);
      counts.add_core(stalls);
    }

    // Dump the average latency for the PC
    float get_latency() const { 
      return (1.0f*stall_cycle_count) / dynamic_count;
    }
    // Return the number of occurances of this PC
    uint64_t get_count() const {
      return (uint64_t)(dynamic_count);
    }
    // Return the total number of cycles spent executing this instruction.
    uint64_t get_total_cycles() const {
      return stall_cycle_count;
    }
    uint64_t get_l1d_hits() const { return l1d_hits; }
    uint64_t get_l2d_hits() const { return l2d_hits; }
    uint64_t get_l3d_hits() const { return l3d_hits; }
    uint64_t get_l1i_hits() const { return l1i_hits; }
    uint64_t get_l2i_hits() const { return l2i_hits; }
    // Return the percent of total execution.
    float get_percent() const { 
      return (1.0f*stall_cycle_count) / total_stall_cycle_count; 
    }

    InstrCycleStats cycles;
    InstrCycleStats counts;

  private:
    // How many times the PC was seen;
    uint64_t dynamic_count;
    // The number of cycles it was stalled
    uint64_t stall_cycle_count;
    // L1D, L2D Hits
    uint64_t l1d_hits, l2d_hits, l3d_hits;
    // L1I, L2I Hits
    uint64_t l1i_hits, l2i_hits;
    // Keep track of all cycles.
    static uint64_t total_stall_cycle_count;

} profile_instr_stall_t; 

class Profile {
  public:

    static void handle_network_stats(uint32_t addr, icmsg_type_t type);

    Profile(rigel::GlobalBackingStoreType *Memory, int cluster_id);

    void start_sim();
    void end_sim();

    void PerCycle() { timing_stats.clk_ticks_sim++; }

    //Cluster Local Stats
    InstrStat timing_stats;
    CacheStat cache_stats;
    //system mem stats
    static MemStat mem_stats;

    // timers
    static Timer_t SystemTimers[rigel::NUM_TIMERS]; 

    // KV Map for building up CouchDB JSON object.
    static KVMap kvmap;
    
    void accumulate_stats();

    //Functions for handling stat values at the end of the simulation.
    //These functions should print the key/value pair to the screen,
    //and insert it into the JSON object if CouchDB support is enabled.
static void PRINT_STAT_INT(std::string name, const uint64_t value, FILE *out = stderr, bool dumpToKV = true) {
  PRINT_STAT_INT(name.c_str(), value, out, dumpToKV);
}
static void PRINT_STAT_STRING(std::string name, const char *value, FILE *out = stderr, bool dumpToKV = true, bool quoteInKV = true) {
  PRINT_STAT_STRING(name.c_str(), value, out, dumpToKV, quoteInKV);
}
static void PRINT_STAT_STRING(std::string name, std::string value, FILE *out = stderr, bool dumpToKV = true, bool quoteInKV = true) {
  PRINT_STAT_STRING(name.c_str(), value.c_str(), out, dumpToKV, quoteInKV);
}
static void PRINT_STAT_BOOL(std::string name, const bool value, FILE *out = stderr, bool dumpToKV = true) {
  PRINT_STAT_BOOL(name.c_str(), value, out, dumpToKV);
}
static void PRINT_STAT_FLOAT(std::string name, const double value, FILE *out = stderr, bool dumpToKV = true) {
  PRINT_STAT_FLOAT(name.c_str(), value, out, dumpToKV);
}
static void PRINT_HEADER(std::string name, FILE *out = stderr) {
  PRINT_HEADER(name.c_str(), out);
}
static void PRINT_STAT_INT(const char *name, const uint64_t value, FILE *out = stderr, bool dumpToKV = true);
static void PRINT_STAT_STRING(const char *name, const char *value, FILE *out = stderr, bool dumpToKV = true, bool quoteInKV = true);
static void PRINT_STAT_BOOL(const char *name, const bool value, FILE *out = stderr, bool dumpToKV = true);
static void PRINT_STAT_FLOAT(const char *name, const double value, FILE *out = stderr, bool dumpToKV = true);
static void PRINT_HEADER(const char *name, FILE *out = stderr);
    
    static void global_dump_profile(int argc, char *argv[]);
    // Dump the statistics to a CouchDB database.
    static void couchdb_dump_profile();
    // Taking care of a few stats about the machine itself, time the job was launched, etc.
    static void helper_meta_stats(int argc, char *argv[]);
    
    //Global Stats
    //These Stats will remain untouched until the end of simulation
    //All the clusters will accumulate their stats into these variables
    // NOBODY outside the class should write these except GCache (ideally these
    // would be private)
    static InstrStat global_timing_stats;
    static CacheStat accum_cache_stats;

    // Task stats are only modified by the task queue.  This may have a
    // per-cluster component when we add distributed queues, but this can be a
    // central repository for task stats
    static TaskStat global_task_stats;

    // Information related to the tile and global interconnect
    static NetworkStat global_network_stats;
    // Data particular to the global cache shared by all
    static GlobalCacheStat global_cache_stats;
    // Track stall time on a per-PC basis
    static std::map< uint32_t, profile_instr_stall_t> per_pc_stats;
    static void per_pc_stat_add_icache(uint32_t pc, bool l1i_hit, bool l2i_hit) {
      if (ProfileStat::is_active()) { 
        per_pc_stats[pc].add_icache(l1i_hit, l2i_hit); 
      }
    }
    static void per_pc_stat_add_dcache(uint32_t pc, bool l1d_hit, bool l2d_hit, bool l3d_hit) {
      if (ProfileStat::is_active()) { 
        per_pc_stats[pc].add_mem(l1d_hit, l2d_hit, l3d_hit);
      }
    }
    static void per_pc_stat_add_latency(uint32_t pc, uint32_t latency) {
      if (ProfileStat::is_active()) { 
        per_pc_stats[pc].add_latency(latency);
      }
    }
    static void per_pc_stat_add_stalls(uint32_t pc, InstrCycleStats stalls) {
      if (ProfileStat::is_active()) {
        per_pc_stats[pc].add_stalls(stalls);
      }
    }
    static void accumulate_per_pc_stall_cycles();
    static void accumulate_stall_cycles(InstrCycleStats ics);


  private:
    int cluster_num;
    bool active;
    InstrMix fetch_instr_mix;
    InstrMix retire_instr_mix;

    /* TIMING */
#ifndef _WIN32
    struct timeval te_start, te_end;
#else
	uint64_t te_start, te_end;
#endif
    tsc_t tsc_start, tsc_end;
    void calc_times(instr_stat_t *ts);

    bool sim_running;

    int num_ckpts;
    rigel::GlobalBackingStoreType *backing_store;

    // Called at startup
    //void init();
    /* Called on clean up */
    void dump_profile();

};

inline void 
ProfileStat::per_cycle_accumulate() 
{
  // If statistics are gated, do nothing.
  if (disable_prof_stat) { return; }

  std::map< int, uint64_t >::iterator iter;
  // Bin the number of events!
  if (config & PROF_STAT_CONFIG_TIME_HISTOGRAM) {
    // If the bin is ready, accumulate.  Then we reset the counter for next
    // time.  Which counter we use is determined by the state type.
    uint64_t curr_count;

    switch (type) {
      case PROF_STAT_INCREMENT: 
      case PROF_STAT_HISTOGRAM: 
        curr_count = count.total; 
        break;
      case PROF_STAT_CACHE: 
        if (config & PROF_STAT_CONFIG_HITS) {
          curr_count =  count.hits;
        } else if (config & PROF_STAT_CONFIG_MISSES) {
          curr_count = count.misses; 
        } else {
          assert(0 && "ERROR: You must select PROF_STAT_CONFIG_MISSES *or* PROF_STAT_CONFIG_HITS");
        }
        break;
      default:
        assert(0 && "ERROR: Unknown PROF_STAT type!");
        break;
    }
  
    if ( (rigel::CURR_CYCLE == 0) || ((rigel::CURR_CYCLE % time_hist_bin_size) == 0)) {
      time_histogram[rigel::CURR_CYCLE] = curr_count - count.last_total;
//DEBUG_HEADER();      
//fprintf(stderr, "cycle: %lld curr_count %lld last_count: %lld\n", 
//  count.last_total,
//  curr_count,
//  count.last_total);
      count.last_total = curr_count;
    }
  }

  if(config & PROF_STAT_CONFIG_CYCLE_COUNTING_HISTOGRAM)
  {
    stat_histogram[count.this_cycle[0]]++;
    count.this_cycle[0] = 0;
  }
  else
  {
    for (iter = count.this_cycle.begin(); iter != count.this_cycle.end(); iter++)
    {
      // Skip zero counts if desired.
      if (!((0 == (*iter).second) && (config & PROF_STAT_CONFIG_IGNORE_ZEROS))) {
        // Add the total to the histogram.
        //stat_histogram[(*iter).second]++;
        stat_histogram[(*iter).first] += (*iter).second;
      }
      // Reset the counter for the next cycle.
      (*iter).second = 0;
    }
  }
}
#endif
