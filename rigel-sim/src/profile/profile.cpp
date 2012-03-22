////////////////////////////////////////////////////////////////////////////////
// profile.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the Profile and ProfileStat classes that are used for
//  tracking statistics inside the simulator.  ProfileStat and Profile have a
//  large amount of static members that are used to track statistics in one
//  centralized location.  The Profile and ProfileStat classes also handle
//  printing out the gathered statistics at the end of a run.
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu64, PRIu32
#include <math.h>                       // for pow
#include <netdb.h>                      // for addrinfo, gai_strerror, etc
//For getting the user's name
#include <pwd.h>                        // for getpwuid, passwd
#include <stddef.h>                     // for NULL, size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include <sys/socket.h>                 // for AF_UNSPEC, SOCK_STREAM
#include <sys/time.h>                   // for gettimeofday, timeval
#include <unistd.h>                     // for gethostname, getuid, etc
//For computing hashes of the RigelSim binary and benchmark binary
#include <zlib.h>                       // for crc32
#include <cstdio>                       // for fprintf, stdout, snprintf, etc
#include <cstdlib>                      // for exit, free
#include <cstring>                      // for memset
#include <fstream>                      // for operator<<, basic_ostream, etc
#include <iomanip>                      // for operator<<, setfill, etc
#include <iostream>                     // for std::cerr
#include <list>                         // for list, _List_iterator, etc
#include <map>                          // for map, _Rb_tree_iterator, etc
#include <set>                          // for set
//For finding the hostname
#include <sstream>                      // for stringstream, ostringstream
#include <string>                       // for string, char_traits, etc
#include <utility>                      // for pair
#include <vector>                       // for vector, etc
#include "caches_legacy/global_cache.h"  // for GlobalCache
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"
#include "memory/dram.h"           // for BURST_SIZE, CONTROLLERS, etc
#include "memory/dram_channel_model.h"  // for DRAMChannelModel, etc
#include "util/event_track.h"    // for EventTrack, global_tracker
#include "instrstats.h"     // for InstrCycleStats
#include "util/kvmap.h"          // for KVMap
#include "overflow_directory.h"  // for OverflowCacheDirectory
#include "profile/profile.h"        // for ProfileStat, Profile, etc
#include "profile/profile_names.h"  // for ::STATNAME_L2IN_TOTAL, etc
#include "sim.h"            // for DUMPFILE_PATH, NUM_TILES, etc
#include "util/task_queue.h"     // for TaskSystemBaseline
#include "memory/backing_store.h"  // for definition of GlobalBackingStoreType

#ifdef ENABLE_COUCHDB
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include "couchdb_simple.h"
#endif

cache_stat_t Profile::accum_cache_stats;
instr_stat_t Profile::global_timing_stats;
task_stat_t Profile::global_task_stats;
mem_stat_t Profile::mem_stats;
network_stat_t Profile::global_network_stats;
global_cache_stat_t Profile::global_cache_stats;
KVMap Profile::kvmap;

bool rigel::profiler::PROFILE_HIST_GCACHEOPS;

// Track the number of commits to each instruction type.
uint64_t ProfileStat::instr_commit_count[I_MAX_INSTR];

// Track global cache references in bins
rigel::profiler::GCacheOpProfType ProfileStat::gcache_hist_stats;
rigel::profiler::NetLatProfType ProfileStat::net_hist_stats;
rigel::profiler::DRAMOpProfType ProfileStat::dram_hist_stats;

// Number of cycles in each bin
int rigel::profiler::gcacheops_histogram_bin_size;

// Track per-PC statistics
std::map< uint32_t, profile_instr_stall_t > Profile::per_pc_stats;

bool rigel::profiler::CMDLINE_DUMP_PER_PC_STATS;
std::string rigel::profiler::DUMPFILE_PREFIX;

uint64_t *rigel::profiler::ccache_access_histogram;
uint64_t *rigel::profiler::icache_access_histogram;
// Accumulated count of all stall cycles across all PCs and instructions.
uint64_t profile_instr_stall_t::total_stall_cycle_count;

// List of profiler statistics.
ProfileStat *rigel::profiler::stats;

// Initially the statics profiler is turned off.  Handled in init().
bool ProfileStat::active;
// Gate all profiler statistics.
const bool ProfileStat::disable_prof_stat = false;

// Number of cycles the profiler is active.  Set to zero in init().
uint64_t ProfileStat::active_cycles;
uint64_t ProfileStat::active_dram_cycles;
// File to dump statistics to.  Set to stderr by default in init().
FILE * ProfileStat::output_file;

// List of statistics that should be accumulated and histogrammed each cycle.
std::list<ProfileStat *> ProfileStat::per_cycle_accumulate_list;

// Array of the number of retired instructions for each thread while profiler is
// active. Initialized in sim.cpp.
uint64_t *ProfileStat::retired_instrs;

// Used to find barrier crossings, to update memory tracking date.
uint64_t ProfileStat::currentBarrierNum;
std::map< uint32_t, ProfileStat::WrtShareTrack* > ProfileStat::WrtShareTable;
std::map< uint32_t, uint32_t > ProfileStat::WrtShareHistrogram;
std::map< uint32_t, uint32_t > ProfileStat::InputRdHistrogram;

// global shared tracking totals for a sim
uint64_t ProfileStat::totRdOnly=0;
uint64_t ProfileStat::sharedRdrCnt=0;
uint64_t ProfileStat::totOutputWrtCnt=0;
uint64_t ProfileStat::totConflictWrtCnt=0;
uint64_t ProfileStat::totWrtCnt=0;
uint64_t ProfileStat::totMatchedInputRdCnt=0;
uint64_t ProfileStat::totUnMatchedInputRdCnt=0;
uint64_t ProfileStat::totConflictRdCnt=0;
uint64_t ProfileStat::totRdCnt=0;
uint64_t ProfileStat::totAllocatingRdCnt=0;
uint64_t ProfileStat::totPrivateRdCnt=0;
uint64_t ProfileStat::totPrivateWrtCnt=0; 
uint64_t ProfileStat::totCleanWrtCnt=0; 
uint64_t ProfileStat::totAllocatingWrtCnt=0; 
uint64_t ProfileStat::bogusTaskNumRd=0;
uint64_t ProfileStat::bogusTaskNumWr=0;
uint64_t ProfileStat::totTaskStackRdCnt=0;
uint64_t ProfileStat::totTaskStackWrCnt=0;
uint64_t ProfileStat::tot_RTM_RdCnt=0;
uint64_t ProfileStat::tot_RTM_WrCnt=0;
uint64_t ProfileStat::tot_global_reads=0;
uint64_t ProfileStat::tot_global_writes=0;
uint64_t ProfileStat::tot_atomics=0;
uint64_t ProfileStat::tot_lock_reads=0;
uint64_t ProfileStat::tot_lock_writes=0;
uint64_t ProfileStat::totRaces=0;


////////////////////////////////////////////////////////////////////////////////
/// ProfileStat()
/// 
/// Constructors for ProfileStat struct
/// 
/// Clear counters and mem share tracking counts
////////////////////////////////////////////////////////////////////////////////
ProfileStat::ProfileStat(std::string name, profile_stat_t _type, uint64_t _config = 0) :
  stat_name(name),
  type(_type),
  // Default bin size is 1k cycles for time histogramming.
  time_hist_bin_size(1024) ,
  config(_config)
{
  // initialize WriteTracker 
  // for memory access tracking, init interval number
  currentBarrierNum = rigel::event::global_tracker.GetRTMBarrierCount();
}

ProfileStat::ProfileStat() :
  type(PROF_STAT_NONE), 
  // Default bin size is 10k cycles for time histogramming.
  time_hist_bin_size(10000)
{

  // Zero counters
  memset(&count, 0, sizeof(count));

  // initialize WriteTracker 
  // for memory access tracking, init interval number
  currentBarrierNum = rigel::event::global_tracker.GetRTMBarrierCount();

}

void
ProfileStat::PerCycle() 
{
  // If statistics are gated, do nothing.
  if (disable_prof_stat) return;
  // Track the number of cycles the profiler tracked.  
  if (active) active_cycles++;
  if (active) profiler::stats[STATNAME_PROFILER_ACTIVE_CYCLES].inc();

  // Clock all of the statistics that keep per-cycle accumulators.
  if (active) {
		std::list<ProfileStat *>::iterator iter_stat;

    for (iter_stat = per_cycle_accumulate_list.begin();
         iter_stat != per_cycle_accumulate_list.end(); 
         iter_stat++)
    {
      (*iter_stat)->per_cycle_accumulate();
    }
  }
}


void 
ProfileStat::print()
{
  // If statistics are gated, do nothing.
  if (disable_prof_stat) { return; }

  // Just handle time histograms first.
  switch (type)
  {
    case PROF_STAT_CACHE:
    case PROF_STAT_INCREMENT:
    case PROF_STAT_HISTOGRAM:
    {
      // Dump file of histogram data over time for the event.
      if (config & PROF_STAT_CONFIG_TIME_HISTOGRAM) {
        using namespace rigel::profiler;

        // Open the dump file for the histogram data
        char file_name[1024];

        // FILENAME: <dumpfile-path>/<dumpfile-prefix><name>.csv
        snprintf(file_name, 1024, "%s/%s%s-time.csv", 
          rigel::DUMPFILE_PATH.c_str(), 
          rigel::profiler::DUMPFILE_PREFIX.c_str(),
          stat_name.c_str());
        FILE *f_hist = fopen(file_name, "w");

        assert( NULL != f_hist && "Histogram file open failed");

        std::map< uint64_t, uint64_t >::iterator iter;
        for ( iter = time_histogram.begin(); iter != time_histogram.end(); iter++) {
          fprintf(f_hist, "0x%012llx, %012llu\n", (unsigned long long)iter->first, (unsigned long long)iter->second);
        }

        fclose(f_hist);
      }
      break;
    }
    default:
      /* NADA */ break;
  }

  if (!(config & PROF_STAT_CONFIG_SUPPRESS_STDERR_DUMP))
  {
    switch (type) {
      // Print the count divided by the number of times it was incremented.
      case PROF_STAT_MEAN_COUNT: {
        // The +1 is to avoid DIVZERO errors.
        double mean = count.total / (1.0f * (count.total_increments+1));
        std::stringstream ss(std::stringstream::out);
        ss << stat_name << "_MEAN";
        Profile::PRINT_STAT_FLOAT(ss.str(), mean, output_file);
        std::stringstream ss2(std::stringstream::out);
        ss2 << stat_name << "_COUNT";
        Profile::PRINT_STAT_INT(ss2.str(), count.total_increments, output_file);
        break;
      }
      // Just print the counter.
      case PROF_STAT_INCREMENT: {
        Profile::PRINT_STAT_INT(stat_name, count.total, output_file);
        // Print per-cycle statistics if requested.
        if (config & PROF_STAT_CONFIG_PER_CYCLE) {
          float multiplier = 1.0f;
          // Divide by number of cores, clusters, tiles.
          if (config & PROF_STAT_CONFIG_CORE)    multiplier = rigel::CORES_TOTAL;
          if (config & PROF_STAT_CONFIG_CLUSTER) multiplier = rigel::NUM_CLUSTERS;
          if (config & PROF_STAT_CONFIG_TILE)    multiplier = rigel::NUM_TILES;
          std::stringstream ss(std::stringstream::out);
          ss << stat_name << "_PER_CYCLE";
          Profile::PRINT_STAT_FLOAT(ss.str(), (1.0f * count.total) / (active_cycles * multiplier), output_file);
        }
        break;
      }
      case PROF_STAT_CACHE: {
        float multiplier = 1.0f;
        // Divide by number of cores, clusters, tiles.
        if (config & PROF_STAT_CONFIG_CORE)    multiplier = rigel::CORES_TOTAL;
        if (config & PROF_STAT_CONFIG_CLUSTER) multiplier = rigel::NUM_CLUSTERS;
        if (config & PROF_STAT_CONFIG_TILE)    multiplier = rigel::NUM_TILES;

        // print hit, miss, percent.
        std::stringstream ss(std::stringstream::out);
        ss << stat_name << "_HITS";
        Profile::PRINT_STAT_INT(ss.str(), count.hits, output_file);
        std::stringstream ss2(std::stringstream::out);
        ss2 << stat_name << "_MISSES";
        Profile::PRINT_STAT_INT(ss2.str(), count.misses, output_file);
        std::stringstream ss3(std::stringstream::out);
        ss3 << stat_name << "_HIT_RATE";
        Profile::PRINT_STAT_FLOAT(ss3.str(), (1.0f * count.hits) / (count.hits + count.misses), output_file);
        std::stringstream ss4(std::stringstream::out);
        ss4 << stat_name << "_PREFETCH_HITS";
        Profile::PRINT_STAT_INT(ss4.str(), count.prefetch_hits, output_file);
        std::stringstream ss5(std::stringstream::out);
        ss5 << stat_name << "_PREFETCH_MISSES";
        Profile::PRINT_STAT_INT(ss5.str(), count.prefetch_misses, output_file);
        std::stringstream ss6(std::stringstream::out);
        ss6 << stat_name << "_PREFETCH_HIT_RATE";
        Profile::PRINT_STAT_FLOAT(ss6.str(), (1.0f * count.prefetch_hits) / (count.prefetch_hits + count.prefetch_misses) , output_file);

        // Print per-cycle statistics if requested.
        if (config & PROF_STAT_CONFIG_PER_CYCLE) {
          std::stringstream ss7(std::stringstream::out);
          ss7 << stat_name << "_ACCESSES_PER_CYCLE";
          Profile::PRINT_STAT_FLOAT(ss7.str(),
                                    ((1.0f * (count.hits + count.misses + count.prefetch_hits + count.prefetch_misses))
                                    / (active_cycles * multiplier)), output_file);
        }
        uint64_t sum_retired_instrs = 0;
        for (int i = 0; i < rigel::THREADS_TOTAL; i++) {
          sum_retired_instrs +=retired_instrs[i];
        }
        if (config & PROF_STAT_CONFIG_HITS_PER_K_INSTRS) {
          std::stringstream ss8(std::stringstream::out);
          ss8 << stat_name << "_MISSES_PER_K_INSTRS";
          Profile::PRINT_STAT_FLOAT(ss8.str(),
                                    ((1.0f * (count.misses)) / (sum_retired_instrs / 1000.0f)), output_file);
        }
        break;
      }
      case PROF_STAT_PREDICTOR: {
        // Print the branch predictor information.
        std::stringstream ss(std::stringstream::out);
        ss << stat_name << "_HITS";
        Profile::PRINT_STAT_INT(ss.str(), count.pred_hits, output_file);
        std::stringstream ss2(std::stringstream::out);
        ss2 << stat_name << "_CORRECT";
        Profile::PRINT_STAT_INT(ss2.str(), count.pred_correct, output_file);
        std::stringstream ss3(std::stringstream::out);
        ss3 << stat_name << "_MISPREDICT";
        Profile::PRINT_STAT_INT(ss3.str(), count.pred_mispredict, output_file);
        std::stringstream ss4(std::stringstream::out);
        ss4 << stat_name << "_TOTAL";
        Profile::PRINT_STAT_INT(ss4.str(), count.pred_total, output_file);
        std::stringstream ss5(std::stringstream::out);
        ss5 << stat_name << "_HIT_RATE";
        Profile::PRINT_STAT_FLOAT(ss5.str(), (1.0f * count.pred_hits) / (count.pred_total), output_file);
        std::stringstream ss6(std::stringstream::out);
        ss6 << stat_name << "_PREDICTION_RATE";
        Profile::PRINT_STAT_FLOAT(ss6.str(), (1.0f * count.pred_correct) / (count.pred_total), output_file);
        break;
      }
      case PROF_STAT_HISTOGRAM:
      case PROF_STAT_MULTI_HISTOGRAM: {
        std::stringstream ss(std::stringstream::out);
        ss << stat_name << "_COUNT";
        Profile::PRINT_STAT_INT(ss.str(), count.total, output_file);

        // Spin through all of the histogram bins.
        std::map< uint64_t, uint64_t >::iterator it;
        for (it = stat_histogram.begin();
             it != stat_histogram.end();
             it++)
        {
          std::stringstream ss(std::stringstream::out);
          ss << stat_name << "_COUNT_" << std::setprecision(4) << std::setfill('0') << it->first;
          Profile::PRINT_STAT_INT(ss.str(), it->second, output_file);
        }
        break;
      }
      case PROF_STAT_INSTR_MIX: {
        uint64_t sum_retired_instrs = 0;
        for (int i = 0; i < rigel::THREADS_TOTAL; i++) {
          sum_retired_instrs += retired_instrs[i];
        }
        std::stringstream ss(std::stringstream::out);
        ss << stat_name << "_INSTR_MIX_RATIO";
        Profile::PRINT_STAT_FLOAT(ss.str(), (1.0f * count.total)/sum_retired_instrs, output_file);
        // Print per-cycle statistics if requested.
        if (config & PROF_STAT_CONFIG_PER_CYCLE) {
          float multiplier = 1.0f;
          // Divide by number of cores, clusters, tiles.
          if (config & PROF_STAT_CONFIG_CORE)    multiplier = rigel::CORES_TOTAL;
          if (config & PROF_STAT_CONFIG_CLUSTER) multiplier = rigel::NUM_CLUSTERS;
          if (config & PROF_STAT_CONFIG_TILE)    multiplier = rigel::NUM_TILES;
          std::stringstream ss2(std::stringstream::out);
          ss2 << stat_name << "_INSTR_PER_CYCLE";
          Profile::PRINT_STAT_FLOAT(ss2.str(), (1.0f * count.total) / (active_cycles * multiplier), output_file);
        }
        break;
      }
      // For mem histograms, we dump to a file.
      case PROF_STAT_MEM_HISTOGRAM: {
        using namespace rigel::profiler;

        // Open the dump file for the histogram data
        char file_name[1024];

        // FILENAME: <dumpfile-path>/<dumpfile-prefix><name>.csv
        snprintf(file_name, 1024, "%s/%s%s-mem.csv", 
          rigel::DUMPFILE_PATH.c_str(), 
          rigel::profiler::DUMPFILE_PREFIX.c_str(),
          stat_name.c_str());
        FILE *f_hist = fopen(file_name, "w");

        assert( NULL != f_hist && "Histogram file open failed");

        std::map< uint32_t, uint64_t>::iterator iter;
        for ( iter = mem_histogram.begin(); iter != mem_histogram.end(); iter++) {
          fprintf(f_hist, "0x%08x, %lld\n", (unsigned int)iter->first, (long long signed)iter->second);
        }

        fclose(f_hist);
        break;
      }
      case PROF_STAT_MULTI_TIME_HISTOGRAM: {
        using namespace rigel::profiler;
        std::stringstream ss(std::stringstream::out);
        ss << stat_name << "_TOTAL";
        Profile::PRINT_STAT_INT(ss.str(), count.total, output_file);

        // Open the dump file for the histogram data
        char file_name[1024];

        // FILENAME: <dumpfile-path>/<dumpfile-prefix><name>.csv
        snprintf(file_name, 1024, "%s/%s%s-mem.csv",
          rigel::DUMPFILE_PATH.c_str(),
          rigel::profiler::DUMPFILE_PREFIX.c_str(),
          stat_name.c_str());
        FILE *f_hist = fopen(file_name, "w");

        assert( NULL != f_hist && "Histogram file open failed");

        std::map< uint64_t, std::vector<uint64_t> >::iterator iter;

	    //
	    // First check to see if there are any items in the histogram
	    //

	    if (multi_time_histogram.size() > 0) {
          uint32_t num_items = multi_time_histogram.begin()->second.size();
          for ( iter = multi_time_histogram.begin(); iter != multi_time_histogram.end(); ++iter) {
            fprintf(f_hist, ", %"PRIu64"", iter->first);
          }
          fprintf(f_hist, "\n");
          for(uint32_t k = 0; k < num_items; k++)
          {
            fprintf(f_hist, "%"PRIu32"", k);
            for ( iter = multi_time_histogram.begin(); iter != multi_time_histogram.end(); ++iter) {
              fprintf(f_hist, ", %"PRIu64"", iter->second[k]);
            }
            fprintf(f_hist, "\n");
          }
	    }

        fclose(f_hist);
        break;
      }
      // The counter was never setup?
      case PROF_STAT_NONE:
      default: {
        fprintf(output_file, "%s, !!UNKNOWN!!\n", stat_name.c_str());
        break;
      }
    }
  }
}

static void 
timify(uint64_t count, std::string &s) {
  int seconds = 0, min = 0, hours = 0, days =0;
  const float usec_per_sec =  pow(10.0, 6.0);
  const float sec_per_min = 60;
  const float sec_per_hour = sec_per_min * 60; 
  const float sec_per_day = sec_per_hour * 24;
  char * buf = new char[256];

  // Reset the string to print the runtime into
  s.clear();

  if (true == rigel::HUMAN_READABLE_TIME) {
    days = (count/usec_per_sec) / (sec_per_day);
    count -= days*(sec_per_day)*usec_per_sec;

    hours = (count/usec_per_sec) / (sec_per_hour);
    count -= hours*(sec_per_hour)*usec_per_sec;

    min = (count/usec_per_sec) / (sec_per_min);
    count -= min*sec_per_min*usec_per_sec;
    seconds = (count/usec_per_sec);

    sprintf(buf, "%d days, %d hours, %d minutes, %d seconds",
      days, hours, min, seconds);

  } else {
    sprintf(buf, "%e", count/usec_per_sec);
  }

  s = std::string(buf);

  delete [] buf;
}

////////////////////////////////////////////////////////////////////////////////
// Profile() Constructor
////////////////////////////////////////////////////////////////////////////////
// constructor
// inputs: Memory Model pointer
//         cluster_id
Profile::Profile(rigel::GlobalBackingStoreType *Memory, int cluster_id)
{
  active = rigel::PROFILER_ACTIVE;  
  this->backing_store = Memory;
  cluster_num = cluster_id;
  for(int i=0;i<rigel::NUM_TIMERS;i++){ // init timers
    SystemTimers[i].valid   = false;
    SystemTimers[i].enabled = false;
    SystemTimers[i].start_time       = 0;
    SystemTimers[i].start_retired_instr_count   = 0;
    SystemTimers[i].accumulated_time = 0;
    SystemTimers[i].accumulated_retired_instr = 0;
  }

  //Allocate the 3D arrays for tracking the number of reads, writes, and activates to each DRAM bank
  mem_stats.init();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void 
Profile::start_sim() {
  sim_running = true;
#ifndef _WIN32
  gettimeofday(&te_start, NULL);
#else
  QueryPerformanceCounter((PLARGE_INTEGER)&te_start);
#endif
  RDTSC(&tsc_start);
  /* Reset all of the counters */
  memset(&timing_stats, 0, sizeof(timing_stats));
  memset(&cache_stats, 0, sizeof(cache_stats));
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void 
Profile::end_sim() { 
  if (!sim_running) { return; }

  sim_running = false; 
#ifndef _WIN32
  gettimeofday(&te_end, NULL);
#else
  QueryPerformanceCounter((PLARGE_INTEGER)&te_end);
#endif
  RDTSC(&tsc_end);
  calc_times(&this->timing_stats);
  this->dump_profile();
}

void
Profile::calc_times(instr_stat_t *ts) 
{
#ifndef _WIN32
  uint64_t usec = (te_end.tv_usec - te_start.tv_usec);
  uint64_t sec = (te_end.tv_sec - te_start.tv_sec);
  ts->tot_usec = (1000000ULL*sec) + usec;
#else
  uint64_t ticksPerSecond;
  QueryPerformanceFrequency((PLARGE_INTEGER)&ticksPerSecond);
  uint64_t usec = (te_end-te_start)/(ticksPerSecond/1000000ULL);
  ts->tot_usec = usec;
#endif
  /* clk_ticks_sim DONE */
  uint64_t end_ticks = (((uint64_t) tsc_end.hi << 32) | tsc_end.lo);
  uint64_t start_ticks = (((uint64_t) tsc_start.hi << 32) | tsc_start.lo);
  ts->clk_ticks_host =  end_ticks - start_ticks;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void
Profile::dump_profile() 
{
  InstrStat *ts = &this->timing_stats;
  std::cerr.setf(std::ios::left);
	std::string stime;
  timify(ts->tot_usec, stime); 

  // Print out the instr count file.
  char file_name[1024];
  // FILENAME: <dumpfile-path>/<dumpfile-prefix><name>.csv
  snprintf(file_name, 1024, "%s/%sper-instr-commit.csv", 
    rigel::DUMPFILE_PATH.c_str(), 
    rigel::profiler::DUMPFILE_PREFIX.c_str());
  FILE *f_hist = fopen(file_name, "w");
  for (int i = 0; i < I_MAX_INSTR; i++) {
    fprintf(f_hist, "%s, %s, %" PRIu64 "\n", 
      instr::instr_string[i], INSTR_FUNIT_STRING[i], 
        ProfileStat::get_instr_commit_count((instr_t)i));
  }
  fclose(f_hist);

  if (rigel::DUMP_MEM && (backing_store != NULL)) {
    backing_store->dump_to_file(rigel::MEM_DUMP_FILE);
  }
}

void
Profile::couchdb_dump_profile() 
{
#ifdef ENABLE_COUCHDB
  std::cerr.setf(std::ios::left);

  // Setup the database.
  using namespace curlpp::options;
  using namespace couchdb_simple;
  curlpp::Cleanup myCleanup;

  // Object for interacting with the DB.
  CouchDBSimple couchObj(rigel::COUCHDB_HOSTNAME, rigel::COUCHDB_PORT);
  // The record we will eventually send to the DB for this simulation.
  std::string json_object = kvmap.serialize();

  //Write out to file in case we want to look at it outside the DB, or submit to the DB later
  //in case of failure.
  //Build up filename
  std::ostringstream json_out_filename;
  //TODO: Not Windows-safe.
  json_out_filename << rigel::DUMPFILE_PATH << "/" << "json.txt";
  std::ofstream json_out(json_out_filename.str().c_str());
  if(!json_out.is_open()) {
    fprintf(stderr, "Error, could not open %s\n", json_out_filename.str().c_str());
  }
  else
  {
    json_out << "{ " << json_object << " }";
    json_out.close();
  }
  std::string uuid = CouchDBSimple::GetUUIDString();
  std::cerr << "Inserting into CouchDB UUID: " << uuid;
  std::string result = couchObj.CreateRecord(rigel::COUCHDB_DBNAME, uuid, json_object);
  if(result.compare("") == 0)
    std::cerr << "... failed." << std::"\n";
  else
    std::cerr << "... succeeded." << std::"\n";
#endif
}

void Profile::PRINT_STAT_INT(const char *name, const uint64_t value, FILE *out, bool dumpToKV) {
  fprintf(out, "%s, %"PRIu64"\n", name, value);
  if(dumpToKV)
  {
    std::string k(name);
    std::stringstream v(std::stringstream::out);
    v << value;
    if(Profile::kvmap.insert(k, v.str()) == false)
    {
      fprintf(stderr, "Error: Duplicate Key %s", name);
      assert(0 && "Duplicate Key");
    }
  }
}

//TODO: JSON escaping of quotes, newlines, etc.  For now we just wrap the provided value in quotes.
void Profile::PRINT_STAT_STRING(const char *name, const char *value, FILE *out, bool dumpToKV, bool quoteInKV) {
  //Replace commas with underscores
  std::string commaReplacedValue(value);
  size_t pos = 0;
  while((pos = commaReplacedValue.find_first_of(',', pos)) != std::string::npos && pos < commaReplacedValue.length())
  {
    commaReplacedValue.replace(pos, 1, "_");
    pos++;
  }
  fprintf(out, "%s, %s\n", name, commaReplacedValue.c_str());
  if(dumpToKV)
  {
    std::string k(name), v;
    if(quoteInKV)
      v = "\"";
    v += value;
    if(quoteInKV)
      v += "\"";
    if(Profile::kvmap.insert(k, v) == false)
    {
      fprintf(stderr, "Error: Duplicate Key %s", name);
      assert(0 && "Duplicate Key");
    }
  }
}

void Profile::PRINT_STAT_BOOL(const char *name, const bool value, FILE *out, bool dumpToKV) {
  std::string v(value ? "true" : "false");
  fprintf(out, "%s, %s\n", name, v.c_str());
  if(dumpToKV)
  {
    std::string k(name);
    if(Profile::kvmap.insert(k, v) == false)
    {
      fprintf(stderr, "Error: Duplicate Key %s", name);
      assert(0 && "Duplicate Key");
    }
  }
}

//Replace NaNs with 0.
//TODO: Deal with infinity.
void Profile::PRINT_STAT_FLOAT(const char *name, const double value, FILE *out, bool dumpToKV) {
  fprintf(out, "%s, %f\n", name, value);
  if(dumpToKV)
  {
    std::string k(name);
    std::stringstream v(std::stringstream::out);
    double remappedValue;
    if(value != value) //Will only be true if value is NaN
      remappedValue = 0.0f;
    else
      remappedValue = value;
    v << remappedValue;
    if(Profile::kvmap.insert(k, v.str()) == false)
    {
      fprintf(stderr, "Error: Duplicate Key %s", name);
      assert(0 && "Duplicate Key");
    }
  }
}

void Profile::PRINT_HEADER(const char *name, FILE *out) {
  fprintf(out, "########## %s ##########\n", name);
}

void Profile::helper_meta_stats(int argc, char *argv[]) {
  //Print start and end time.
  //NOTE: Here, we put @s before and after the number of milliseconds since UNIX epoch.
  //The deserialization code should transform "@384983@" to "new Date(384983)" then eval() it.
  std::ostringstream starttime;
  starttime << "@" << rigel::SIM_START_TIME << "@";
  PRINT_STAT_STRING("SIM_START_TIME", starttime.str());
  std::ostringstream endtime;
  endtime << "@" << rigel::SIM_END_TIME << "@";
  PRINT_STAT_STRING("SIM_END_TIME", endtime.str());
  //Print username
  struct passwd *ps = getpwuid(getuid());  // Check for NULL!
  PRINT_STAT_STRING("USER", (ps == NULL) ? "" : ps->pw_name);
  //Concatenate command-line args into a single string
  //TODO: Escaping
  std::ostringstream commandLineArgs;
  for(int i = 0; i < argc; i++)
  {
    commandLineArgs << argv[i] << " ";
  }
  std::ostringstream tags;
  tags << "[";
  PRINT_STAT_STRING("COMMAND_LINE_ARGS", commandLineArgs.str());
  if(rigel::STDIN_AS_STRING_SHORT_ENOUGH)
    PRINT_STAT_STRING("STDIN", rigel::STDIN_AS_STRING.str());
  for(std::vector<std::string>::const_iterator it = SIM_TAGS.begin(), end = SIM_TAGS.end();
      it != end; ++it)
  {
    tags << "\"" << (*it) << "\"";
    if(++it != end)
    {
      tags << ", ";
    }
    --it;
  }
  tags << "]";
  PRINT_STAT_STRING("TAGS", tags.str(), stderr, true, false); //Don't quote the whole string so it gets
  //Inserted into the DB as a JSON array.
  std::ostringstream targetArgs;
  targetArgs << "[";
  for(std::vector<std::string>::const_iterator it = TARGET_ARGS.begin(), end = TARGET_ARGS.end();
      it != end; ++it)
  {
    targetArgs << "\"" << (*it) << "\"";
    if(++it != end)
    {
      targetArgs << ", ";
    }
    --it;
  }
  targetArgs << "]";
  PRINT_STAT_STRING("TARGET_ARGS", targetArgs.str(), stderr, true, false); //Don't quote the whole string so it gets
  //Inserted into the DB as a JSON array.
  //Print hostname (fully qualified if possible)
  struct addrinfo hints, *info, *p;
  int gai_result;

  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; //either IPV4 or IPV6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  if ((gai_result = getaddrinfo(hostname, "http", &hints, &info)) != 0) {
    fprintf(stderr, "error in getaddrinfo(): %s\ncontinuing...\n", gai_strerror(gai_result));
  }
  else {
    for(p = info; p != NULL; p = p->ai_next) {
      std::string hostname_s(p->ai_canonname);
      PRINT_STAT_STRING("HOSTNAME", hostname_s);
      //printf("hostname: %s\n", p->ai_canonname);
      break; //Not sure why I'd want to traverse this linked list.
    }
  }
  free(info);

  //Hash the rigelsim and benchmark binaries and insert them as keys, so we can search for
  //"all runs using this particular benchmark or rigelsim binary" in the DB.
  //1) Load the binaries into byte arrays
  //2) Hash the byte arrays
  //NOTE: I am using an initial CRC seed of 0xffffffff.
  
  //FIXME: This method of finding the RigelSim binary will only work on Unices w/ /proc.
  //There are other solutions for Unices w/o /proc and Windows.  We can employ those later if we need to.
  char rigelsimBinaryName[512];
  ssize_t len = readlink("/proc/self/exe", rigelsimBinaryName, 512);
  if((len == -1) || (len > 511)) {
    fprintf(stderr, "Error: readlink() failed or RigelSim binary name is too long\n");
  }
  else {
    rigelsimBinaryName[len] = '\0';
		std::ifstream fl(rigelsimBinaryName);
    if(!fl.is_open()) { //Could not be found
      fprintf(stderr, "Error: Could not find RigelSim binary at %s\n", rigelsimBinaryName);
    }
    else {
      fl.seekg( 0, std::ios::end );
      size_t len = fl.tellg();
      char *rigelsimBinaryData = new char[len];
      fl.seekg(0, std::ios::beg);
      fl.read(rigelsimBinaryData, len); //META!
      fl.close();
      unsigned long crc32Rigelsim = crc32(0xffffffff, (unsigned char *)rigelsimBinaryData, len);
      PRINT_STAT_STRING("RIGELSIM_BINARY_NAME", rigel::RIGELSIM_BINARY_PATH);
      PRINT_STAT_INT("RIGELSIM_BINARY_SIZE", len);
      PRINT_STAT_INT("RIGELSIM_BINARY_CRC32", crc32Rigelsim);
    }
  }
	std::ifstream fl2(rigel::BENCHMARK_BINARY_PATH.c_str());
  if(!fl2.is_open()) { //Could not be found (less likely, since it's passed as an argument)
    fprintf(stderr, "Error: Could not find benchmark binary at %s\n", rigel::BENCHMARK_BINARY_PATH.c_str());
  }
  else {
    fl2.seekg( 0, std::ios::end );
    size_t len2 = fl2.tellg();
    char *benchmark = new char[len2];
    fl2.seekg(0, std::ios::beg);
    fl2.read(benchmark, len2); //META!
    fl2.close();
    unsigned long crc32Benchmark = crc32(0xffffffff, (unsigned char *)benchmark, len2);
    PRINT_STAT_STRING("BENCHMARK_BINARY_NAME", rigel::BENCHMARK_BINARY_PATH);
    PRINT_STAT_INT("BENCHMARK_BINARY_SIZE", len2);
    PRINT_STAT_INT("BENCHMARK_BINARY_CRC32", crc32Benchmark);
  }
  //Let's try to guess the name of the benchmark as the text between the last / and the next . in the path.
  size_t lastSlashPos = rigel::BENCHMARK_BINARY_PATH.find_last_of('/');
  if(lastSlashPos == std::string::npos) { //No slashes (binary is in current directory)
    lastSlashPos = 0;
  }
  else {
    lastSlashPos++;
  }
  size_t nextDotPos = rigel::BENCHMARK_BINARY_PATH.find_first_of('.', lastSlashPos);
  if(nextDotPos != std::string::npos) {
    PRINT_STAT_STRING("BENCHMARK", rigel::BENCHMARK_BINARY_PATH.substr(lastSlashPos, (nextDotPos-lastSlashPos)));
  }
  else {
    PRINT_STAT_STRING("BENCHMARK", rigel::BENCHMARK_BINARY_PATH.substr(lastSlashPos));
  }

}

/// global_dump_profile()
/// Print out all of the statistics for the simulation run.
void
Profile::global_dump_profile(int argc, char *argv[]) 
{  
  helper_meta_stats(argc, argv);
  if (rigel::PRINT_GLOBAL_TIMING_STATS == false) return;

  // Print the final state of the system on exit.
  if (rigel::PRINT_TERMINAL_SYSTEM_STATE) { 
    rigel::GLOBAL_debug_sim.dump_all();
  }

  InstrStat *ts = &Profile::global_timing_stats;
  CacheStat *cs = &Profile::accum_cache_stats;
  std::cerr.setf(std::ios::left);
	std::string stime;
  timify(ts->tot_usec, stime); 

  if(rigel::profiler::CMDLINE_DUMP_PER_PC_STATS)
    accumulate_per_pc_stall_cycles(); //Otherwise, the stats will be accumulated on a per-instruction basis

  PRINT_HEADER("SIMULATION PARAMETERS");
  {
    using namespace rigel;
    using namespace rigel::cache;
    using namespace rigel::mem_sched;
    using namespace rigel::DRAM;
    
    PRINT_STAT_INT("NUM_TILES", NUM_TILES);
    PRINT_STAT_INT("CLUSTERS_PER_TILE", CLUSTERS_PER_TILE);
    PRINT_STAT_INT("CORES_PER_CLUSTER", CORES_PER_CLUSTER);
    PRINT_STAT_INT("THREADS_PER_CORE", THREADS_PER_CORE);
    PRINT_STAT_INT("ISSUE_WIDTH", ISSUE_WIDTH);
    PRINT_STAT_INT("REGISTERS", NUM_REGS);
    PRINT_STAT_INT("MC_PENDING_PER_BANK", PENDING_PER_BANK);
    PRINT_STAT_INT("DRAM_CLK_RATIO", CLK_RATIO);
    PRINT_STAT_BOOL("MODEL_CONTENTION", CMDLINE_MODEL_CONTENTION);
    PRINT_STAT_BOOL("NONBLOCKING_PREFETCH", CMDLINE_NONBLOCKING_PREFETCH);
    PRINT_STAT_BOOL("NONBLOCKING_LOADS", NONBLOCKING_MEM);
    PRINT_STAT_BOOL("STACK_SMASH_CHECK", CMDLINE_CHECK_STACK_ACCESSES);        
  }
  PRINT_HEADER("INTERCONNECT PARAMETERS");
  {
    using namespace rigel::interconnect;
    using namespace rigel;
    PRINT_STAT_INT("TILENET_GNET2CC_LAT_L1", TILENET_GNET2CC_LAT_L1);
    PRINT_STAT_INT("TILENET_GNET2CC_LAT_L2", TILENET_GNET2CC_LAT_L2);
    PRINT_STAT_INT("TILENET_CC2GNET_LAT_L1", TILENET_CC2GNET_LAT_L1);
    PRINT_STAT_INT("TILENET_CC2GNET_LAT_L2", TILENET_CC2GNET_LAT_L2);
    PRINT_STAT_INT("L1_ROUTER_ENTRIES", L1_ROUTER_ENTRIES);
    PRINT_STAT_INT("L2_ROUTER_ENTRIES", L2_ROUTER_ENTRIES);
    PRINT_STAT_INT("L1_TO_L2_REQUESTS_PER_CYCLE", L1_TO_L2_REQUESTS_PER_CYCLE);
    PRINT_STAT_INT("L2_TO_GNET_REQUESTS_PER_CYCLE", L2_TO_GNET_REQUESTS_PER_CYCLE);
    PRINT_STAT_INT("MAX_ENTRIES_TILE_REQUEST_BUFFER", MAX_ENTRIES_TILE_REQUEST_BUFFER);
    PRINT_STAT_INT("MAX_ENTRIES_TILE_PROBE_REQUEST_BUFFER", MAX_ENTRIES_TILE_PROBE_REQUEST_BUFFER);
    PRINT_STAT_INT("MAX_ENTRIES_TILE_PROBE_RESP_BUFFER", MAX_ENTRIES_TILE_PROBE_RESP_BUFFER);
    PRINT_STAT_INT("MAX_ENTRIES_TILE_REPLY_BUFFER", MAX_ENTRIES_TILE_REPLY_BUFFER);
    PRINT_STAT_INT("CLUSTERS_PER_L1_ROUTER", CLUSTERS_PER_L1_ROUTER);
    PRINT_STAT_INT("MAX_ENTRIES_GCACHE_PROBE_RESP_BUFFER", MAX_ENTRIES_GCACHE_PROBE_RESP_BUFFER);
    PRINT_STAT_INT("MAX_ENTRIES_GCACHE_REQUEST_BUFFER", MAX_ENTRIES_GCACHE_REQUEST_BUFFER);
    PRINT_STAT_INT("MAX_ENTRIES_GCACHE_REPLY_BUFFER", MAX_ENTRIES_GCACHE_REPLY_BUFFER);
    PRINT_STAT_INT("MAX_ENTRIES_GCACHE_PROBE_REQUEST_BUFFER", MAX_ENTRIES_GCACHE_PROBE_REQUEST_BUFFER);
    PRINT_STAT_INT("MAX_GNET_UP_INPUT_MESSAGES_PER_CYCLE", MAX_GNET_UP_INPUT_MESSAGES_PER_CYCLE);
    PRINT_STAT_INT("MAX_GNET_UP_OUTPUT_MESSAGES_PER_CYCLE", MAX_GNET_UP_OUTPUT_MESSAGES_PER_CYCLE);
    PRINT_STAT_INT("MAX_GNET_DOWN_INPUT_MESSAGES_PER_CYCLE", MAX_GNET_DOWN_INPUT_MESSAGES_PER_CYCLE);
    PRINT_STAT_INT("MAX_GNET_DOWN_OUTPUT_MESSAGES_PER_CYCLE", MAX_GNET_DOWN_OUTPUT_MESSAGES_PER_CYCLE);
    PRINT_STAT_INT("GNET_GC2TILE_LAT", GNET_GC2TILE_LAT);
  }
  PRINT_HEADER("CACHE PARAMETERS");
  {
    using namespace rigel::cache;
    using namespace rigel;

    PRINT_STAT_INT("LINE_SIZE", LINESIZE);
    PRINT_STAT_INT("GCACHE_PREFETCH_DEGREE", CMDLINE_GCACHE_PREFETCH_SIZE);
    PRINT_STAT_INT("GCACHE_ACCESS_LATENCY", GCACHE_ACCESS_LATENCY);
    PRINT_STAT_INT("GCACHE_WAYS", GCACHE_WAYS);
    PRINT_STAT_INT("GCACHE_SETS", GCACHE_SETS);
    PRINT_STAT_INT("GCACHE_SIZE", NUM_GCACHE_BANKS * LINESIZE * GCACHE_WAYS * GCACHE_SETS);

    PRINT_STAT_INT("L2D_ACCESS_LATENCY", L2D_ACCESS_LATENCY);
    PRINT_STAT_INT("L2D_WAYS", L2D_WAYS);
    PRINT_STAT_INT("L2D_SETS", L2D_SETS);
    PRINT_STAT_INT("L2D_SIZE", LINESIZE * L2D_WAYS * L2D_SETS);

    PRINT_STAT_INT("L1D_ACCESS_LATENCY", L1D_ACCESS_LATENCY);
    PRINT_STAT_INT("L1D_WAYS", L1D_WAYS);
    PRINT_STAT_INT("L1D_SETS", L1D_SETS);
    PRINT_STAT_INT("L1D_SIZE", LINESIZE * L1D_WAYS * L1D_SETS);

    PRINT_STAT_INT("L1I_ACCESS_LATENCY", L1I_ACCESS_LATENCY);
    PRINT_STAT_INT("L1I_WAYS", L1I_WAYS);
    PRINT_STAT_INT("L1I_SETS", L1I_SETS);
    PRINT_STAT_INT("L1I_SIZE", LINESIZE * L1I_WAYS * L1I_SETS);

    PRINT_STAT_INT("L1D_CACHE_OUTSTANDING_MISSES", L1D_OUTSTANDING_MISSES);
    PRINT_STAT_INT("L1I_CACHE_OUTSTANDING_MISSES", L1I_OUTSTANDING_MISSES );
    PRINT_STAT_INT("L2I_CACHE_OUTSTANDING_MISSES", L2I_OUTSTANDING_MISSES);
    PRINT_STAT_INT("L2_CACHE_OUTSTANDING_MISSES", MAX_L2D_OUTSTANDING_MISSES);
    PRINT_STAT_INT("L2_CACHE_PREFETCH_COUNT", CMDLINE_CCPREFETCH_SIZE);
    PRINT_STAT_INT("GCACHE_OUTSTANDING_MISSES", GCACHE_OUTSTANDING_MISSES);
    PRINT_STAT_BOOL("COUNTED_CCACHE_WRITEBACKS", ENABLE_WRITEBACK_COUNTING);
    PRINT_STAT_INT("COHERENCE_ENABLED", ENABLE_EXPERIMENTAL_DIRECTORY);
    PRINT_STAT_INT("COHERENCE_OVERFLOW_ENABLED", ENABLE_OVERFLOW_DIRECTORY);
    PRINT_STAT_INT("COHERENCE_DIR_SETS", COHERENCE_DIR_SETS);
    PRINT_STAT_INT("COHERENCE_DIR_WAYS", COHERENCE_DIR_WAYS);
    PRINT_STAT_INT("COHERENCE_LIMITED_DIRECTORY_ENABLED", ENABLE_LIMITED_DIRECTORY);
    PRINT_STAT_INT("COHERENCE_LIMITED_DIRECTORY_PTRS", MAX_LIMITED_PTRS);
  }
  PRINT_HEADER("PERFORMANCE STATS");
  {
    PRINT_STAT_INT("OVERFLOW_DIRECTORY_ALLOCS", OverflowCacheDirectory::get_total_allocs());

    for ( int i = 0 ; i < STATNAME_PROFILE_STAT_COUNT; i++) {
      profiler::stats[i].print();
    }

    uint64_t flops =
    profiler::stats[STATNAME_COMMIT_FU_FPU].total()
    + profiler::stats[STATNAME_COMMIT_FU_FPU_VECTOR].total()*4
    + profiler::stats[STATNAME_COMMIT_FU_FPU_SPECIAL].total();
    PRINT_STAT_INT("FLOPS",flops);
    double active_nanoseconds = ((double)ProfileStat::get_active_cycles()/rigel::CLK_FREQ);
    double gflops_per_second = (double)flops/active_nanoseconds;
    PRINT_STAT_FLOAT("GFLOPS", gflops_per_second);
    double peak_gflops = rigel::CLK_FREQ*rigel::CORES_TOTAL;
    PRINT_STAT_FLOAT("PEAK_GFLOPS", peak_gflops);
    PRINT_STAT_FLOAT("GFLOPS_PERCENT_OF_PEAK", (gflops_per_second/peak_gflops)*100.0f);
  }
  PRINT_HEADER("DRAM STATS");
  {
    //Calculate the total number of activations and read/write data BEATS (not bursts) for each
    //DRAM bank in the system.
    uint64_t total_read = 0, total_write = 0, total_activate = 0;
    uint64_t total_waiting = 0;
    for (unsigned int i = 0; i < rigel::DRAM::CONTROLLERS; i++) {
      char stat_name[128];
      snprintf(stat_name, 128, "DRAM_WAITING_CYCLES_%02u", i);
      PRINT_STAT_INT(stat_name, mem_stats.waiting_cycles[i]);
      total_waiting += mem_stats.waiting_cycles[i];
      for(unsigned int j = 0; j < rigel::DRAM::RANKS; j++)
      {
        for(unsigned int k = 0; k < rigel::DRAM::BANKS; k++)
        {
          total_read += mem_stats.read_bank[i][j][k];
          total_write += mem_stats.write_bank[i][j][k];
          total_activate += mem_stats.activate_bank[i][j][k];
        }
      }
    }
    //TODO Hard-coded to 32-bit DRAM channels

    uint64_t num_bursts =
    profiler::stats[STATNAME_DRAM_CMD_READ].total()+profiler::stats[STATNAME_DRAM_CMD_WRITE].total();
    uint64_t num_bytes = num_bursts*32;
    double active_nanoseconds = ((double)ProfileStat::get_active_cycles()/rigel::CLK_FREQ);
    PRINT_STAT_INT("DRAM_TOTAL_ACTIVE_BYTES", num_bytes);
    double peak_bw = (double)rigel::DRAM::CONTROLLERS*rigel::DRAM::CLK_FREQ*4;
    PRINT_STAT_FLOAT("DRAM_PEAK_BW_GBPS", peak_bw);
    double sustained_bw = (double)(num_bytes)/active_nanoseconds;
    PRINT_STAT_FLOAT("DRAM_ACTIVE_SUSTAINED_BW_GBPS", sustained_bw);
    PRINT_STAT_FLOAT("DRAM_ACTIVE_SUSTAINED_BW_PERCENT_OF_PEAK",( sustained_bw/peak_bw)*100.0f);
    uint64_t num_effective_bytes = num_bytes-
    (4*(profiler::stats[STATNAME_GCACHE_HWPREFETCH_TOTAL].total()-profiler::stats[STATNAME_GCACHE_HWPREFETCH_USEFUL].total()));
    double effective_bw = (double)num_effective_bytes/active_nanoseconds;
    PRINT_STAT_FLOAT("DRAM_ACTIVE_EFFECTIVE_BW_GBPS", effective_bw);
    PRINT_STAT_FLOAT("DRAM_ACTIVE_EFFECTIVE_BW_PERCENT_OF_PEAK", (effective_bw/peak_bw)*100.0f);
    double percent_time_nonempty =
    ((double)total_waiting/((double)rigel::DRAM::CONTROLLERS*ProfileStat::get_active_dram_cycles()))*100.0f;
    PRINT_STAT_FLOAT("DRAM_PERCENT_TIME_NONEMPTY", percent_time_nonempty);
    double nonempty_bw = sustained_bw / (percent_time_nonempty/100.0f);
    PRINT_STAT_FLOAT("DRAM_NONEMPTY_BW_GBPS", nonempty_bw);
    PRINT_STAT_FLOAT("DRAM_NONEMPTY_BW_PERCENT_OF_PEAK", (nonempty_bw/peak_bw)*100.0f);
    //FIXME Assume 4 bytes per FU_MEM instruction -- need to be more subtle.
    uint64_t ldst_bytes = profiler::stats[STATNAME_COMMIT_FU_MEM].total()*4;
    PRINT_STAT_FLOAT("CACHE_BW_AMPLIFICATION_FACTOR", (double)ldst_bytes/(double)num_bytes);
    PRINT_STAT_FLOAT("DRAM_ACTIVE_BURSTS_PER_ACTIVATE",
                     ((double)(num_bursts)
                     /(double)profiler::stats[STATNAME_DRAM_CMD_ACTIVATE].total()));
    PRINT_STAT_INT("DRAM_TOTAL_READS", total_read/rigel::DRAM::BURST_SIZE);
    PRINT_STAT_INT("DRAM_TOTAL_WRITES", total_write/rigel::DRAM::BURST_SIZE);
    //TODO Hard-coded to 32-bit DRAM channels
    PRINT_STAT_INT("DRAM_TOTAL_BYTES", (total_read+total_write)*4); //4 bytes per data beat for 32-bit channels
    PRINT_STAT_INT("DRAM_TOTAL_ACTIVATES", total_activate);
    if(rigel::CMDLINE_VERBOSE_DRAM_STATS)
    {
      for (unsigned int i = 0; i < rigel::DRAM::CONTROLLERS; i++) {
        for (unsigned int j = 0; j < rigel::DRAM::RANKS; j++) {
          for (unsigned int k = 0; k < rigel::DRAM::BANKS; k++) {
            char stat_name[128];
            snprintf(stat_name, 128, "DRAM_READ_BANK_%02u_%02u_%02u", i, j, k);
            PRINT_STAT_INT(stat_name, mem_stats.read_bank[i][j][k]/rigel::DRAM::BURST_SIZE);
            snprintf(stat_name, 128, "DRAM_WRITE_BANK_%02u_%02u_%02u", i, j, k);
            PRINT_STAT_INT(stat_name, mem_stats.write_bank[i][j][k]/rigel::DRAM::BURST_SIZE);
            snprintf(stat_name, 128, "DRAM_ACTIVATE_BANK_%02u_%02u_%02u", i, j, k);
            PRINT_STAT_INT(stat_name, mem_stats.activate_bank[i][j][k]);
          }
        }
      }
    }
    PRINT_STAT_INT("MAX_DRAM_WINDOW_DELAY", DRAMChannelModel::GetMaxDelay());
  }
  PRINT_HEADER("CACHE INFO");
  {
    using namespace rigel::cache;
    using namespace rigel::profiler;
    using namespace rigel;

    // Profiler information for number of sharers for each cache line while in
    // the cache.
    for (int i = 0; i <= THREADS_PER_CLUSTER; i++) {
      char stat_name[128];
      snprintf(stat_name, 128, "L2D_LINE_READ_SHARERS_%02d", i);
      PRINT_STAT_INT(stat_name, ccache_access_histogram[i]);
    }
    for (int i = 0; i <= THREADS_PER_CLUSTER; i++) {
      char stat_name[128];
      snprintf(stat_name, 128, "L2I_LINE_READ_SHARERS_%02d", i);
      PRINT_STAT_INT(stat_name, icache_access_histogram[i]);
    }
  }
  PRINT_HEADER("SIMULATION TIMERS");
  {
    for(int tt = 0; tt < rigel::NUM_TIMERS; tt++) {
      uint64_t timer_data = 0;
      uint64_t instr_count = 0;

      if( SystemTimers[tt].valid )  {
        if( SystemTimers[tt].enabled ) { // someone left the timer running...
          uint64_t delta = rigel::CURR_CYCLE - SystemTimers[tt].start_time;
          uint64_t delta_instr = Profile::global_timing_stats.ret_num_instrs -
                  SystemTimers[tt].start_retired_instr_count;
          timer_data = SystemTimers[tt].accumulated_time + delta;
          instr_count = SystemTimers[tt].accumulated_retired_instr + delta_instr;
        } else { // the timer was not left running
          timer_data = SystemTimers[tt].accumulated_time;
          instr_count = SystemTimers[tt].accumulated_retired_instr;
        }
        char stat_name[128], stat_name2[128];
        snprintf(stat_name, 128, "TIMER_%04d", tt);
        snprintf(stat_name2, 128, "INSTRCNT_%04d", tt);
        PRINT_STAT_INT(stat_name, timer_data);
        PRINT_STAT_INT(stat_name2, instr_count);
      }
    }
  }
  PRINT_HEADER("RTM STATISTICS");
  {
    using namespace rigel::event;
    // Print out the statistics for the EventTracking System.
    global_tracker.PrintResultsRTM(stderr);
  }
  PRINT_HEADER("TASK STATS (DEPRECATE?)");
  {
    using namespace rigel::task_queue;

    PRINT_STAT_INT("TQ_ENQUEUE_COUNT", Profile::global_task_stats.tq_enqueue_count);
    PRINT_STAT_INT("TQ_DEQUEUE_COUNT", Profile::global_task_stats.tq_dequeue_count);
    PRINT_STAT_INT("TQ_LOOP_COUNT", Profile::global_task_stats.tq_loop_count);
    PRINT_STAT_INT("TQ_INIT_COUNT", Profile::global_task_stats.tq_init_count);
    PRINT_STAT_INT("TQ_END_COUNT", Profile::global_task_stats.tq_end_count);
    PRINT_STAT_INT("TQ_CYCLES_BLOCKED", Profile::global_task_stats.cycles_blocked);
    PRINT_STAT_INT("TOTAL_LOAD_STALL_CYCLES",  cs->load_stall_total);
    PRINT_STAT_STRING("TQ_TYPE", GlobalTaskQueue->GetQueueType().c_str());
    PRINT_STAT_INT("TQ_LATENCY_ENQUEUE_ONE", LATENCY_ENQUEUE_ONE);
    PRINT_STAT_INT("TQ_LATENCY_ENQUEUE_LOOP", LATENCY_ENQUEUE_LOOP);
    PRINT_STAT_INT("TQ_LATENCY_DEQUEUE", LATENCY_DEQUEUE);
    PRINT_STAT_INT("TQ_LATENCY_BLOCKED_SYNC", LATENCY_BLOCKED_SYNC);
    PRINT_STAT_INT("TQ_MAX_SIZE", QUEUE_MAX_SIZE);
    PRINT_STAT_BOOL("TQ_SCHED_WITH_CACHE_AFFINITY", 
      Profile::global_task_stats.sched_with_cache_affinity);
    PRINT_STAT_BOOL("TQ_SCHED_WITHOUT_CACHE_AFFINITY",  
      Profile::global_task_stats.sched_without_cache_affinity);
  }

  std::cerr << "TODO TODO -- THIS STUFF (BELOW) NEEDS ATTENTION --\n" << "\n";

  PRINT_HEADER("CACHE STATS");
  {
    PRINT_STAT_INT("CLUSTER_CACHE_MEMORY_BARRIERS", cs->L2_cache_memory_barriers);
    PRINT_STAT_INT("GLOBAL_CACHE_REQUEST_BUFFER_FULL", global_cache_stats.GCache_pending_buffer_full_cycles);
    PRINT_STAT_INT("GLOBAL_CACHE_REQUEST_BUFFER_EMPTY", global_cache_stats.GCache_pending_buffer_empty_cycles);
    PRINT_STAT_INT("GLOBAL_CACHE_REQUEST_BUFFER_NOTFULL_NOTEMPTY", global_cache_stats.GCache_pending_buffer_notempty_notfull_cycles);
    PRINT_STAT_FLOAT("GLOBAL_CACHE_REQUEST_BUFFER_MEAN_LENGTH", global_cache_stats.GCache_pending_buffer_length_sum / (1.0f * rigel::CURR_CYCLE));
    float pr_crq_ne_len = global_cache_stats.GCache_pending_buffer_length_sum / 
        (1.0f * (global_cache_stats.GCache_pending_buffer_notempty_notfull_cycles 
	 + global_cache_stats.GCache_pending_buffer_full_cycles));
    PRINT_STAT_FLOAT("GLOBAL_CACHE_REQUEST_BUFFER_NONEMPTY_MEAN_LENGTH", pr_crq_ne_len );
  }

  PRINT_HEADER("STALL CYCLES");
  {
    // Assumes two-wide
    float total = 1.0f * (Profile::global_timing_stats.two_issue_cycles +
                        Profile::global_timing_stats.one_issue_cycles +
                        Profile::global_timing_stats.zero_issue_cycles);
    PRINT_STAT_INT("ISSUE_SLOTS_TOTAL", Profile::global_timing_stats.issue_slots_total);
    PRINT_STAT_INT("ISSUE_SLOTS_FILLED", Profile::global_timing_stats.issue_slots_filled);
    PRINT_STAT_INT("ISSUE_SLOTS_FILLED_TWO", Profile::global_timing_stats.two_issue_cycles);
    PRINT_STAT_INT("ISSUE_SLOTS_FILLED_ONE", Profile::global_timing_stats.one_issue_cycles);
    PRINT_STAT_INT("ISSUE_SLOTS_FILLED_ZERO", Profile::global_timing_stats.zero_issue_cycles);
    PRINT_STAT_FLOAT("ISSUE_SLOTS_FILLED_TWO_FRAC", Profile::global_timing_stats.two_issue_cycles / total);
    PRINT_STAT_FLOAT("ISSUE_SLOTS_FILLED_ONE_FRAC", Profile::global_timing_stats.one_issue_cycles / total);
    PRINT_STAT_FLOAT("ISSUE_SLOTS_FILLED_ZERO_FRAC", Profile::global_timing_stats.zero_issue_cycles / total);
  }

  // The normal "IPC" figure is based purely on retired instructions and cycles.
  // This value takes into account "active" cycles, i.e., it does not count
  // cycles spent halted and in sleep mode/fast forward. 
  //PRINT_STAT_FLOAT("IPC_ADJUSTED", (1.0f*Profile::global_timing_stats.issue_slots_filled) /
              //((1.0f*Profile::global_timing_stats.issue_slots_total)/rigel::ISSUE_WIDTH));
//  std::cerr << "L1D_STALL_CYCLES, " << cs->L1D_cache_stall_cycles << "\n";
//  std::cerr << "L1I_STALL_CYCLES, " << cs->L1I_cache_stall_cycles << "\n";
//  std::cerr << "L2D_STALL_CYCLES, " << cs->L2D_cache_stall_cycles << "\n";
//  std::cerr << "L2I_STALL_CYCLES, " << cs->L2I_cache_stall_cycles << "\n";
//  std::cerr << "L2_STALL_CYCLES, "  << cs->L2_cache_stall_cycles << "\n";

  PRINT_HEADER("NETWORK");
  {
    PRINT_STAT_INT("NETWORK_INJECT_REQ", Profile::global_network_stats.msg_injects_reqs);
    PRINT_STAT_INT("NETWORK_INJECTS", Profile::global_network_stats.msg_injects);
    PRINT_STAT_INT("NETWORK_REMOVES", Profile::global_network_stats.msg_removes);
    PRINT_STAT_INT("NETWORK_OCCUPANCY", Profile::global_network_stats.network_occupancy);
    PRINT_STAT_INT("GCACHE_MEM_WAIT", Profile::global_network_stats.gcache_wait_time);
    PRINT_STAT_FLOAT("NETWORK_OCCUPANCY_MEAN", 
      Profile::global_network_stats.network_occupancy / 
        (1.0 * Profile::global_network_stats.msg_removes));
    PRINT_STAT_FLOAT("NETWORK_OCCUPANCY_READ_MEAN", 
      Profile::global_network_stats.network_occupancy_read / 
        (1.0 * Profile::global_network_stats.msg_removes_read));
    PRINT_STAT_FLOAT("GCACHE_MEM_WAIT_MEAN", 
      Profile::global_network_stats.gcache_wait_time / 
        (1.0 * Profile::global_network_stats.msg_removes));
    PRINT_STAT_FLOAT("GCACHE_MEM_WAIT_READ_MEAN", 
      Profile::global_network_stats.gcache_wait_time_read / 
        (1.0 * Profile::global_network_stats.msg_removes_read));
  }


  if(rigel::CMDLINE_DUMP_SHARING_STATS) {
    ProfileStat::dumpWrtTrackStats();
  }
  
  PRINT_HEADER("SIMULATION STATISTICS");
  {
    using namespace rigel;
    using namespace rigel::profiler;

    PRINT_STAT_INT("SIM_CYCLES", CURR_CYCLE);
    PRINT_STAT_INT("ACTIVE_CYCLES", ProfileStat::get_active_cycles());
    PRINT_STAT_FLOAT("ACTIVE_NANOSECONDS",(((double)ProfileStat::get_active_cycles())/rigel::CLK_FREQ));
    PRINT_STAT_INT("DRAM_CYCLES", DRAMChannelModel::DRAM_CURR_CYCLE);
    PRINT_STAT_INT("ACTIVE_DRAM_CYCLES", (uint64_t)((float)DRAMChannelModel::DRAM_CURR_CYCLE*(float)ProfileStat::get_active_cycles()/(float)CURR_CYCLE));
    PRINT_STAT_FLOAT("DRAM_CYCLES/SIM_CYCLES", ((float)DRAMChannelModel::DRAM_CURR_CYCLE/(float)CURR_CYCLE));
    PRINT_STAT_FLOAT("CYCLES_PER_SECOND_TOTAL", (1.0 * CURR_CYCLE)/(ts->tot_usec
/ 1000000.0));
    PRINT_STAT_FLOAT("ACTIVE_CYCLES_PER_SECOND", (ProfileStat::get_active_cycles())/(ts->tot_usec
/ 1000000.0));
    PRINT_STAT_STRING("SIMULATION_TIME_STRING", stime.c_str());
    PRINT_STAT_INT("SIMULATION_TIME_USEC", ts->tot_usec);
    uint64_t tot_instrs = 0;
    for (int tid = 0; tid < rigel::THREADS_TOTAL; tid++) {
       tot_instrs += ProfileStat::get_retired_instrs(tid);
    }
    PRINT_STAT_FLOAT("KIPS", (float)tot_instrs / ((float)ts->tot_usec / 1000.0));
    PRINT_STAT_STRING("DUMPFILE_PREFIX", DUMPFILE_PREFIX.c_str());
    PRINT_STAT_STRING("DUMPFILE_PATH", DUMPFILE_PATH.c_str());
  }
  
  // Get usage statistics for each cell in the global cache.
  {
    using namespace rigel::profiler;

    // Open the dump file for the histogram data
    char file_name[1024];

    snprintf(file_name, 1024, "%s/%st%03d-cl%03d-gcache-usage.csv",
      rigel::DUMPFILE_PATH.c_str(), 
      rigel::profiler::DUMPFILE_PREFIX.c_str(),
      rigel::NUM_TILES,
      rigel::CLUSTERS_PER_TILE);
    FILE *f_gcache_usage = fopen(file_name, "w");
    // Dump the matrix of access counts for each way in the cache.
    fprintf(f_gcache_usage, "# Contains the count of how many times a cell in "
        "the global cache had an insert performed to it.\n"
        "# The fields are (insert count, touch count)\n");
    for(int bank = 0; bank < rigel::NUM_GCACHE_BANKS; bank++) {
      fprintf(f_gcache_usage, 
        "--------------------- BANK %3d --------------------\n", bank);
      for (int set = 0; set < GCACHE_SETS; set++) {
        fprintf(f_gcache_usage, "SET [%3d] ", set);
        for (int way = 0; way < GCACHE_WAYS; way++) {
          if (GLOBAL_CACHE_PTR) {
            fprintf(f_gcache_usage, " (%6d, %6d), ", 
              GLOBAL_CACHE_PTR[bank]->get_insert_count(set, way),
              GLOBAL_CACHE_PTR[bank]->get_touch_count(set, way));
          } else {
            fprintf(f_gcache_usage, "disabled, ");
          }
        }
        fprintf(f_gcache_usage, "\n");
      }
    }
    fclose(f_gcache_usage);
  }

  if (rigel::profiler::PROFILE_HIST_GCACHEOPS) {
    ProfileStat::dumpHistStats();
  }


  // Print out latency and hit miss counts for each PC.
  if (rigel::profiler::CMDLINE_DUMP_PER_PC_STATS) {
    FILE * filep;
		std::string dump_filename;

    dump_filename =   rigel::DUMPFILE_PATH + std::string("/")
                    + rigel::profiler::DUMPFILE_PREFIX 
                    + std::string(rigel::profiler::per_pc_filename);

    filep = fopen(dump_filename.c_str(), "w");

    std::map<uint32_t, profile_instr_stall_t>::const_iterator pciter;

    // Print header
    fprintf(filep, "# PC, COUNT, L1D hits, L2D hits, L3D hits, L1I hits, L2I hits, "
                   "total time, instr latency, percent of runtime, \n");
    for (pciter = per_pc_stats.begin(); pciter != per_pc_stats.end(); pciter++) {
        const uint32_t pc = pciter->first; 
        const profile_instr_stall_t &is = pciter->second;
      fprintf(filep, "0x%08x, %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%12"PRIu64", %5.4f, %5.4f\n", 
        // PC of instruction.
        pc, 
        // Number of times the PC was seen during execution.
        is.get_count(), 
        // Number that were L1D hits.
        is.get_l1d_hits(),
        // Number that were L2D hits.
        is.get_l2d_hits(),
        // Global cache hits.
        is.get_l3d_hits(),
        // Number that were L1I hits.
        is.get_l1i_hits(),
        // Number that were L2I hits.
        is.get_l2i_hits(),
        // Total number of cycles spend executing the instruction.
        is.get_total_cycles(), 
        // The average latency of the instruction.
        is.get_latency(),
        // The percentage of total execution.
        is.get_percent()
        );
    }
    fclose(filep);

    //Now dump per-pc stall cycle info
		std::string stall_dump_filename;

    stall_dump_filename = rigel::DUMPFILE_PATH + std::string("/")
                        + rigel::profiler::DUMPFILE_PREFIX 
                        + std::string(rigel::profiler::per_pc_filename)
                        + std::string(".stalls");

    filep = fopen(stall_dump_filename.c_str(), "w");

    // Print header
    fprintf(filep, "PC, Total Count, Fetch Ct., Fetch, Decode Ct., Decode, Execute Ct., Execute, Mem Ct., Mem, FP Ct., FP, CC Ct., CC, WB Ct., WB, "
                   "IStall Ct., Instr. Stall, Sync Ct., Sync Waiting For Ack, ALU Ct., ALU Conflict, "
                   "FPU Ct., FPU Conflict, Shift Ct., ALU Shift Conflict, Mem Conflict Ct., Mem Conflict, Branch Conflict Ct., Branch Conflict, "
                   "Both Conflict Ct., Both Conflict, StuckInstr Ct., Stuck Behind Other Instr., StuckGlobal Ct., Stuck Behind Global, "
                   "L1D Pend Ct., L1D Pending, L1D MSHR Ct., L1D MSHR, L1I Pend Ct., L1I Pending, L1I MSHR Ct., L1I MSHR, L2D Pend Ct., "
                   "L2D Pending, L2D MSHR Ct., L2D MSHR, L2D Arb Ct., L2D Arbitrate, L2D Acc Ct., L2D Access, L2D Int Ct., L2D Interlock, "
                   "L2I Pend Ct., L2I Pending, L2I MSHR Ct., L2I MSHR, L2I Arb Ct., L2I Arbitrate, L2I Acc Ct., L2I Access, L1R Req Ct., "
                   "L1 Router Req., L1R Rep Ct., L1 Router Rep., L2R Req Ct., L2 Router Req., L2R Rep., L2 Router Rep., "
                   "L2 GC Req Ct., L2 GC Req., L2 GC Rep Ct., L2 GC Rep., Tile ReqBuf Ct., Tile ReqBuf., Tile RepBuf Ct., "
                   "Tile RepBuf., GNet Tile Req Ct., GNet Tile Req., GNet GC Req Ct., GNet GC Req., GNet ReqBuf Ct., GNet ReqBuf., "
                   "GNet RepBuf Ct., GNet RepBuf., GNet GC Rep Ct., GNet GC Rep., GNet Tile Rep Ct., GNet Tile Rep., BCAST Notify Ct., "
                   "Sending BCAST Notifies, GC Fill Ct., GC Fill, GC MSHR Ct., GC MSHR, GC Write Alloc Ct., GC Write Alloc, GC Pend Ct., GC Pending\n");

    for (pciter = per_pc_stats.begin(); pciter != per_pc_stats.end(); ++pciter) {
      const uint32_t pc = pciter->first; 
      const profile_instr_stall_t &is = pciter->second;
      fprintf(filep, "0x%08x, %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", "
                     "%10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64", %10"PRIu64"\n", 
        pc,
        is.get_count(),
        is.counts.fetch,
        is.cycles.fetch,
        is.counts.decode,
        is.cycles.decode,
        is.counts.execute,
        is.cycles.execute,
        is.counts.mem,
        is.cycles.mem,
        is.counts.fp,
        is.cycles.fp,
        is.counts.cc,
        is.cycles.cc,
        is.counts.wb,
        is.cycles.wb,
        is.counts.instr_stall,
        is.cycles.instr_stall,
        is.counts.sync_waiting_for_ack,
        is.cycles.sync_waiting_for_ack,
        is.counts.resource_conflict_alu,
        is.cycles.resource_conflict_alu,
        is.counts.resource_conflict_fpu,
        is.cycles.resource_conflict_fpu,
        is.counts.resource_conflict_alu_shift,
        is.cycles.resource_conflict_alu_shift,
        is.counts.resource_conflict_mem,
        is.cycles.resource_conflict_mem,
        is.counts.resource_conflict_branch,
        is.cycles.resource_conflict_branch,
        is.counts.resource_conflict_both,
        is.cycles.resource_conflict_both,
        is.counts.stuck_behind_other_instr,
        is.cycles.stuck_behind_other_instr,
        is.counts.stuck_behind_global,
        is.cycles.stuck_behind_global,
        is.counts.l1d_pending,
        is.cycles.l1d_pending,
        is.counts.l1d_mshr,
        is.cycles.l1d_mshr,
        is.counts.l1i_pending,
        is.cycles.l1i_pending,
        is.counts.l1i_mshr,
        is.cycles.l1i_mshr,
        is.counts.l2d_pending,
        is.cycles.l2d_pending,
        is.counts.l2d_mshr,
        is.cycles.l2d_mshr,
        is.counts.l2d_arbitrate,
        is.cycles.l2d_arbitrate,
        is.counts.l2d_access,
        is.cycles.l2d_access,
        is.counts.l2d_access_bit_interlock,
	      is.cycles.l2d_access_bit_interlock,
        is.counts.l2i_pending,
        is.cycles.l2i_pending,
        is.counts.l2i_mshr,
        is.cycles.l2i_mshr,
        is.counts.l2i_arbitrate,
        is.cycles.l2i_arbitrate,
        is.counts.l2i_access,
        is.cycles.l2i_access,
        is.counts.l1router_request,
        is.cycles.l1router_request,
        is.counts.l1router_reply,
        is.cycles.l1router_reply,
        is.counts.l2router_request,
        is.cycles.l2router_request,
        is.counts.l2router_reply,
        is.cycles.l2router_reply,
        is.counts.l2gc_request,
        is.cycles.l2gc_request,
        is.counts.l2gc_reply,
        is.cycles.l2gc_reply,
        is.counts.tile_requestbuffer,
        is.cycles.tile_requestbuffer,
        is.counts.tile_replybuffer,
        is.cycles.tile_replybuffer,
        is.counts.gnet_tilerequest,
        is.cycles.gnet_tilerequest,
        is.counts.gnet_gcrequest,
        is.cycles.gnet_gcrequest,
        is.counts.gnet_requestbuffer,
        is.cycles.gnet_requestbuffer,
        is.counts.gnet_replybuffer,
        is.cycles.gnet_replybuffer,
        is.counts.gnet_gcreply,
        is.cycles.gnet_gcreply,
        is.counts.gnet_tilereply,
        is.cycles.gnet_tilereply,
        is.counts.sending_bcast_notifies,
        is.cycles.sending_bcast_notifies,
        is.counts.gc_fill,
        is.cycles.gc_fill,
        is.counts.gc_mshr,
        is.cycles.gc_mshr,
        is.counts.gc_write_allocate,
        is.cycles.gc_write_allocate,
        is.counts.gc_pending,
        is.cycles.gc_pending
      );
    }
    fclose(filep);

  }

  // TODO: Add me as a command line option!
  couchdb_dump_profile();

}

//This function takes all the local stats and accumulates them into the global stats 
//This function should be called once by each cluster profiler at the end of simulation
void
Profile::accumulate_stats(){

  // Sanity check to make sure we are actually using our cache
  // effectively
  //add in the  timing_stats
  global_timing_stats.tot_num_fetch_instrs += timing_stats.tot_num_fetch_instrs;
  // FIXME These should not have been summations
  global_timing_stats.tot_usec = timing_stats.tot_usec;  
  global_timing_stats.clk_ticks_sim = timing_stats.clk_ticks_sim;  
  global_timing_stats.clk_ticks_host = timing_stats.clk_ticks_host;  
  global_timing_stats.ipc = timing_stats.ipc;  
  global_timing_stats.sim_slowdown = timing_stats.sim_slowdown;  

  global_timing_stats.branches_mispredicted += timing_stats.branches_mispredicted;  
  global_timing_stats.branches_correct += timing_stats.branches_correct;  
  global_timing_stats.branches_total += timing_stats.branches_total;  
  global_timing_stats.branch_target_register_hit += timing_stats.branch_target_register_hit;  
  global_timing_stats.issue_slots_total  += timing_stats.issue_slots_total;  
  global_timing_stats.issue_slots_filled += timing_stats.issue_slots_filled;  
  global_timing_stats.zero_issue_cycles += timing_stats.zero_issue_cycles;  
  global_timing_stats.one_issue_cycles += timing_stats.one_issue_cycles;  
  global_timing_stats.two_issue_cycles += timing_stats.two_issue_cycles;  

  global_timing_stats.if_stall += timing_stats.if_stall;  
  global_timing_stats.dc_stall += timing_stats.dc_stall;  
  global_timing_stats.ex_stall += timing_stats.ex_stall;  
  global_timing_stats.mc_stall += timing_stats.mc_stall;  
  global_timing_stats.tq_stall += timing_stats.tq_stall;  
  global_timing_stats.fp_stall += timing_stats.fp_stall;  
  global_timing_stats.cc_stall += timing_stats.cc_stall;  
  global_timing_stats.wb_stall += timing_stats.wb_stall;  

  global_timing_stats.if_block += timing_stats.if_block;  
  global_timing_stats.dc_block += timing_stats.dc_block;  
  global_timing_stats.ex_block += timing_stats.ex_block;  
  global_timing_stats.mc_block += timing_stats.mc_block;  
  global_timing_stats.fp_block += timing_stats.fp_block;  
  global_timing_stats.cc_block += timing_stats.cc_block;  
  global_timing_stats.wb_block += timing_stats.wb_block;  

  global_timing_stats.if_bubble += timing_stats.if_bubble;  
  global_timing_stats.dc_bubble += timing_stats.dc_bubble;  
  global_timing_stats.ex_bubble += timing_stats.ex_bubble;  
  global_timing_stats.mc_bubble += timing_stats.mc_bubble;  
  global_timing_stats.fp_bubble += timing_stats.fp_bubble;  
  global_timing_stats.cc_bubble += timing_stats.cc_bubble;  
  global_timing_stats.wb_bubble += timing_stats.wb_bubble;  

  global_timing_stats.if_active += timing_stats.if_active;  
  global_timing_stats.dc_active += timing_stats.dc_active;  
  global_timing_stats.ex_active += timing_stats.ex_active;  
  global_timing_stats.mc_active += timing_stats.mc_active;  
  global_timing_stats.fp_active += timing_stats.fp_active;  
  global_timing_stats.cc_active += timing_stats.cc_active;  
  global_timing_stats.wb_active += timing_stats.wb_active;  


  global_timing_stats.halt_stall += timing_stats.halt_stall;  
  global_timing_stats.ex_badpath += timing_stats.ex_badpath;  

  global_timing_stats.st_queued += timing_stats.st_queued;  
  global_timing_stats.ld_queued += timing_stats.ld_queued;  
  
  //add in the cache_stats
  accum_cache_stats.L1I_cache_hits += cache_stats.L1I_cache_hits;
  accum_cache_stats.L1I_cache_misses += cache_stats.L1I_cache_misses;
  accum_cache_stats.L1D_cache_read_hits += cache_stats.L1D_cache_read_hits;
  accum_cache_stats.L1D_cache_read_misses += cache_stats.L1D_cache_read_misses;
  accum_cache_stats.L1D_cache_write_hits += cache_stats.L1D_cache_write_hits;
  accum_cache_stats.L1D_cache_write_misses += cache_stats.L1D_cache_write_misses;

  accum_cache_stats.L2_cache_hits += cache_stats.L2_cache_hits;
  accum_cache_stats.L2_cache_misses += cache_stats.L2_cache_misses;
  accum_cache_stats.L2_stack_misses += cache_stats.L2_stack_misses;
  accum_cache_stats.L2_cache_memory_barriers += cache_stats.L2_cache_memory_barriers;
  accum_cache_stats.CCache_mshr_full += cache_stats.CCache_mshr_full;
  accum_cache_stats.CCache_memory_barrier_stalls += cache_stats.CCache_memory_barrier_stalls;

  accum_cache_stats.L2I_cache_misses += cache_stats.L2I_cache_misses;
  accum_cache_stats.L2I_cache_hits += cache_stats.L2I_cache_hits;

}


////////////////////////////////////////////////////////////////////////////////
// Memory Sharing Tracker
////////////////////////////////////////////////////////////////////////////////
//
// This collection of code tracks data sharing across task (or other) boundaries
// based on "EVENTs" as tracked in EventTracker.
//
// Memory access tracking methods
// barrierWrtTrack()
//
// get the current simulation barrier count from the event tracker
// keep track of current barrier and detect new barrier.
//
// Update the write tracker data structure on barrier change by walking
// the table to clear all "writtenThisInterval" flags, updtae the rd share aggregate
// count, and clear the rd sharers list
//
void 
ProfileStat::barrierWrtTrack(bool final) {
  int interval_num = rigel::event::global_tracker.GetRTMBarrierCount();

  uint32_t maxRdShares = 0;
  // final indicates called from dump, need to update with the final (or only) interval
  if (((unsigned int)interval_num != currentBarrierNum) || final) {
    // clear written bits, update aggregating counters
    WrtShareTrack* WT;
		uint32_t addr;
    std::map<uint32_t, WrtShareTrack*>::iterator AI, AE;
    for (AI = WrtShareTable.begin(), AE=WrtShareTable.end(); AI != AE; ++AI) {
      WT = AI->second;
      addr = AI->first;
      if (WT->currShareState==RACED) totRaces++;
      uint32_t shLen = WT->sharedWrtList.size();
      sharedRdrCnt += shLen;
      if (shLen > maxRdShares) maxRdShares = shLen;
      if (final){
        if (shLen>0) WT->sharedWrtCounts.push_back(shLen); //normally updated on new wrt, also at final
	totOutputWrtCnt += WT->outputWrt;
	updateWrtShareHistrogram(WT);
	if (WT->lastIntervalWritten==-1) {
	  totRdOnly++; // never written from the kernel program
	  updateInputRdHistrogram(WT);
	}
      } else {
        // reset various tracker fields for continued tracking
        WT->writtenThisInterval=false;
        WT->readThisInterval=false;
        WT->lastWrtTask=-1;
        WT->lastRdTask=-1;
        //WT->sharedWrtList.clear(); // keep live across barriers if still running
        WT->currShareState=CLEAN;
        // don't clear outputWrt, one outputwrt can be read in multiple intervals
      }

    }

    #ifdef DEBUG_MEMTRACKER
    fprintf(stdout,"MEM SHARE TRACK: interval %d max sharers: %d \n", interval_num, maxRdShares);
    fprintf(stdout,"barrierWrtTrack: %d \n", interval_num);
    #endif

  //WrtShareTable.clear();   // wipe entire map struct, start new each interval
  }
  currentBarrierNum = interval_num;
}

/// updateInputRdHistrogram()
/// Creates a histrogram using a STL map with the number of sharers as the key
/// and the number of addresses with that many sharers as the data.
/// 
/// The WrtShareTable tracks number of sharers (tasks) for each address.
/// This histogram turns it around to list the number of addresses with
/// that many sharers.
/// 
/// Called with the WT struct of each address, pulls the number of sharers
/// from the size of that addresses share list, and increments the count in
/// the bucket of that share count
void
ProfileStat::updateInputRdHistrogram(WrtShareTrack* WT) {
  uint32_t bucket = WT->inputRdList.size();
  if (bucket > 0) {
    std::pair<uint32_t, uint32_t> init_bucket(bucket,0);
    // this version of map.insert() creates the bucket with init_bucket value if it is not present
    // or if present just returns iterator I pointing to it, unmodified
    std::map<uint32_t,uint32_t>::iterator I = InputRdHistrogram.insert(InputRdHistrogram.begin(), init_bucket);
    // so now we can just increment the value in the bucket
    I->second++;
  }
}


/// updateWrtShareHistrogram()
void
ProfileStat::updateWrtShareHistrogram(WrtShareTrack* WT) {
  uint32_t bucket = WT->sharedWrtCounts.size();
  if (bucket > 0) {
    std::pair<uint32_t, uint32_t> init_bucket(bucket,0);
    // this version of map.insert() creates the bucket with init_bucket value if it is not present
    // or if present just returns iterator I pointing to it, unmodified
    std::map<uint32_t,uint32_t>::iterator I = WrtShareHistrogram.insert(WrtShareHistrogram.begin(), init_bucket);
    // so now we can just increment the value in the bucket
    I->second++;
  }
    /*
    fprintf(stdout,"OUTPUTWRT addr %08x\twrites %d\t",addr,sz);
    for (int i=0;i<sz;i++){
      assert(WT->sharedWrtCounts[i] > 0);
      fprintf(stdout,"%d\t",WT->sharedWrtCounts[i]);
    }
    fprintf(stdout,"\n");
    */
}

/// dumpInputRdHistogram()
void
ProfileStat::dumpInputRdHistogram() {

  std::map<uint32_t,uint32_t>::iterator HI,HE;
  for (HI=InputRdHistrogram.begin(), HE=InputRdHistrogram.end();HI!=HE;++HI) {
    fprintf(stdout,"%d INPUT RDONLY SHARERS: \t%d\n",HI->first,HI->second);
  }
}


/// dumpWrtShareHistrogram()
void
ProfileStat::dumpWrtShareHistrogram() {

  std::map<uint32_t,uint32_t>::iterator HI,HE;
  for (HI=WrtShareHistrogram.begin(), HE=WrtShareHistrogram.end();HI!=HE;++HI) {
    fprintf(stdout,"%d OUTPUT WRT SHARERS: \t%d\n",HI->first,HI->second);
  }
}

/// MemWrtTrack()
///
/// Called on memory writes, to update the table and structures as needed
///
/// Uses addr (aligned to cache line bdy) as key to a std::map structure
/// Check if key is present, build new struct and insert it if not
/// If present, update counts/flags as needed to track state of the address
void 
ProfileStat::MemWrtTrack(uint32_t addr, uint32_t write_pc, int task,
                     bool is_global , bool is_atomic ){ // default to false

  // count ALL writes (any source)
  totWrtCnt++;  

  //...and track globals and atomics seperately
  if(is_global) {
    tot_global_writes++;
    return;
    // NOTE: do we want to exit early for globals?
  }
  if(is_atomic) {
    tot_atomics++;
    return;
    // NOTE: do we want to exit early for atomics?
  }

  // count lock reads
  if( addr >= rigel::LOCKS_REGION_BEGIN &&
      addr <  rigel::LOCKS_REGION_END )
  { 
    tot_lock_writes++; 
    return; // return early, locks counted seperate
  }

  // filter bogus task nums 
  // (startup accesses, enqueue traffic not charged to a particular task)
  assert(task >= -1 && "out of expected range of >= -1");
  if (task == -1) {
    #ifdef DEBUG_MEMTRACKER
    fprintf(stderr, "BOGUS WRT TASK NUM -1\n");
    #endif
    bogusTaskNumWr++;
    return; //exit early, not tracking these accesses in detail
  }

  // treat stack accesses seperately
  if ((unsigned int)addr >= 0xC0000000) {
    totTaskStackWrCnt++;
    return; //exit early, not tracking these accesses in detail
  }

  // and track RTM accesses seperately
  // TODO: < or <= END?
  if ( (write_pc >= rigel::CODEPAGE_LIBPAR_BEGIN) &&
       (write_pc < rigel::CODEPAGE_LIBPAR_END) )  
  {
    tot_RTM_WrCnt++;
    return; //exit early, not tracking these accesses in detail
  }


  // now, handle other writes not classified as "special"

  WrtShareTrack* WT;
  //uint32_t addr_line = addr & 0xffffffe0;
  uint32_t addr_line = addr ; // don't call false sharing a conflict

  // check for barrier crossing, handle
  barrierWrtTrack();

  // see if this address is in the table already
  std::map<uint32_t, WrtShareTrack*>::iterator I = WrtShareTable.find(addr_line);

  //  if not, create a new entry and load it up - only occurs on first write to an addr
  if (I == WrtShareTable.end()) {
    #ifdef DEBUG_MEMTRACKER
    fprintf(stdout,"Alloc First Wrt task=%d, addr=%x, int=%d\n",task, addr, currentBarrierNum);
    #endif
    WT = new WrtShareTrack;
    WT->lastIntervalWritten = currentBarrierNum;
    WT->lastWrtTask = task;
    WT->writtenThisInterval = true;
    WT->readThisInterval = false;
    WT->lastRdTask = -1;
    WT->outputWrt = 0;
    WT->currShareState=DIRTY;
    WrtShareTable[addr_line] = WT;

    totAllocatingWrtCnt++;

  // entry for this address exists, so update it
  } else { 

    WT = I->second;

    if ( WT->writtenThisInterval) {
      assert((WT->lastIntervalWritten==(int)currentBarrierNum) && "WRT TRACK ERR: Interval number doesn't match");
      assert( (WT->currShareState==DIRTY || WT->currShareState==RACED) && "WRT TRACK ERR: Written addr incorrect state");

      // conflict wrt is different task/task within interval
      if (WT->lastWrtTask != task) {
        #ifdef DEBUG_MEMTRACKER
        fprintf(stdout,"CONFLICT Wrt-Wrt task=%d addr=0x%08x intrvl=%d\n",task, addr, currentBarrierNum);
        #endif
        totConflictWrtCnt++;  
        WT->lastWrtTask=task;
        WT->currShareState=RACED;

        // log PCs of conflicts
        WT->writeConflictPCs.insert(write_pc); // writePCs is a set so no dups
      }
      // fully private write
      // (WT->writtenThisInterval && WT->lastWrtTask == task) // only possible case left
      else {
        // TODO: something in the else case?
	// if (WT->currShareState==DIRTY) ??? take state from RACED to DIRTY
	// should we look for multiple tasks that write and last task repeats writes?
        totPrivateWrtCnt++;
      }

    }
    // ???  if this is a read-allocated entry, mark this write as the first
    //if(WT->lastIntervalWritten == -1)  // don't think this matters...

    // first write in this interval
    else {
      #ifdef DEBUG_MEMTRACKER
      fprintf(stdout,"MEM-TRACK - First Wrt task=%d, addr=%x, int=%d\n",task, addr, currentBarrierNum);
      #endif
      assert( (WT->currShareState!=DIRTY && WT->currShareState!=RACED) && "WRT TRACK ERR: Addr not written but in modified state");

      // update for first write in the interval
      WT->lastWrtTask = task;
      WT->writtenThisInterval=true;
      WT->lastIntervalWritten = currentBarrierNum; // record interval of last write
      
      // these need to stay live until an actual write so only counted once
      if (WT->outputWrt){
        //totOutputWrtCnt += WT->outputWrt; // aggregate output writes before clearing
        totOutputWrtCnt++; // aggregate output writes before clearing
	assert(WT->sharedWrtList.size()>0 && "OUTPUT WRT but share size 0");
	WT->sharedWrtCounts.push_back(WT->sharedWrtList.size());
        #ifdef DEBUG_MEMTRACKER
        fprintf(stdout,"write to output write detected pc: 0x%08x addr: 0x%08x task: %d\n", write_pc, addr, task);
        fprintf(stdout,"write to output write num_writes %d curr sharers %d\n", (uint32_t)WT->sharedWrtCounts.size(), (uint32_t)WT->sharedWrtList.size());
	#endif
	WT->sharedWrtList.clear();
      }
      // a new wrt kills previous value so clear outputWrt
      WT->outputWrt = 0;

      // if read, should be either PRIVATE or SHARED. If its PRIVATE and writer is the
      // same as reader, it stays private(dirty), else it's a conflict/race
      if (WT->readThisInterval){
	assert(WT->currShareState==PRIVATE || WT->currShareState==SHARED);
        if (WT->currShareState==PRIVATE && task==(WT->lastRdTask)) { 
	  WT->currShareState=DIRTY;
          totPrivateWrtCnt++;
        } else {
	  WT->currShareState=RACED;
          #ifdef DEBUG_MEMTRACKER
          fprintf(stdout,"CONFLICT Rd-Wrt task=%d pc=0x%08x addr=0x%08x intrvl=%d\n",task, write_pc, addr, currentBarrierNum);
          #endif
          totConflictWrtCnt++; // clean
          // log PCs of conflicts
          WT->writeConflictPCs.insert(write_pc); // writePCs is a set so no dups
	}
      } else { // neither read nor written yet this interval
        #ifdef DEBUG_MEMTRACKER
        fprintf(stdout,"MEMTRACK state %d\n", WT->currShareState);
        #endif
	assert(WT->currShareState==CLEAN);
	WT->currShareState=DIRTY;  // DIRTY is a private state
        totCleanWrtCnt++; // clean - does this give any information?
      }
    }
  }
}

///  MemRdTrack()
/// 
///  Called on memory reads to update access tracking structures
/// 
///  Checks if address has been written this interval.  If not, it is an Input Rd
///  Input Rd: these define an output wrt, but only once.  Flag and count updated
///  Shared rd list (std::vector) used to track unique sharers, length of this is the
///  count added to the aggregate count of shared rd events
void 
ProfileStat::MemRdTrack(uint32_t addr, uint32_t read_pc, int task,
                     bool is_global , bool is_atomic ) { // default to false

  // count ALL reads here (of any kind!)
  totRdCnt++;   

  //...and track globals and atomics seperately
  if(is_global) {
    tot_global_reads++;
    return;
    // NOTE: do we want to exit early for globals?
  }
  if(is_atomic) {
    tot_atomics++;
    return;
    // NOTE: do we want to exit early for atomics?
  }

  // count lock reads
  if( addr >= rigel::LOCKS_REGION_BEGIN &&
      addr <  rigel::LOCKS_REGION_END )
  { 
    tot_lock_reads++; 
    return; // count locks seperate, not keeping details
  }

  // filter reads with "bogus" task nums
  assert( task >= -1 && "out of expected range of >= -1");
  if (task ==-1 ) {
    #ifdef DEBUG_MEMTRACKER
    fprintf(stderr, "BOGUS RD TASK NUM -1\n");
    #endif
    bogusTaskNumRd++;
    return; // early exit
  }

  // also filter task stack access, count separately
  if ((unsigned int)addr >= 0xC0000000) {
    totTaskStackRdCnt++;
    return; // early exit
  }

  // and track RTM accesses seperately
  // TODO: < or <= END?
  if ( (read_pc >= rigel::CODEPAGE_LIBPAR_BEGIN) &&
       (read_pc <  rigel::CODEPAGE_LIBPAR_END) )  
  {
    tot_RTM_RdCnt++;
    return; // early exit
  }


  // now, for the remainder of accesses not classified as something "special"

  WrtShareTrack* WT;
  //uint32_t addr_line = addr & 0xffffffe0;
  uint32_t addr_line = addr ;

  // check for barrier crossing, handle
  barrierWrtTrack();

  // NOTE: not necessarily already written, this may be input dataset loaded and
  // never changed (or empty data set that is uninitialized due to data independent code)
  std::map<uint32_t, WrtShareTrack*>::iterator I = WrtShareTable.find(addr_line);

  // Allocate if .end returned, meaning address entry not yet tracked
  if (I == WrtShareTable.end()) {
    #ifdef DEBUG_MEMTRACKER
    fprintf(stdout, "WARN: Rd Track task %d addr 0x%08x not present on Read!!!\n", task, addr);
    #endif
    //  allocate new entry, mark it as in "Allocating" Input Rd
    WT = new WrtShareTrack;
    WT->lastIntervalWritten = -1; // invalid, this is a read so its not written yet
    WT->lastWrtTask = -1; // invalid, this is a read, no original writer if read allocated
    WT->writtenThisInterval = false;
    WT->readThisInterval = true;
    WT->lastRdTask = task;
    WT->outputWrt = 0; // allocated on read, never written yet
    WT->currShareState=PRIVATE;
    WT->inputRdList.insert(task);
    WrtShareTable[addr_line] =  WT;
    totAllocatingRdCnt++;  // realy don't care about this one
    totUnMatchedInputRdCnt++;

  } else { // address entry already exists for read address

    WT = I->second;

    // DIRTY: written in THIS interval, treat "written" addresses specially
    if ( WT->writtenThisInterval ) {
      // conflict read to location written in this interval by another task
      // (in sw, should have been protected by some locking/mutex)
      assert(WT->currShareState==DIRTY || WT->currShareState==RACED);
      if ( WT->lastWrtTask != task) { 
        #ifdef DEBUG_MEMTRACKER
        fprintf(stdout,"CONFLICT Rd task=%d pc=0x%08x addr=0x%08x intrvl=%d\n",task, read_pc, addr, currentBarrierNum);
	fprintf(stdout,"LAST WRT by task %d intrvl=%d\n",WT->lastWrtTask,WT->lastIntervalWritten);
        #endif
        totConflictRdCnt++; 
        WT->currShareState=RACED;

        // log PCs of conflicts
	WT->readConflictPCs.insert(read_pc);

      } else { // reading own write
        // nothing special, just a private read (counted as part of totRdCnt)
        totPrivateRdCnt++; // technically, this is redundant
      }

    // NOT written in the current interval
    } else { 
      // written is false entering an interval now, indicating output wrt (??!)
      // location not yet written (FIXME: how should we be handling this case??
      // read before write? most are input reads)
      assert( (WT->currShareState!=DIRTY && WT->currShareState!=RACED) && "WRT TRACK ERR: Addr not written but in modified state");
      
      // if written EVER (any previous interval EXCEPT current)
      if (WT->lastIntervalWritten != -1) { 
        #ifdef DEBUG_MEMTRACKER
        fprintf(stdout,"read to output write detected pc: 0x%08x addr: 0x%08x task: %d CYCLE: %d\n", read_pc, addr, task, rigel::CURR_CYCLE);
        #endif
        totMatchedInputRdCnt++;  // but count all input rd share events (matches existing address entry)
        WT->outputWrt=1; // output write if we are reading something written from earlier interval
        WT->sharedWrtList.insert(task);  // dedicate to sharers of programmed writes for now

      // NOT yet written EVER (UNMATCHED)
      } else { 
        // input read from an unwritten memory address
        assert(WT->lastIntervalWritten == -1 && "unhandled read with no write case!");
        totUnMatchedInputRdCnt++;
        WT->inputRdList.insert(task);
      }

      // add this "task" to sharing list if not present 
      // handle "sharers" tracking 
      // check if this task/task is in the list of sharers already, track unique sharers
      //ShareAttr sh;
      //sh.tasknum=task;
      //sh.corenum=RigelGetCoreNum();
      //sh.intervalnum=currentBarrierNum;
      //WT->sharedWrtList.insert(task);
      // TODO decide how to handle kernel input data read in multiple intervals
      // for now it is assumed to be clean at each barrier, that may not be optimal
      // since it could be kept valid if we know if can't change across intervals.
      // Probably want to add support for calls to Writeback/Invalidate to clean addresses
      //
      //  TODO make separate list for input data sharers
      if (!WT->readThisInterval || (WT->lastRdTask==task && WT->currShareState==PRIVATE)){  // test for multiple rd sharers
        #ifdef DEBUG_MEMTRACKER
        fprintf(stdout,"MEMTRACK state %d\n", WT->currShareState);
        #endif
        WT->currShareState=PRIVATE;
      } else {
        WT->currShareState=SHARED;
      }
    }
    WT->readThisInterval = true;
    WT->lastRdTask = task;
  }
}

/// dumpWrtTrackStats()
/// Prints out the memory tracker's stats at the end
void 
ProfileStat::dumpWrtTrackStats(  ) {

  // this call forces the last interval's stats into the aggregate totals
  // so the print is accurate for the whole run
  barrierWrtTrack(true);

  dumpInputRdHistogram();
  dumpWrtShareHistrogram();

  // NOTE: don't use math expressions in prints, loss of information can result
  // (print all stats directly, if derived stats printed, also print source data)
  fprintf(stdout, "##################################################\n");
  fprintf(stdout, "BEGIN WRT-Share-Tracking Stats:\n");
  fprintf(stdout, "TOTAL_INTERVALS TRACKED, %10llu\n", (long long unsigned)currentBarrierNum+1);
  fprintf(stdout, "##################################################\n");
  fprintf(stdout, "READ STATS: \n");
  fprintf(stdout, " INPUT_RD_ONLY,           %10llu\n", (long long unsigned)totRdOnly);
  fprintf(stdout, " INPUT_RD_MATCHED,        %10llu\n", (long long unsigned)totMatchedInputRdCnt);
  fprintf(stdout, " INPUT_RD_UNMATCHED,      %10llu\n", (long long unsigned)totUnMatchedInputRdCnt);
  fprintf(stdout, " ALLOCATING_RD,           %10llu\n", (long long unsigned)totAllocatingRdCnt);
  fprintf(stdout, " PRIVATE_RD,              %10llu\n", (long long unsigned)totPrivateRdCnt);
  fprintf(stdout, " CONFLICT_RD,             %10llu\n", (long long unsigned)totConflictRdCnt);
  fprintf(stdout, " TASK STACK RD,           %10llu\n", (long long unsigned)totTaskStackRdCnt);
  fprintf(stdout, " RTM RD,                  %10llu\n", (long long unsigned)tot_RTM_RdCnt);
  fprintf(stdout, " LOCK_READS,              %10llu\n", (long long unsigned)tot_lock_reads);
  fprintf(stdout, " BOGUS TASK RD CNT,       %10llu\n", (long long unsigned)bogusTaskNumRd);
  fprintf(stdout, "--------------------------------------------------\n");
  fprintf(stdout, " TOTAL_RD_CNT,            %10llu\n", (long long unsigned)totRdCnt);
  fprintf(stdout, "##################################################\n");
  fprintf(stdout, "WRITE STATS: \n");
  fprintf(stdout, " CLEAN_WR,                %10llu\n", (long long unsigned)totCleanWrtCnt);
  fprintf(stdout, " ALLOCATING_WR,           %10llu\n", (long long unsigned)totAllocatingWrtCnt);
  fprintf(stdout, " PRIVATE_WR,              %10llu\n", (long long unsigned)totPrivateWrtCnt);
  fprintf(stdout, " CONFLICT_WR,             %10llu\n", (long long unsigned)totConflictWrtCnt);
  fprintf(stdout, " OUTPUT_WR,               %10llu\n", (long long unsigned)totOutputWrtCnt);
  fprintf(stdout, " TASK STACK Wrt,          %10llu\n", (long long unsigned)totTaskStackWrCnt);
  fprintf(stdout, " RTM Wrt,                 %10llu\n", (long long unsigned)tot_RTM_WrCnt);
  fprintf(stdout, " LOCK_WRITES,             %10llu\n", (long long unsigned)tot_lock_writes);
  fprintf(stdout, " BOGUS TASK WR CNT,       %10llu\n", (long long unsigned)bogusTaskNumWr);
  fprintf(stdout, "--------------------------------------------------\n");
  fprintf(stdout, " TOTAL_WR_CNT,            %10llu\n", (long long unsigned)totWrtCnt);
  fprintf(stdout, "##################################################\n");
  fprintf(stdout, " INPUT_RD_SHARERS,        %10llu\n", (long long unsigned)sharedRdrCnt);
  fprintf(stdout, "##################################################\n");
  fprintf(stdout, " GLOBAL_READS,            %10llu\n", (long long unsigned)tot_global_reads);
  fprintf(stdout, " GLOBAL_WRITES,           %10llu\n", (long long unsigned)tot_global_writes);
  fprintf(stdout, " ATOMICS,                 %10llu\n", (long long unsigned)tot_atomics);
  fprintf(stdout, " RACES,                   %10llu\n", (long long unsigned)totRaces);
  fprintf(stdout, "##################################################\n");
  // print conflict PCs
  // std::vector<uint32_t>::iterator it;
  // fprintf(stdout, "READ CONFLICT PCs:\n");
  // for ( it=readPCs.begin() ; it < readPCs.end(); it++ ) {
  //   fprintf(stdout, "0x%08x\n", *it);
  // }
  // fprintf(stdout, "WRITE CONFLICT PCs:\n");
  // for ( it=writePCs.begin() ; it < writePCs.end(); it++ ) {
  //   fprintf(stdout, "0x%08x\n", *it);
  // }
  // fprintf(stdout, "##################################################\n");

    // TODO: something in the else case?

  // avg input rd sharers would be sharedRdrCnt / totOutputWrtCnt
  // private rd count would be totRdCnt - totInputRdCnt - totConflictRdCnt

  fprintf(stdout, "END WRT-Share-Tracking Stats:\n");
  fprintf(stdout, "##################################################\n");
}



/// ProfileStat::dumpHistStats
/// 
/// Prints out the global history stats for Gcache, Network and Memory
/// to their dedicated output files
/// 
/// Currently called from Profile::global_dump_profile
void
ProfileStat::dumpHistStats() 
{

    using namespace rigel::profiler;

    // Open the dump file for the histogram data
    char file_name[1024];
    
    snprintf(file_name, 1024, "%s/%s%s-t%03d-cl%03d-gcache.csv", 
      rigel::DUMPFILE_PATH.c_str(), 
      rigel::profiler::DUMPFILE_PREFIX.c_str(),
      gcacheops_histogram_dumpfile, 
      rigel::NUM_TILES, 
      rigel::CLUSTERS_PER_TILE);
    FILE *f_gcache_hist = fopen(file_name, "w");
    
    snprintf(file_name, 1024, "%s/%s%s-t%03d-cl%03d-net.csv",
      rigel::DUMPFILE_PATH.c_str(), 
      rigel::profiler::DUMPFILE_PREFIX.c_str(),
      gcacheops_histogram_dumpfile, 
      rigel::NUM_TILES,
      rigel::CLUSTERS_PER_TILE);
    FILE *f_net_hist = fopen(file_name, "w");

    snprintf(file_name, 1024, "%s/%s%s-t%03d-cl%03d-mem.csv",
      rigel::DUMPFILE_PATH.c_str(), 
      rigel::profiler::DUMPFILE_PREFIX.c_str(),
      gcacheops_histogram_dumpfile, 
      rigel::NUM_TILES,
      rigel::CLUSTERS_PER_TILE);
    FILE *f_mem_hist = fopen(file_name, "w");



    assert( NULL != f_gcache_hist && "GCache histogram file open failed");
    assert( NULL != f_net_hist && "Network histogram file open failed");
    assert( NULL != f_mem_hist && "Memory histogram file open failed");


    // XXX: DUMP GCACHE ACCESS COUNT BINS
    // Print the headers to the file.  There is no first element.
    for(int i = 0; i < (rigel::NUM_GCACHE_BANKS); i++)
    {
      for (int j = 0; j < PROF_HIST_COUNT; j++) {
        fprintf(f_gcache_hist, ", %s[%d] ", profile_gcacheops_names[j], i);
      }
    }

    fprintf(f_gcache_hist, "\n");

  // Print out each time bin.  Normalize to accesses-per-cycle
  GCacheOpProfType::iterator giter = gcache_hist_stats.begin();
  while (giter != gcache_hist_stats.end()) {
    fprintf(f_gcache_hist, "%lld", (unsigned long long)giter->first);
    for (int k = 0; k < (rigel::NUM_GCACHE_BANKS); k++)
    {
      for (int j = 0; j < PROF_HIST_COUNT; j++) {
        fprintf(f_gcache_hist, ", %10d",
        (giter->second)[k][(profile_gcacheops_histogram_t)j]);
      }
    }

    fprintf(f_gcache_hist, "\n");
    giter++;
  }

    // XXX: DUMP NETWORK OCCUPANCY BINS
    // Print the headers to the file.  There is no first element.
    for (int i = 1; i < 2*gcacheops_histogram_bin_size; i*=2) {
      fprintf(f_net_hist, ", %10d ", i);
    }
    fprintf(f_net_hist, "\n");

    // Print out each time bin.  Normalize to accesses-per-cycle
    NetLatProfType::iterator net_iter = net_hist_stats.begin();
    while (net_iter != net_hist_stats.end()) {
      fprintf(f_net_hist, "%lld", (unsigned long long)net_iter->first);

      for (int i = 1; i < 2*gcacheops_histogram_bin_size; i*=2) {
        fprintf(f_net_hist, ", %10d", (net_iter->second)[i]);
      }

      fprintf(f_net_hist, "\n");
      net_iter++;
    }

    // XXX: DUMP DRAM OCCUPANCY BINS
    // Print the headers to the file.  There is no first element.
    for (int i = 0; i < PROF_DRAM_COUNT; i++) {
      fprintf(f_mem_hist, ", %s ", profile_dram_names[i]);
    }
    fprintf(f_mem_hist, "\n");

    // Print out each time bin.  Normalize to accesses-per-cycle
    DRAMOpProfType::iterator mem_iter = dram_hist_stats.begin();
    while (mem_iter != dram_hist_stats.end()) {
      fprintf(f_mem_hist, "%lld", (unsigned long long)mem_iter->first);

      for (int j = 0; j < PROF_DRAM_COUNT; j++) {
        fprintf(f_mem_hist, ", %10d",
          (mem_iter->second)[(profile_dram_histogram_t)j]);
      }

      fprintf(f_mem_hist, "\n");
      mem_iter++;
    }
    std::cerr << "######### DUMPING HISTOGRAM ########" << "\n";
    std::cerr << "MEM_HIST_BIN_SIZE, " << gcacheops_histogram_bin_size << "\n";

    // Close the file, all done.
    fclose(f_gcache_hist);
    fclose(f_net_hist);
    fclose(f_mem_hist);
}


void Profile::accumulate_per_pc_stall_cycles()
{
  std::map< uint32_t, profile_instr_stall_t >::iterator pciter;
  for(pciter = per_pc_stats.begin(); pciter != per_pc_stats.end(); ++pciter)
    accumulate_stall_cycles(pciter->second.cycles);
}


void Profile::accumulate_stall_cycles(InstrCycleStats ics)
{
  profiler::stats[STATNAME_INSTR_INSTR_STALL_CYCLES].inc(ics.instr_stall);
  profiler::stats[STATNAME_INSTR_IF_OCCUPANCY].inc(ics.fetch);
  profiler::stats[STATNAME_INSTR_DE_OCCUPANCY].inc(ics.decode);
  profiler::stats[STATNAME_INSTR_EX_OCCUPANCY].inc(ics.execute);
  profiler::stats[STATNAME_INSTR_MC_OCCUPANCY].inc(ics.mem);
  profiler::stats[STATNAME_INSTR_FP_OCCUPANCY].inc(ics.fp);
  profiler::stats[STATNAME_INSTR_CC_OCCUPANCY].inc(ics.cc);
  profiler::stats[STATNAME_INSTR_WB_OCCUPANCY].inc(ics.wb);
  profiler::stats[STATNAME_INSTR_SYNC_WAITING_FOR_ACK].inc(ics.sync_waiting_for_ack);
  profiler::stats[STATNAME_INSTR_RESOURCE_CONFLICT_ALU].inc(ics.resource_conflict_alu);
  profiler::stats[STATNAME_INSTR_RESOURCE_CONFLICT_FPU].inc(ics.resource_conflict_fpu);
  profiler::stats[STATNAME_INSTR_RESOURCE_CONFLICT_ALU_SHIFT].inc(ics.resource_conflict_alu_shift);
  profiler::stats[STATNAME_INSTR_RESOURCE_CONFLICT_MEM].inc(ics.resource_conflict_mem);
  profiler::stats[STATNAME_INSTR_RESOURCE_CONFLICT_BRANCH].inc(ics.resource_conflict_branch);
  profiler::stats[STATNAME_INSTR_RESOURCE_CONFLICT_BOTH].inc(ics.resource_conflict_both);
  profiler::stats[STATNAME_INSTR_STUCK_BEHIND_OTHER_INSTR].inc(ics.stuck_behind_other_instr);
  profiler::stats[STATNAME_INSTR_STUCK_BEHIND_GLOBAL].inc(ics.stuck_behind_global);
  profiler::stats[STATNAME_INSTR_L1D_PENDING].inc(ics.l1d_pending);
  profiler::stats[STATNAME_INSTR_L1D_MSHR].inc(ics.l1d_mshr);
  profiler::stats[STATNAME_INSTR_L1I_PENDING].inc(ics.l1i_pending);
  profiler::stats[STATNAME_INSTR_L1I_MSHR].inc(ics.l1i_mshr);
  profiler::stats[STATNAME_INSTR_L2D_PENDING].inc(ics.l2d_pending);
  profiler::stats[STATNAME_INSTR_L2D_MSHR].inc(ics.l2d_mshr);
  profiler::stats[STATNAME_INSTR_L2D_ARBITRATE].inc(ics.l2d_arbitrate);
  profiler::stats[STATNAME_INSTR_L2D_ACCESS].inc(ics.l2d_access);
  profiler::stats[STATNAME_INSTR_L2I_PENDING].inc(ics.l2i_pending);
  profiler::stats[STATNAME_INSTR_L2I_MSHR].inc(ics.l2i_mshr);
  profiler::stats[STATNAME_INSTR_L2I_ARBITRATE].inc(ics.l2i_arbitrate);
  profiler::stats[STATNAME_INSTR_L2I_ACCESS].inc(ics.l2i_access);
  profiler::stats[STATNAME_INSTR_L1ROUTER_REQUEST].inc(ics.l1router_request);
  profiler::stats[STATNAME_INSTR_L2ROUTER_REQUEST].inc(ics.l2router_request);
  profiler::stats[STATNAME_INSTR_L2GC_REQUEST].inc(ics.l2gc_request);
  profiler::stats[STATNAME_INSTR_TILE_REQUESTBUFFER].inc(ics.tile_requestbuffer);
  profiler::stats[STATNAME_INSTR_L1ROUTER_REPLY].inc(ics.l1router_reply);
  profiler::stats[STATNAME_INSTR_L2ROUTER_REPLY].inc(ics.l2router_reply);
  profiler::stats[STATNAME_INSTR_L2GC_REPLY].inc(ics.l2gc_reply);
  profiler::stats[STATNAME_INSTR_TILE_REPLYBUFFER].inc(ics.tile_replybuffer);
  profiler::stats[STATNAME_INSTR_GNET_TILEREQUEST].inc(ics.gnet_tilerequest);
  profiler::stats[STATNAME_INSTR_GNET_GCREQUEST].inc(ics.gnet_gcrequest);
  profiler::stats[STATNAME_INSTR_GNET_REQUESTBUFFER].inc(ics.gnet_requestbuffer);
  profiler::stats[STATNAME_INSTR_GNET_REPLYBUFFER].inc(ics.gnet_replybuffer);
  profiler::stats[STATNAME_INSTR_GNET_GCREPLY].inc(ics.gnet_gcreply);
  profiler::stats[STATNAME_INSTR_GNET_TILEREPLY].inc(ics.gnet_tilereply);
  profiler::stats[STATNAME_INSTR_SENDING_BCAST_NOTIFIES].inc(ics.sending_bcast_notifies);
  profiler::stats[STATNAME_INSTR_GC_FILL].inc(ics.gc_fill);
  profiler::stats[STATNAME_INSTR_GC_MSHR].inc(ics.gc_mshr);
  profiler::stats[STATNAME_INSTR_GC_WRITE_ALLOCATE].inc(ics.gc_write_allocate);
  profiler::stats[STATNAME_INSTR_GC_PENDING].inc(ics.gc_pending);
}

////////////////////////////////////////////////////////////////////////////////
// Profile::handle_network_stats()
////////////////////////////////////////////////////////////////////////////////
// Called from the cluster cache after successsfully receiving  or sending a
// message from the network.  Counts the number of input and output messages at
// the L2D.
//
// FIXME: Move the IC_MSG* logic to icmsg-specific files.
////////////////////////////////////////////////////////////////////////////////
void 
Profile::handle_network_stats(uint32_t addr, icmsg_type_t type) 
{
  using namespace rigel::cache;
  using namespace rigel::memory;
  using namespace rigel;

  uint32_t stack_size = PER_THREAD_STACK_SIZE*THREADS_PER_CLUSTER*NUM_CLUSTERS;
  uint32_t bottom_of_stack = 0UL - stack_size;

  switch (type) {
    case IC_MSG_INSTR_READ_REQ:
			rigel::profiler::stats[STATNAME_L2OUT_INSTR_REQ].inc();
			rigel::profiler::stats[STATNAME_L2OUT_TOTAL].inc();
			break;
    case IC_MSG_READ_REQ:          
      (addr < bottom_of_stack) ?
        rigel::profiler::stats[STATNAME_L2OUT_READ_NONSTACK_REQ].inc() :
        rigel::profiler::stats[STATNAME_L2OUT_READ_STACK_REQ].inc();
			rigel::profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    case IC_MSG_WRITE_REQ:               
      (addr < bottom_of_stack) ?
      rigel::profiler::stats[STATNAME_L2OUT_WRITE_NONSTACK_REQ].inc() :
      rigel::profiler::stats[STATNAME_L2OUT_WRITE_STACK_REQ].inc();
      profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    case IC_MSG_LINE_WRITEBACK_REQ:      
      rigel::profiler::stats[STATNAME_L2OUT_WRITEBACK_REQ].inc();
      profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    case IC_MSG_BCAST_INV_REPLY:         
    case IC_MSG_CC_INVALIDATE_ACK:       
    case IC_MSG_CC_INVALIDATE_NAK:       
    case IC_MSG_CC_WR_RELEASE_ACK:       
    case IC_MSG_CC_WR_RELEASE_NAK:       
    case IC_MSG_CC_RD_RELEASE_ACK:       
    case IC_MSG_CC_RD_RELEASE_NAK:       
    case IC_MSG_CC_WR2RD_DOWNGRADE_ACK:  
    case IC_MSG_CC_WR2RD_DOWNGRADE_NAK:  
    case IC_MSG_CC_BCAST_ACK:            
    case IC_MSG_SPLIT_BCAST_INV_REPLY:
    case IC_MSG_SPLIT_BCAST_SHR_REPLY:
    case IC_MSG_SPLIT_BCAST_OWNED_REPLY:
      rigel::profiler::stats[STATNAME_L2OUT_COHERENCE_REPLY].inc();
      profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    case IC_MSG_GLOBAL_READ_REQ:         
    case IC_MSG_GLOBAL_WRITE_REQ:        
    case IC_MSG_BCAST_UPDATE_REQ:        
    case IC_MSG_BCAST_INV_REQ:           
      rigel::profiler::stats[STATNAME_L2OUT_GLOBAL_REQ].inc();
      profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    case IC_MSG_ATOMXCHG_REQ:            
    case IC_MSG_ATOMDEC_REQ:             
    case IC_MSG_ATOMINC_REQ:             
    case IC_MSG_ATOMCAS_REQ:             
    case IC_MSG_ATOMADDU_REQ:            
    case IC_MSG_ATOMMAX_REQ:             
    case IC_MSG_ATOMMIN_REQ:             
    case IC_MSG_ATOMOR_REQ:              
    case IC_MSG_ATOMAND_REQ:             
    case IC_MSG_ATOMXOR_REQ:             
      rigel::profiler::stats[STATNAME_L2OUT_ATOMIC_REQ].inc();
      profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    case IC_MSG_PREFETCH_NGA_REQ:        
    case IC_MSG_PREFETCH_BLOCK_GC_REQ:
    case IC_MSG_PREFETCH_BLOCK_CC_REQ:
    case IC_MSG_PREFETCH_REQ:            
    case IC_MSG_CCACHE_HWPREFETCH_REQ:   
      rigel::profiler::stats[STATNAME_L2OUT_PREFETCH_REQ].inc();
      profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    case IC_MSG_EVICT_REQ:               
      (addr < bottom_of_stack) ?
      rigel::profiler::stats[STATNAME_L2OUT_EVICT_NONSTACK_REQ].inc() :
      rigel::profiler::stats[STATNAME_L2OUT_EVICT_STACK_REQ].inc();
      profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    case IC_MSG_CC_RD_RELEASE_REQ:       
      (addr < bottom_of_stack) ?
      rigel::profiler::stats[STATNAME_L2OUT_READ_RELEASE_NONSTACK].inc() :
      rigel::profiler::stats[STATNAME_L2OUT_READ_RELEASE_STACK].inc();
      profiler::stats[STATNAME_L2OUT_TOTAL].inc();
      break;
    // BEGIN REPLY MESSAGES
    case IC_MSG_INSTR_READ_REPLY:
			rigel::profiler::stats[STATNAME_L2IN_INSTR_REPLY].inc();
			rigel::profiler::stats[STATNAME_L2IN_TOTAL].inc();
			break;
    case IC_MSG_READ_REPLY:
      rigel::profiler::stats[STATNAME_L2IN_READ_REPLY].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    case IC_MSG_WRITE_REPLY:             
      rigel::profiler::stats[STATNAME_L2IN_WRITE_REPLY].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    case IC_MSG_LINE_WRITEBACK_REPLY:    
      rigel::profiler::stats[STATNAME_L2IN_WRITEBACK_REPLY].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    case IC_MSG_GLOBAL_READ_REPLY:       
    case IC_MSG_GLOBAL_WRITE_REPLY:      
    case IC_MSG_BCAST_UPDATE_REPLY:      
      rigel::profiler::stats[STATNAME_L2IN_GLOBAL_REPLY].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    case IC_MSG_ATOMXCHG_REPLY:          
    case IC_MSG_ATOMDEC_REPLY:           
    case IC_MSG_ATOMINC_REPLY:           
    case IC_MSG_ATOMCAS_REPLY:           
    case IC_MSG_ATOMADDU_REPLY:          
    case IC_MSG_ATOMMAX_REPLY:           
    case IC_MSG_ATOMMIN_REPLY:           
    case IC_MSG_ATOMOR_REPLY:            
    case IC_MSG_ATOMAND_REPLY:           
    case IC_MSG_ATOMXOR_REPLY:           
      rigel::profiler::stats[STATNAME_L2IN_ATOMIC_REPLY].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    case IC_MSG_PREFETCH_REPLY:
    case IC_MSG_PREFETCH_BLOCK_GC_REPLY:
    case IC_MSG_PREFETCH_BLOCK_CC_REPLY:
    case IC_MSG_PREFETCH_NGA_REPLY:      
    case IC_MSG_CCACHE_HWPREFETCH_REPLY:
      rigel::profiler::stats[STATNAME_L2IN_PREFETCH_REPLY].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    case IC_MSG_EVICT_REPLY:             
      rigel::profiler::stats[STATNAME_L2IN_EVICT_REPLY].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    case IC_MSG_BCAST_INV_NOTIFY:        
    case IC_MSG_BCAST_UPDATE_NOTIFY:     
    case IC_MSG_CC_WR_RELEASE_PROBE:     
    case IC_MSG_CC_INVALIDATE_PROBE:     
    case IC_MSG_CC_RD_RELEASE_PROBE:     
    case IC_MSG_CC_WR2RD_DOWNGRADE_PROBE:
    case IC_MSG_CC_BCAST_PROBE:          
    case IC_MSG_SPLIT_BCAST_INV_REQ:          
    case IC_MSG_SPLIT_BCAST_SHR_REQ:          
      rigel::profiler::stats[STATNAME_L2IN_COHERENCE_PROBE].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    case IC_MSG_CC_RD_RELEASE_REPLY:     
      rigel::profiler::stats[STATNAME_L2IN_READ_RELEASE].inc();
      profiler::stats[STATNAME_L2IN_TOTAL].inc();
      break;
    default:
      DEBUG_HEADER();
      fprintf(stderr, "Addr: %08x type: %d\n", addr, type);
      assert(0 && "Unknown Message type!");
  }
}



