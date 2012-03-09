////////////////////////////////////////////////////////////////////////////////
// cmdline_parse.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  Handles command line parsing and sets parameters and options
//  for the simulator
////////////////////////////////////////////////////////////////////////////////
#include <assert.h>                     // for assert
#include <errno.h>                      // for EEXIST, errno
#include <stddef.h>                     // for size_t, NULL
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for sprintf, printf, snprintf, etc
#include <stdlib.h>                     // for atoi, exit, abort, free, etc
#include <sys/stat.h>                   // for mkdir, S_IRGRP, S_IRWXU, etc
#include <time.h>                       // for time_t
#include <cstring>                      // for strncmp, strncpy
#include <fstream>                      // for operator<<, basic_ostream, etc
#include <iomanip>                      // for operator<<, std::setw, _std::setw
#include <iostream>                     // for std::cout, std::cerr
#include <list>                         // for list
#include <map>                          // for map, map<>::mapped_type
#include <sstream>                      // for std::ostringstream, stringstream
#include <string>                       // for string, char_traits, etc
#include <vector>                       // for vector, allocator, etc
#include "define.h"         // for ::TQ_SYSTEM_BASELINE, etc
#include "memory/dram.h"
#include "sim.h"            // for CMDLINE_DUMP_PER_PC_STATS, etc
#include "util/util.h"
#include "profile/profile.h"
#include "RandomLib/Random.hpp"  // for Random

static std::string tq_type_string("-tq_type [default|baseline|interval|int_lifo|recent|recent_v2|rand]");

std::string CommandLineArgs::exec_name;

// Globally-defined values make sense in util.cpp since it gets linked into
// rigelsim early
FILE *rigel::memory::cctrace_out;
FILE *rigel::memory::dramtrace_out;
FILE *rigel::memory::coherencetrace_out;
FILE *rigel::memory::direvicttrace_out;
// File handle for RigelPrint()/printreg output.
FILE * rigel::RIGEL_PRINT_FILEP;


////////////////////////////////////////////////////////////////////////////////
// namespace rigel
////////////////////////////////////////////////////////////////////////////////
//
//  namespace which stores parameters and control variables
//  for the simulator. These are externally defined from sim.h
////////////////////////////////////////////////////////////////////////////////
namespace rigel {
  uint64_t CYC_COUNT_MAX;
  uint64_t CURR_CYCLE;
  char MEM_DUMP_FILE[1024];
	std::string DUMPFILE_PATH;
	std::string RIGELSIM_BINARY_PATH;
	std::string BENCHMARK_BINARY_PATH;
  std::vector<std::string> SIM_TAGS;
  time_t SIM_START_TIME;
  time_t SIM_END_TIME;
	std::string COUCHDB_HOSTNAME;
  int COUCHDB_PORT;
	std::string COUCHDB_DBNAME;
  std::ostringstream STDIN_AS_STRING(std::stringstream::out);
  bool STDIN_AS_STRING_SHORT_ENOUGH;
  std::vector<std::string> TARGET_ARGS;
  // Total clusters on the chip
  int NUM_CLUSTERS;
  int CLUSTERS_PER_TILE;
  int NUM_TILES;
  int NUM_GCACHE_BANKS;
  int CORES_TOTAL;
  int THREADS_TOTAL;
  int THREADS_PER_CLUSTER;
  int THREADS_PER_CORE;
  int L1DS_PER_CORE;
  int L1DS_PER_CLUSTER;
  // Nonblocking loads/stores in core
  bool NONBLOCKING_MEM;
  bool GENERATE_CCTRACE;
  bool GENERATE_DRAMTRACE;
  bool GENERATE_COHERENCETRACE;
  bool GENERATE_DIREVICTTRACE;
  bool SLEEPY_CORES;
  bool START_AWAKE_NOT_ASLEEP;
	bool SINGLE_THREADED_MODE;
  bool TRACK_BIT_PATTERNS;
  PipelineBypassPaths PIPELINE_BYPASS_PATHS;
  bool SIGINT_STATS;
  bool PRINT_GLOBAL_TIMING_STATS;
  bool INIT_REGISTER_FILE;
  bool DUMP_REGISTER_FILE;
  bool DUMP_REGISTER_TRACE;
  bool DUMP_ELF_IMAGE;
  bool LOAD_CHECKPOINT;
  size_t NUM_BTB_ENTRIES;
  bool CMDLINE_MODEL_CONTENTION;
  bool CMDLINE_GLOBAL_CACHE_MRU;
  bool CMDLINE_SHORT_RIGELPRINT;
  bool CMDLINE_SUPPRESS_RIGELPRINT;
  bool CMDLINE_INTERACTIVE_MODE;
  bool CMDLINE_DUMP_SHARING_STATS;
  bool CMDLINE_VERBOSE_DRAM_STATS;
  bool CMDLINE_CHECK_STACK_ACCESSES;
  bool CMDLINE_ENUMERATE_MEMORY;
  bool ENABLE_EXPERIMENTAL_DIRECTORY;
  bool ENABLE_OVERFLOW_DIRECTORY;
  int COHERENCE_DIR_SETS;
  int COHERENCE_DIR_WAYS;
  DirectoryReplacementPolicy DIRECTORY_REPLACEMENT_POLICY;
  int INTERACTIVE_CORENUM;
  bool ENABLE_OVERFLOW_MODEL_TIMING;
  int HEARTBEAT_INTERVAL;
  bool CMDLINE_ENABLE_GCACHE_NOALLOCATE_WR2WR_TRANSFERS;
  bool CMDLINE_ENABLE_HYBRID_COHERENCE;
  int CMDLINE_MAX_BCASTS_PER_DIRECTORY;
  bool ENABLE_LOCALITY_TRACKING;
  bool ENABLE_TLB_TRACKING;
  bool STDIN_STRING_ENABLE;
  std::string STDIN_STRING;
  std::string STDIN_STRING_FILENAME;
  bool REDIRECT_HOST_STDIN;
  bool REDIRECT_HOST_STDOUT;
  bool REDIRECT_HOST_STDERR;
  bool REDIRECT_TARGET_STDOUT;
  bool REDIRECT_TARGET_STDERR;
  std::string TARGET_STDOUT_FILENAME;
  std::string TARGET_STDERR_FILENAME;
  bool CMDLINE_ENABLE_INCOHERENT_MALLOC;
  bool CMDLINE_ENABLE_FREE_LIBPAR;
  bool CMDLINE_ENABLE_FREE_LIBPAR_ICACHE;
  bool SINGLE_CYCLE_ALUS;
  // Latencies in the network when using idealized interconnect.
  uint32_t TILENET_GNET2CC_LAT_L1;
  uint32_t TILENET_CC2GNET_LAT_L1;
  uint32_t TILENET_GNET2CC_LAT_L2;
  uint32_t TILENET_CC2GNET_LAT_L2;
  // Minimum address that the stack could be.  Set in crt0.S at simulator boot.
  uint32_t STACK_MIN_BASEADDR;
  // Enable idealized memory model.
  bool CMDLINE_ENABLE_IDEALIZED_DRAM;
  uint32_t CMDLINE_IDEALIZED_DRAM_LATENCY;

  // Used in the memory model for scheduling
  namespace mem_sched {
    DRAMSchedulingPolicy DRAM_SCHEDULING_POLICY;
    DRAMBatchingPolicy DRAM_BATCHING_POLICY;
    DRAMRowCacheReplacementPolicy ROW_CACHE_REPLACEMENT_POLICY;
    unsigned int DRAM_BATCHING_CAP;
    int PENDING_PER_BANK;
    bool MONOLITHIC_SCHEDULER;
  };
  namespace cache {
    int GCACHE_ACCESS_LATENCY;
    bool PERFECT_L1D;
    bool PERFECT_L1I;
    bool PERFECT_L2D;
    bool PERFECT_L2I;
    bool PERFECT_GLOBAL_CACHE;
    bool PERFECT_GLOBAL_OPS;
    // Stores never stall
    bool CMDLINE_FREE_WRITES;
    int L2D_PORTS_PER_BANK;
    bool CMDLINE_NONBLOCKING_PREFETCH;
    int CMDLINE_CCPREFETCH_SIZE;
    bool CMDLINE_CCPREFETCH_STRIDE;
    int CMDLINE_GCACHE_PREFETCH_SIZE;
    bool GCACHE_BULK_PREFETCH;
    size_t GCACHE_REPLY_PORTS_PER_BANK;
    size_t GCACHE_REQUEST_PORTS_PER_BANK;
    uint32_t MAX_LIMITED_PTRS;
    bool ENABLE_LIMITED_DIRECTORY;
    bool ENABLE_PROBE_FILTER_DIRECTORY;
    bool ENABLE_BCAST_NETWORK;
  };
	extern RandomLib::Random TargetRNG;
	extern RandomLib::Random RNG;
};

////////////////////////////////////////////////////////////////////////////////
// CommandLineArgs::get_val_bool
////////////////////////////////////////////////////////////////////////////////
// Returns a boolean value from the commandline table
// Parameters: char* s - the key used to index the command line table
////////////////////////////////////////////////////////////////////////////////
bool
CommandLineArgs::get_val_bool(char *s) {
  return ("1" == (this->cmdline_table[std::string(s)]) || "true" == (this->cmdline_table[std::string(s)]));
}

////////////////////////////////////////////////////////////////////////////////
// CommandLineArgs::get_val_int
////////////////////////////////////////////////////////////////////////////////
// Returns an int value from the commandline table
// Parameters: char* s - the key used to index the command line table
////////////////////////////////////////////////////////////////////////////////
int
CommandLineArgs::get_val_int(char *s) {
  return atoi((this->cmdline_table[std::string(s)]).c_str());
}

////////////////////////////////////////////////////////////////////////////////
// CommandLineArgs::get_val
////////////////////////////////////////////////////////////////////////////////
// Returns a string value directly from the commandline table
// Parameters: char* s - the key used to index the command line table
////////////////////////////////////////////////////////////////////////////////
std::string
CommandLineArgs::get_val(char *s) {
  return cmdline_table[std::string(s)];
}

////////////////////////////////////////////////////////////////////////////////
// CommandLineArgs Constructor
////////////////////////////////////////////////////////////////////////////////
// Constructor
// Sets up initial values and parameters for the simulator
// Also, parse the commandline and set the simulation parameters
////////////////////////////////////////////////////////////////////////////////
CommandLineArgs::CommandLineArgs(int argc, char *argv[]) {

  if(argc == 1) {
     print_help();
    ExitSim(0);
  }

  CommandLineArgs::exec_name = std::string(argv[0]);

  /* Initial key settings */
  this->cmdline_table["instr_count"] = "-1"; // Run forever
  this->cmdline_table["interactive"] = "0";    // Not interactive
  this->cmdline_table["memdump"] = "dump.mem.out"; // No output by default
  rigel::DUMPFILE_PATH = std::string("RUN_OUTPUT"); // Junk directory by default
  this->cmdline_table["heartbeat_print_pcs"] = "false"; // Do not dump PCs at heartbeat
  this->cmdline_table["TQ_SYSTEM_TYPE"] = "0";
  this->cmdline_table["cctrace"] = "0";
  this->cmdline_table["dramtrace"] = "0";
  this->cmdline_table["coherencetrace"] = "0";
  this->cmdline_table["direvicttrace"] = "0";
  rigel::SIGINT_STATS = false; //Do not dump stats on SIGINT catch by default
  rigel::SLEEPY_CORES = false; //Do not turn cores off on C$ misses by default
  rigel::START_AWAKE_NOT_ASLEEP = false; // all cores start in sleep mode by default (FIXME: we probably want this the opposite way...)
  rigel::TRACK_BIT_PATTERNS = false; //Don't track bit patterns by default, it's expensive.
  rigel::PIPELINE_BYPASS_PATHS = rigel::pipeline_bypass_full;
  /******* BEGIN ACTUAL SIMULATION PARAMETERS ******/
  this->cmdline_table["NUM_CLUSTERS"] = "1";
  this->cmdline_table["NUM_THREADS"] = "1";
  this->cmdline_table["NUM_TILES"] = "1";
  this->cmdline_table["MEMCHANNELS"] = "-1";
  this->cmdline_table["PENDING_PER_BANK"] = "16";
  rigel::mem_sched::MONOLITHIC_SCHEDULER = false;
  this->cmdline_table["DRAM_SCHEDULING_POLICY"] = rigel::DRAM::DRAM_SCHEDULING_POLICY_DEFAULT;
  this->cmdline_table["DRAM_BATCHING_POLICY"] = rigel::DRAM::DRAM_BATCHING_POLICY_DEFAULT;
  //TODO: Some of these defaults are in sim.h, some in dram.h.  Clean this up.
  this->cmdline_table["ROW_CACHE_REPLACEMENT_POLICY"] = rigel::mem_sched::ROW_CACHE_REPLACEMENT_POLICY_DEFAULT;
  this->cmdline_table["DRAM_OPEN_ROWS"] = rigel::DRAM::DRAM_OPEN_ROWS_DEFAULT;
  this->cmdline_table["DRAM_BATCHING_CAP"] = "0";
  this->cmdline_table["HEARTBEAT_INTERVAL"] = "100000";

  // In Profile::global_dump_profile() perform the check
  this->cmdline_table["PRINT_GLOBAL_TIMING_STATS"] = "1";
  rigel::PRINT_GLOBAL_TIMING_STATS = true;

  // Read the contents of the regfile from a file at the start of simulation
  this->cmdline_table["INIT_REGISTER_FILE"] = "0";
  rigel::INIT_REGISTER_FILE = false;
  // Print the contents of the register file for each core on exit
  this->cmdline_table["DUMP_REGISTER_FILE"] = "0";
  rigel::DUMP_REGISTER_FILE = false;

  // Dump a trace of all register file writes
  this->cmdline_table["DUMP_REGISTER_TRACE"] = "0";
  rigel::DUMP_REGISTER_TRACE = false;

  // Load a checkpoint 
  this->cmdline_table["LOAD_CHECKPOINT"] = "0";
  rigel::LOAD_CHECKPOINT = false;

  // Dump ELF file image
  this->cmdline_table["DUMP_ELF_IMAGE"] = "0";
  rigel::DUMP_ELF_IMAGE = false;

  // By default, do not profile memory operations at the global cache
  rigel::profiler::PROFILE_HIST_GCACHEOPS = false;
  rigel::profiler::gcacheops_histogram_bin_size = 10000;


  // By default, do not allow the simulation to continue if the dump directory exists and is the not the default path
  this->cmdline_table["OVERWITE_DUMP_DIRECTORY"] = "false";

  //Setup default CouchDB parameters
  rigel::COUCHDB_HOSTNAME = "http://corezilla.crhc.illinois.edu";
  rigel::COUCHDB_PORT = 5984;
  rigel::COUCHDB_DBNAME = "rigelsim-test";
  
  //SHORT_ENOUGH should be true by default, then set to false if MAXLENGTH is exceeded
  rigel::STDIN_AS_STRING_SHORT_ENOUGH = true;
  // By default, do not print per-pc stats
  rigel::profiler::CMDLINE_DUMP_PER_PC_STATS = true;

  // Default is no prefix for output file names
  rigel::profiler::DUMPFILE_PREFIX = std::string("");

  // Default is to *not* have perfect caches.
  rigel::cache::PERFECT_L1D = false;
  rigel::cache::PERFECT_L1I = false;
  rigel::cache::PERFECT_L2D = false;
  rigel::cache::PERFECT_L2I = false;
  rigel::cache::PERFECT_GLOBAL_CACHE = false;
  rigel::cache::PERFECT_GLOBAL_OPS = false;
  rigel::cache::CMDLINE_FREE_WRITES = false;
  // Default is to use auto generated latencies.
  rigel::SINGLE_CYCLE_ALUS = false;

  // Default is to have contention modeling *ON*.
  rigel::CMDLINE_MODEL_CONTENTION = true;

  // Use MRU at global cache instead of LRU.
  rigel::CMDLINE_GLOBAL_CACHE_MRU = false;

  // Default is to have nonblocking mem turned off.
  rigel::NONBLOCKING_MEM = rigel::DEFAULT_NONBLOCKING_MEM;

  // Default is to block on prefetches that cannot get an MSHR at the CCache.
  rigel::cache::CMDLINE_NONBLOCKING_PREFETCH = false;

  // Default is no prefetching into the cluster cache
  rigel::cache::CMDLINE_CCPREFETCH_SIZE = 0;

  // Default is next-4-line prefetching into the global cache
  rigel::cache::CMDLINE_GCACHE_PREFETCH_SIZE =
    rigel::cache::DEFAULT_GCACHE_PREFETCH_SIZE;

  // Default is to turn on G$ bulk prefetching (1 MSHR/DRAM request per set of prefetches)
  rigel::cache::GCACHE_BULK_PREFETCH = true;
    
  // Default is to take two cycles to turn around an access at the GCache.
  rigel::cache::GCACHE_ACCESS_LATENCY = 1;

  // Default is one port per cluster cache bank with one bank.
  rigel::cache::L2D_PORTS_PER_BANK = rigel::cache::DEFAULT_L2D_PORTS_PER_BANK;
  
  // Default is to use standard (long) RigelPrint output.
  rigel::CMDLINE_SHORT_RIGELPRINT = false;
  rigel::CMDLINE_SUPPRESS_RIGELPRINT = false;
  rigel::CMDLINE_INTERACTIVE_MODE = false;

  // Default is to only dump aggregate DRAM stats
  rigel::CMDLINE_VERBOSE_DRAM_STATS = false;

  // Default is NO sharing stats dumped (EXPENSIVE!)
  rigel::CMDLINE_DUMP_SHARING_STATS = false;

  // Default is to have stack smashing off.  Probably want to make it on in the
  // future.
  rigel::CMDLINE_CHECK_STACK_ACCESSES = false;

  // Check all of memory for address aliasing. ***SLOW***
  rigel::CMDLINE_ENUMERATE_MEMORY = false;

  // By default, keep coherence off.
  rigel::ENABLE_EXPERIMENTAL_DIRECTORY = false;
  rigel::ENABLE_OVERFLOW_DIRECTORY = false;
  rigel::ENABLE_OVERFLOW_MODEL_TIMING = true;
  // For now we will keep this off, but we may want to flip it the other way if
  // we find Wr2Wr transfers without allocation at the GCache to be beneficial.
  rigel::CMDLINE_ENABLE_GCACHE_NOALLOCATE_WR2WR_TRANSFERS = true;
  // Turn on hybrid coherence to allow mixed SW/HW coherence in RigelSim.
  rigel::CMDLINE_ENABLE_HYBRID_COHERENCE = false;
  // Initially we allow infinite outstanding broadcasts per directory.
  rigel::CMDLINE_MAX_BCASTS_PER_DIRECTORY = 0;
  // Leave off by default, until we can quantify slowdown (FIXME: Quantify slowdown)
  rigel::ENABLE_LOCALITY_TRACKING = false;
  // Don't do TLB statistics gathering by default, it can be expensive if we're doing a design space sweep.
  rigel::ENABLE_TLB_TRACKING = false;
  // Do not redirect any streams internally by default.
  rigel::REDIRECT_HOST_STDIN = false;
  rigel::REDIRECT_HOST_STDOUT = false;
  rigel::REDIRECT_HOST_STDERR = false;
  // By default, disable incoherent malloc.
  rigel::CMDLINE_ENABLE_INCOHERENT_MALLOC = false;
  // By default, pay timing costs for libpar data acesses.
  rigel::CMDLINE_ENABLE_FREE_LIBPAR = false;
  rigel::CMDLINE_ENABLE_FREE_LIBPAR_ICACHE = false;

  // Oversized by a lot to ensure that it works.
  // Fully associative, 8 times the size of the G$.
  rigel::COHERENCE_DIR_SETS = 1;
  rigel::COHERENCE_DIR_WAYS = 8*rigel::cache::GCACHE_SETS*rigel::cache::GCACHE_WAYS;
  // Default is LRU replacement policy
  rigel::DIRECTORY_REPLACEMENT_POLICY = rigel::dir_rep_lru;
  
  rigel::cache::GCACHE_REPLY_PORTS_PER_BANK = rigel::cache::GCACHE_REPLY_PORTS_PER_BANK_DEFAULT;
  rigel::cache::GCACHE_REQUEST_PORTS_PER_BANK = rigel::cache::GCACHE_REQUEST_PORTS_PER_BANK_DEFAULT;

  // Configuration of limited directory.  Off by default.
  rigel::cache::MAX_LIMITED_PTRS = rigel::cache::DEFAULT_MAX_LIMITED_PTRS;
  rigel::cache::ENABLE_LIMITED_DIRECTORY = rigel::cache::DEFAULT_ENABLE_LIMITED_DIRECTORY;
  rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY = false;
  rigel::cache::ENABLE_BCAST_NETWORK = false;

  // (Practically) infinite BTB.
  rigel::NUM_BTB_ENTRIES = 0x10;
  
  // Setup idealized DRAM.
  rigel::CMDLINE_IDEALIZED_DRAM_LATENCY = 0;
  rigel::CMDLINE_ENABLE_IDEALIZED_DRAM = false;

  // Latencies in the network when using idealized interconnect.
  rigel::TILENET_GNET2CC_LAT_L1 = 2;
  rigel::TILENET_GNET2CC_LAT_L2 = 2;
  rigel::TILENET_CC2GNET_LAT_L1 = 2;
  rigel::TILENET_CC2GNET_LAT_L2 = 2;
  // Task queue latencies are by default zero (ideal TQ)
  {
    using namespace rigel::task_queue;
    char num_str[16];
    snprintf(num_str, 16, "%d", LATENCY_ENQUEUE_ONE);
    this->cmdline_table["LATENCY_ENQUEUE_ONE"] = num_str;
    snprintf(num_str, 16, "%d", LATENCY_ENQUEUE_LOOP);
    this->cmdline_table["LATENCY_ENQUEUE_LOOP"] = num_str;
    snprintf(num_str, 16, "%d", LATENCY_DEQUEUE);
    this->cmdline_table["LATENCY_DEQUEUE"] = num_str;
    snprintf(num_str, 16, "%d", LATENCY_BLOCKED_SYNC);
    this->cmdline_table["LATENCY_BLOCKED_SYNC"] = num_str;
  }
  /******* END ACTUAL SIMULATION PARAMETERS ******/

  std::vector<char *> argList;
  //If first parameter is -argfile, we know to parse command-line args
  //from the file whose name is given by the following argument,
  //rather than from the rest of argv
	std::string firstArg(argv[1]);
  bool fromArgFile = (firstArg.compare("-argfile") == 0);

  if(fromArgFile)
  {
    std::ifstream argfile(argv[2]);
    if(!argfile.is_open()) //opening argv[2] failed
    {
      printf("Error opening argfile %s\n", argv[2]);
      exit(1);
    }
    while(!argfile.eof())
    {
      char *tmp = (char *)malloc(1000*sizeof(*tmp));
      if(tmp == NULL)
      {
        printf("Error allocating temporary string for -argfile\n");
        exit(1);
      }
			std::string tmpstr;
      argfile >> tmpstr;
      if(tmpstr.length() > 0)
      {
        strncpy(tmp, tmpstr.c_str(), 1000);
        argList.push_back(tmp);
        //printf("Arg: |%s|\n", tmp);
      }
    }
  }
  else
  {
    for(int i = 1; i < argc; i++)
    {
      argList.push_back(argv[i]);
    }
  }

  //After initialization, we parse the commandline for arguments
  //and set the parameters
  for  (unsigned int i = 0; i < argList.size(); ) {
		std::string key(argList[i++]);
    /* Parse arguments one-by-one */

    /*XXX*/
    if (0 == key.compare("--max-bcasts")) {
      /* Number of clusters per tile */
      if (i == argList.size()) {
        throw CommandLineError("--max-bcasts <num bcasts per directory>");
      }
      rigel::CMDLINE_MAX_BCASTS_PER_DIRECTORY = atoi(argList[i++]);
      continue;
    }
    if (0 == key.compare("--free-libpar-dcache")) {
      rigel::CMDLINE_ENABLE_FREE_LIBPAR = true;
      continue;
    }
    if (0 == key.compare("--free-libpar-icache")) {
      rigel::CMDLINE_ENABLE_FREE_LIBPAR_ICACHE = true;
      continue;
    }
    if (0 == key.compare("--tags")) {
      StringTokenizer::tokenize(argList[i++], rigel::SIM_TAGS, ",");
      continue;
    } 
    if (0 == key.compare("--coherence")) {
      rigel::ENABLE_EXPERIMENTAL_DIRECTORY = true;
      continue;
    }
    if (0 == key.compare("--limited-directory")) {
      rigel::ENABLE_EXPERIMENTAL_DIRECTORY = true;
      rigel::cache::ENABLE_LIMITED_DIRECTORY = true;
      continue;
    }
    if (0 == key.compare("--bcast-network")) {
      rigel::cache::ENABLE_BCAST_NETWORK = true;
      continue;
    }
    if (0 == key.compare("--probe-filter-directory")) {
      rigel::ENABLE_EXPERIMENTAL_DIRECTORY = true;
      rigel::cache::ENABLE_PROBE_FILTER_DIRECTORY = true;
      rigel::cache::ENABLE_LIMITED_DIRECTORY = true;
      continue;
    }
    if (0 == key.compare("--directory-max-ptrs")) {
      rigel::cache::MAX_LIMITED_PTRS = atoi(argList[i++]);
      continue;
    }
    if (0 == key.compare("--no-coherence")) {
      rigel::ENABLE_EXPERIMENTAL_DIRECTORY = false;
      continue;
    }
    if (0 == key.compare("--overflow-timing")) {
      rigel::ENABLE_OVERFLOW_MODEL_TIMING = true;
      continue;
    }
    if (0 == key.compare("--no-overflow-timing")) {
      rigel::ENABLE_OVERFLOW_MODEL_TIMING = false;
      continue;
    }
    if (0 == key.compare("--overflow-directory")) {
      rigel::ENABLE_OVERFLOW_DIRECTORY = true;
      continue;
    }
    if (0 == key.compare("--no-overflow-directory")) {
      rigel::ENABLE_OVERFLOW_DIRECTORY = false;
      continue;
    }
    if (0 == key.compare("--disable-wr2wr-noalloc")) {
      rigel::CMDLINE_ENABLE_GCACHE_NOALLOCATE_WR2WR_TRANSFERS = false;
      continue;
    }
    if (0 == key.compare("--enable-hybrid-coherence")) {
      rigel::ENABLE_EXPERIMENTAL_DIRECTORY = true;
      rigel::CMDLINE_ENABLE_HYBRID_COHERENCE = true;
      rigel::CMDLINE_ENABLE_INCOHERENT_MALLOC = true;
      continue;
    }
    //if (0 == key.compare("--enable-incoherent-malloc")) {
    //  rigel::CMDLINE_ENABLE_INCOHERENT_MALLOC = true;
    //  continue;
    //}
    if (0 == key.compare("-lt")) {
      rigel::ENABLE_LOCALITY_TRACKING = true;
      continue;
    }
    if (0 == key.compare("-tlb")) {
      rigel::ENABLE_TLB_TRACKING = true;
      continue;
    }
    /*XXX Initialize regfile to a saved state by reading it in from a file */
    if (0 == key.compare("-regfile-init")) {
      this->cmdline_table["INIT_REG_FILE"] = "1";
      rigel::INIT_REGISTER_FILE = true;
      continue;
    }
    /*XXX Dump regfile on simulation end */
    if (0 == key.compare("-regfile-dump")) {
      this->cmdline_table["DUMP_REGISTER_FILE"] = "1";
      rigel::DUMP_REGISTER_FILE = true;
      continue;
    }
    /*XXX Dump regfile trace */
    if (0 == key.compare("-regfile-trace-dump")) {
      this->cmdline_table["DUMP_REGISTER_TRACE"] = "1";
      rigel::DUMP_REGISTER_TRACE = true;
      continue;
    }
    /*XXX Load Checkpoint */
    if (0 == key.compare("-load-checkpoint")) {
      this->cmdline_table["LOAD_CHECKPOINT"] = "1";
      rigel::LOAD_CHECKPOINT = true;
      continue;
    }
    // dump code image (elfimage.hex)
    if (0 == key.compare("-dump-elf-image")) {
      this->cmdline_table["DUMP_ELF_IMAGE"] = "1";
      rigel::DUMP_ELF_IMAGE = true;
      continue;
    }
    /*XXX Don't dump stats*/
    if (0 == key.compare("-no-stats-dump")) {
      this->cmdline_table["PRINT_GLOBAL_TIMING_STATS"] = "0";
      rigel::PRINT_GLOBAL_TIMING_STATS = false;
      continue;
    }
    /////// XXX: PROFILER INFORMATION :XXX ///////
    if (0 == key.compare("-per-pc-stats")) {
      rigel::profiler::CMDLINE_DUMP_PER_PC_STATS = true;
      continue;
    }
    //////// XXX: Turn off printing RigelPrints XXX ////////
    if (0 == key.compare("-no-rigelprint")) {
      rigel::CMDLINE_SUPPRESS_RIGELPRINT = true;
      continue;
    }
    //////// XXX: Turn on stack smash checking XXX ////////
    if (0 == key.compare("--memory-alias-test")) {
      rigel::CMDLINE_ENUMERATE_MEMORY = true;
      break;
    }
    //////// XXX: Turn on stack smash checking XXX ////////
    if (0 == key.compare("--stack-smash-check")) {
      rigel::CMDLINE_CHECK_STACK_ACCESSES = true;
      continue;
    }
    //////// XXX: Turn on MRU at the GCache XXX ////////
    if (0 == key.compare("--enable-gcache-mru")) {
      rigel::CMDLINE_GLOBAL_CACHE_MRU = true;
      continue;
    }
    //////// XXX: Turn on printing short RigelPrints XXX ////////
    if (0 == key.compare("-short-rigelprint")) {
      rigel::CMDLINE_SHORT_RIGELPRINT = true;
      continue;
    }
    /////// XXX: NETWORK SCHEDULING :XXX ///////
//    if (0 == key.compare("-network-core-zero-prio")) {
//      rigel::interconnect::CORE_ZERO_PRIORITY = 1;
//      continue;
//    }
//    if (0 == key.compare("-no-network-core-zero-prio")) {
//      rigel::interconnect::CORE_ZERO_PRIORITY = 0;
//      continue;
//    }

    /////// XXX: SIMULATION GENERAL PARAMS :XXX ///////
    if (0 == key.compare("-i")) {
      /* Interactive flag */
      rigel::CMDLINE_INTERACTIVE_MODE = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-n")) {
      /* Instructions to execute */
      if (i == argList.size()) {
        throw CommandLineError("-n <num instrs to run>");
      }
      this->cmdline_table["instr_count"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--args")) {
      /*Target arguments*/
      int targetArgc = atoi(std::string(argList[i++]).c_str());
      if(i+targetArgc-1 >= argList.size())
      {
        throw CommandLineError("--args <NARGS> ARG1 ARG2 .. ARGN");
      }
      for(int j = 0; j < targetArgc; j++)
      {
        rigel::TARGET_ARGS.push_back(std::string(argList[i++]));
      }
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-ndb"))
    {
      //This should screw it up enough to not insert into the DB, but we should have a bool.
      //Really, we should *only* have a bool, not a bool and a #define
      rigel::COUCHDB_HOSTNAME = std::string("FIXME");
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-db")) {
      /*Set up CouchDB hostname, port number, and db name.*/
      if (i+3-1 >= argList.size()) {
        throw CommandLineError("-db <hostname> <port #> <db name>");
      }
      rigel::COUCHDB_HOSTNAME = std::string(argList[i++]);
      rigel::COUCHDB_PORT = atoi(std::string(argList[i++]).c_str());
      rigel::COUCHDB_DBNAME = std::string(argList[i++]);
      continue;
    }
    /////// XXX: SIMULATION SYSTEM PARAMS :XXX ///////
    if (0 == key.compare("-tiles") || 0 == key.compare("-t")) {
      /* Number of tiles to simulate */
      if (i == argList.size()) {
        throw CommandLineError("-tiles <num tiles>");
      }
      this->cmdline_table["NUM_TILES"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-clusters") || 0 == key.compare("-c")) {
      /* Number of clusters per tile */
      if (i == argList.size()) {
        throw CommandLineError("-clusters <num clusters>");
      }
      this->cmdline_table["NUM_CLUSTERS"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-threads")) {
      /* Number of clusters per tile */
      if (i == argList.size()) {
        throw CommandLineError("-threads <num threads>");
      }
      this->cmdline_table["NUM_THREADS"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--gnet2cc-lat")) {
      if (i == argList.size()) {
        throw CommandLineError("--gnet2cc-lat <N>");
      }
      rigel::TILENET_GNET2CC_LAT_L1 = atoi(argList[i++]);
      rigel::TILENET_GNET2CC_LAT_L2 = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--cc2gnet-lat")) {
      if (i == argList.size()) {
        throw CommandLineError("--cc2gnet-lat <N>");
      }
      rigel::TILENET_CC2GNET_LAT_L1 = atoi(argList[i++]);
      rigel::TILENET_CC2GNET_LAT_L2 = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--btb-entries")) {
      /* Number of clusters per tile */
      if (i == argList.size()) {
        throw CommandLineError("--btb-entries <N>");
      }
      rigel::NUM_BTB_ENTRIES = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--perfect-alus")) {
      rigel::SINGLE_CYCLE_ALUS = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-nbl")) {
      rigel::NONBLOCKING_MEM = true;
      continue;
    }
    /////// XXX: SIMULATION MEMORY PARAMS :XXX ///////
    /*XXX PERFECT CACHES */
    if (0 == key.compare("-perfect-global-cache")) {
      rigel::cache::PERFECT_GLOBAL_CACHE= true;
      continue;
    }
    if (0 == key.compare("-perfect-global-ops")) {
      rigel::cache::PERFECT_GLOBAL_OPS = true;
      continue;
    }
    if (0 == key.compare("-perfect-l1d")) {
      rigel::cache::PERFECT_L1D = true;
      continue;
    }
    if (0 == key.compare("-perfect-l1i")) {
      rigel::cache::PERFECT_L1I = true;
      continue;
    }
    if (0 == key.compare("-perfect-l2d")) {
      rigel::cache::PERFECT_L2D = true;
      continue;
    }
    if (0 == key.compare("-perfect-l2i")) {
      rigel::cache::PERFECT_L2I = true;
      continue;
    }
    if (0 == key.compare("-free-writes")) {
      rigel::cache::CMDLINE_FREE_WRITES = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-model-contention-off")) {
      rigel::CMDLINE_MODEL_CONTENTION = false;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-mem-sched-pend-count")) {
      /* Schedule memory operations with priority given to row hits*/
      if (i == argList.size()) {
        throw CommandLineError("-mem-sched-pend-count <pending req. per bank at MC>");
      }
      this->cmdline_table["PENDING_PER_BANK"] = std::string(argList[i++]);
      continue;
    }
    if (0 == key.compare("-mem-sched-policy")) {
      /* Schedule memory operations with priority given to row hits*/
      if (i == argList.size()) {
        throw CommandLineError("-mem-sched-pend-count <single|perbank>");
      }
      this->cmdline_table["DRAM_SCHEDULING_POLICY"] = std::string(argList[i++]);
      continue;
    }
    if (0 == key.compare("-mem-monolithic-scheduler")) {
      /* Memory controllers have one monolithic scheduler*/
      rigel::mem_sched::MONOLITHIC_SCHEDULER = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--row-cache")) {
      /*Number of entries in per-DRAM-bank row cache (WCDRAM) */
      if (i == argList.size()) {
         throw CommandLineError("--row-cache <N>");
      }
      this->cmdline_table["DRAM_OPEN_ROWS"] = std::string(argList[i++]);
      continue;
    }
    if (0 == key.compare("--row-cache-replacement")) {
      /*Replacement policy for per-DRAM-bank row cache (WCDRAM) */
      if (i == argList.size()) {
         throw CommandLineError("--row-cache-replacement <lru|tristage|mru|lfu>");
      }
      this->cmdline_table["ROW_CACHE_REPLACEMENT_POLICY"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-l2d-ports-per-bank")) {
      /* Number of clusters per tile */
      if (i == argList.size()) {
        throw CommandLineError("-l2d-ports-per-bank <num ports>");
      }
      rigel::cache::L2D_PORTS_PER_BANK = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-memchannels")) {
      /* Number of memory controllers/channels to simulate (must be  */
      if (i == argList.size()) {
        throw CommandLineError("-memchannels <# of memory controllers/channels to simulate>");
      }
      this->cmdline_table["MEMCHANNELS"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-gcache-prefetch")) {
      /* How many lines to sequentially prefetch into the GCache on a miss */
      if (i == argList.size()) {
        throw CommandLineError("-gcache-prefetch <N>");
      }
      rigel::cache::CMDLINE_GCACHE_PREFETCH_SIZE = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-no-gcache-bulk-prefetch")) {
      /*Should we pend all G$ prefetches as one MSHR/DRAM request? */
      rigel::cache::GCACHE_BULK_PREFETCH = false;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-cc-prefetch")) {
      /* How many lines to sequentially prefetch into the CCache on a miss */
      if (i == argList.size()) {
        throw CommandLineError("-cc-prefetch <N>");
      }
      rigel::cache::CMDLINE_CCPREFETCH_SIZE = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-cc-prefetch-stride")) {
      /* Table based strdie prefetcher */
      rigel::cache::CMDLINE_CCPREFETCH_STRIDE = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--gcache-access-delay")) {
      if (i == argList.size()) {
        throw CommandLineError(" --gcache-access-delay <cycles of delay GCache access> ");
      }

      rigel::cache::GCACHE_ACCESS_LATENCY = atoi(argList[i++]);
      continue;
    }
    /////// XXX: SIMULATION PROFILING PARAMS :XXX ///////
    // Dump Task Sharing Stats
    if (0 == key.compare("-dump-sharing-stats")) {
      /*   */
      rigel::CMDLINE_DUMP_SHARING_STATS = true;
      continue;
    }
    if (0 == key.compare("-memprof")) {
      if (i == argList.size()) {
        throw CommandLineError("-memprof <cycles per bin>");
      }
      rigel::profiler::PROFILE_HIST_GCACHEOPS = true;
      rigel::profiler::gcacheops_histogram_bin_size = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-cctrace")) {
      /* Generate a cluster cache memory trace SLOW! */
      this->cmdline_table["cctrace"] = "1";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-dramtrace")) {
      /* Generate a DRAM memory trace */
      this->cmdline_table["dramtrace"] = "1";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-coherencetrace")) {
      /* Generate a trace of coherence requests coming up to the directories */
      this->cmdline_table["coherencetrace"] = "1";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-direvicttrace")) {
        /* Generate a trace of directory cache evictions */
        this->cmdline_table["direvicttrace"] = "1";
        continue;
    }
    /*XXX*/
    if (0 == key.compare("-verbose-dram-stats")) {
      /* Dump per-bank DRAM stats */
      rigel::CMDLINE_VERBOSE_DRAM_STATS = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-dumpfile-path")) {
      if(i == argList.size()) {
        throw CommandLineError("-dumpfile-path <path to output files>");
      }
      rigel::DUMPFILE_PATH = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-dumpfile-prefix")) {
      if (i == argList.size()) {
        throw CommandLineError("-dumpfile-prefix <prefix for output files>");
      }
      rigel::profiler::DUMPFILE_PREFIX = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-heartbeat_print_pcs")) {
      if (i == argList.size()) {
        throw CommandLineError("--heartbeat_print_pcs <N>");
      }
      this->cmdline_table["heartbeat_print_pcs"] = "true";
      this->cmdline_table["HEARTBEAT_INTERVAL"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-d")) {
      if (i == argList.size()) {
        throw CommandLineError("-d <debug filename>");
      }
      this->cmdline_table["debug_file"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-overwrite-dump-directory")) {
      this->cmdline_table["OVERWRITE_DUMP_DIRECTORY"] = "true";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-m")) {
      if (i == argList.size()) {
        throw CommandLineError("-m <mem dump filename>");
      }
      this->cmdline_table["memdump"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    // The number of sets and ways to have in the directory at each global cache
    // bank.
    if (0 == key.compare("--directory-sets")) {
      if (i == argList.size()) {
        throw CommandLineError("--directory-sets <N>");
      }
      rigel::ENABLE_EXPERIMENTAL_DIRECTORY = true;
      rigel::COHERENCE_DIR_SETS = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--directory-ways")) {
      if (i == argList.size()) {
        throw CommandLineError("--directory-ways <N>");
      }
      rigel::ENABLE_EXPERIMENTAL_DIRECTORY = true;
      rigel::COHERENCE_DIR_WAYS = atoi(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--directory-replacement")) {
      if (i == argList.size()) {
        throw CommandLineError("--directory-replacement <lru|lfu|lu|lra|ls>");
      }
			std::string argList_i = std::string(argList[i++]);
      if (argList_i.compare("lru") == 0) { rigel::DIRECTORY_REPLACEMENT_POLICY = rigel::dir_rep_lru; }
      else if (argList_i.compare("lfu") == 0) { rigel::DIRECTORY_REPLACEMENT_POLICY = rigel::dir_rep_lfu; }
      else if (argList_i.compare("lu") == 0) { rigel::DIRECTORY_REPLACEMENT_POLICY = rigel::dir_rep_lu; }
      else if (argList_i.compare("lra") == 0) { rigel::DIRECTORY_REPLACEMENT_POLICY = rigel::dir_rep_lra; }
      else if (argList_i.compare("ls") == 0) { rigel::DIRECTORY_REPLACEMENT_POLICY = rigel::dir_rep_ls; }
      else {
        throw CommandLineError("--directory-replacement <lru|lfu|lu|lra|ls>");
      }
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--gcache-ports-per-bank")) {
      if (i == argList.size()) {
        throw CommandLineError("--gcache-ports-per-bank <N>");
      }
      rigel::cache::GCACHE_REQUEST_PORTS_PER_BANK = atoi(argList[i++]);
      rigel::cache::GCACHE_REPLY_PORTS_PER_BANK =
        rigel::cache::GCACHE_REQUEST_PORTS_PER_BANK;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--sigint-stats")) {
      rigel::SIGINT_STATS = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--sleepy-cores")) {
      rigel::SLEEPY_CORES = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--start-awake-not-asleep")) {
      // this value may not be needed, as really I just want to override the value for SPRF_LOCK_CORES...
      rigel::START_AWAKE_NOT_ASLEEP = true; 
      printf("NOTE: Starting up with SIM_SLEEP [OFF]! All cores/threads AWAKE!\n");
      rigel::SPRF_LOCK_CORES = false;
      continue;
    } else {
      printf("NOTE: Starting up with SIM_SLEEP [ON]! All cores but 0 are ASLEEP!\n");
    }
		/*XXX*/
		if (0 == key.compare("-st")) {
			rigel::SINGLE_THREADED_MODE = true;
			printf("NOTE: Single-threaded mode ON\n");
			continue;
		}
    /////// XXX: SIMULATION OTHER PARAMS :XXX ///////
    if (0 == key.compare("-tqlat_enqueue_one")) {
      if (i == argList.size()) {
        throw CommandLineError("-tqlat_enqueue_one <cycle latency>");
      }
      this->cmdline_table["LATENCY_ENQUEUE_ONE"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-tqlat_enqueue_loop")) {
      if (i == argList.size()) {
        throw CommandLineError("-tqlat_enqueue_loop <cycle latency>");
      }
      this->cmdline_table["LATENCY_ENQUEUE_LOOP"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-tqlat_dequeue")) {
      if (i == argList.size()) {
        throw CommandLineError("-tqlat_dequeue <cycle latency>");
      }
      this->cmdline_table["LATENCY_DEQUEUE"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-tqlat_block_sync")) {
      if (i == argList.size()) {
        throw CommandLineError("-tqlat_block_sync <cycle latency>");
      }
      this->cmdline_table["LATENCY_BLOCKED_SYNC"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-tq_type")) {
      if (i == argList.size()) {
        throw CommandLineError(tq_type_string);
      }
      char str_type[32] = "none";
			std::string argList_i = std::string(argList[i++]);

      if (argList_i.compare("default") == 0) sprintf(str_type, "%d", TQ_SYSTEM_DEFAULT);
      if (argList_i.compare("baseline") == 0) sprintf(str_type, "%d", TQ_SYSTEM_BASELINE);
      if (argList_i.compare("interval") == 0) sprintf(str_type, "%d", TQ_SYSTEM_INTERVAL);
      if (argList_i.compare("int_lifo") == 0) sprintf(str_type, "%d", TQ_SYSTEM_INT_LIFO);
      if (argList_i.compare("recursive") == 0) sprintf(str_type, "%d", TQ_SYSTEM_RECURSIVE);
      if (argList_i.compare("recent") == 0) sprintf(str_type, "%d", TQ_SYSTEM_RECENT);
      if (argList_i.compare("recent_v1") == 0) sprintf(str_type, "%d", TQ_SYSTEM_RECENT_V1);
      if (argList_i.compare("recent_v2") == 0) sprintf(str_type, "%d", TQ_SYSTEM_RECENT_V2);
      if (argList_i.compare("recent_v3") == 0) sprintf(str_type, "%d", TQ_SYSTEM_RECENT_V3);
      if (argList_i.compare("recent_v4") == 0) sprintf(str_type, "%d", TQ_SYSTEM_RECENT_V4);
      if (argList_i.compare("near_range") == 0) sprintf(str_type, "%d", TQ_SYSTEM_NEAR_RANGE);
      if (argList_i.compare("near_all") == 0) sprintf(str_type, "%d", TQ_SYSTEM_NEAR_ALL);
      if (argList_i.compare("rand") == 0) sprintf(str_type, "%d", TQ_SYSTEM_RAND);
      if (argList_i.compare("type_range") == 0) sprintf(str_type, "%d", TQ_SYSTEM_TYPE_RANGE);

      if (strncmp("none", str_type, 4) == 0)
        throw CommandLineError(tq_type_string);

      std::cerr << "Using TQ_TYPE: " << str_type << "\n";
      this->cmdline_table["TQ_SYSTEM_TYPE"] = str_type;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--dram-batch-perbank")) {
      if(i == argList.size())
      {
        throw CommandLineError("--dram-batch-perbank <MAX REQUESTS PER BATCH PER BANK>");
      }
      this->cmdline_table["DRAM_BATCHING_POLICY"] = std::string("batch_perbank");
      this->cmdline_table["DRAM_BATCHING_CAP"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--dram-batch-perchannel")) {
      if(i == argList.size())
      {
        throw CommandLineError("--dram-batch-perchannel <MAX REQUESTS PER BATCH PER CHANNEL>");
      }
      this->cmdline_table["DRAM_BATCHING_POLICY"] = std::string("batch_perchannel");
      this->cmdline_table["DRAM_BATCHING_CAP"] = std::string(argList[i++]);
      continue;
    }
    //////// XXX: Enable idealized DRAM model XXX ////////
    if (0 == key.compare("--ideal-dram")) {
      rigel::CMDLINE_ENABLE_IDEALIZED_DRAM = true;
      continue;
    }
    //////// XXX: Change default latency for ideal DRAM XXX /////////
    if (0 == key.compare("--ideal-dram-lat")) {
      /* Instructions to execute */
      if (i == argList.size()) {
        throw CommandLineError("--ideal-dram-lat <N>");
      }
      rigel::CMDLINE_IDEALIZED_DRAM_LATENCY = atoi(std::string(argList[i++]).c_str());
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--track-bit-patterns")) {
      rigel::TRACK_BIT_PATTERNS = true;
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--pipeline-bypass")) {
      if(i == argList.size())
      {
        throw CommandLineError("--pipeline-bypass <full|none>");
      }
			std::string argList_i = std::string(argList[i++]);
      
      if (argList_i.compare("full") == 0) rigel::PIPELINE_BYPASS_PATHS = rigel::pipeline_bypass_full;
      else if (argList_i.compare("none") == 0) rigel::PIPELINE_BYPASS_PATHS = rigel::pipeline_bypass_none;
      else assert(0);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--stdin-string")) {
      if(i == argList.size())
      {
        throw CommandLineError("--stdin-string <\"STRING\">");
      }
      if(rigel::REDIRECT_HOST_STDIN) {
        throw CommandLineError("Cannot source target stdin from file (-stdin) and string"
          "(--stdin-string) at the same time");
      }
      rigel::STDIN_STRING_ENABLE = true;
      rigel::STDIN_STRING = argList[i++];
      std::cerr << "Piping \"" << rigel::STDIN_STRING << "\" into stdin\n";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-hstdin")) {
      if(i == argList.size())
      {
        throw CommandLineError("-hstdin <FILE TO ATTACH TO HOST STDIN>");
      }
      if(rigel::STDIN_STRING_ENABLE) {
        throw CommandLineError("Cannot source target stdin from file (-stdin) and string"
          "(--stdin-string) at the same time");
      }
			std::string stdin_file(argList[i++]);
      rigel::REDIRECT_HOST_STDIN = true;
      this->cmdline_table["host_stdin_file"] = stdin_file;
      std::cerr << "Redirecting host stdin from " << stdin_file << "\n";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-hstdout")) {
      if(i == argList.size())
      {
        throw CommandLineError("-hstdout <FILE TO ATTACH TO HOST STDOUT>");
      }
			std::string stdout_file(argList[i++]);
      rigel::REDIRECT_HOST_STDOUT = true;
      this->cmdline_table["host_stdout_file"] = stdout_file;
      std::cerr << "Redirecting host stdout to " << stdout_file << "\n";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-hstderr")) {
      if(i == argList.size())
      {
        throw CommandLineError("-hstderr <FILE TO ATTACH TO HOST STDERR>");
      }
			std::string stderr_file(argList[i++]);
      rigel::REDIRECT_HOST_STDERR = true;
      this->cmdline_table["host_stderr_file"] = stderr_file;
      std::cerr << "Redirecting host stderr to " << stderr_file << "\n";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-tstdout")) {
      if(i == argList.size())
      {
        throw CommandLineError("-tstdout <FILE TO ATTACH TO TARGET STDOUT>");
      }
      rigel::TARGET_STDOUT_FILENAME = argList[i++];
      rigel::REDIRECT_TARGET_STDOUT = true;
      std::cerr << "Redirecting target stdout to " << rigel::TARGET_STDOUT_FILENAME << "\n";
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-tstderr")) {
      if(i == argList.size())
      {
        throw CommandLineError("-tstderr <FILE TO ATTACH TO TARGET STDERR>");
      }
      rigel::TARGET_STDERR_FILENAME = argList[i++];
      rigel::REDIRECT_TARGET_STDERR = true;
      std::cerr << "Redirecting target stderr to " << rigel::TARGET_STDERR_FILENAME << "\n";
      continue;
    }
		/*XXX*/
		if (0 == key.compare("--hostrng-seed")) {
			if(i == argList.size())
			{
				throw CommandLineError("--hostrng-seed <RNG SEED FOR HOST>");
			}
			rigel::RNG.Reseed(atoi(argList[i++]));
			continue;
		}
		/*XXX*/
		if (0 == key.compare("--targetrng-seed")) {
      if(i == argList.size())
      {
        throw CommandLineError("--targetrng-seed <RNG SEED FOR TARGET>");
      }
      rigel::TargetRNG.Reseed(atoi(argList[i++]));
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-pbo")) {
      if (i == argList.size()) {
        throw CommandLineError("-pbo <protobuf output filename>");
      }
      this->cmdline_table["protobuf_out_filename"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("-pbi")) {
      if (i == argList.size()) {
        throw CommandLineError("-pbi <protobuf input filename>");
      }
      this->cmdline_table["protobuf_in_filename"] = std::string(argList[i++]);
      continue;
    }
    /*XXX*/
    if (0 == key.compare("--help") || 0 == key.compare("-h")) {
      i++;
      print_help();
      ExitSim(0);
    }
    /*XXX*/
    if (i == argList.size()) {
      /* Last arg, always object file */
      this->cmdline_table["objfile"] = key;
      rigel::BENCHMARK_BINARY_PATH = key;
      rigel::TARGET_ARGS.insert(rigel::TARGET_ARGS.begin(), key); //argv[0]
      continue;
    }
    /*XXX*/
    /* Did not find the key? */
		std::string err = "Unrecognized token: " + key;
    throw CommandLineError(err);
  }

  //Free dynamically-allocated strings, if they were read from a file.
  if(fromArgFile)
    for(std::vector<char *>::iterator it = argList.begin(); it != argList.end(); ++it)
      free(*it);

#ifndef _MSC_VER
  // Make dumpfile directory.
  if (mkdir(rigel::DUMPFILE_PATH.c_str(), S_IRWXU | S_IRGRP | S_IWGRP)) {
    // If it already exists, we will end up overwriting any names that
    // alias inside of that directory.
    if (EEXIST != errno) {
      perror("mkdir(DUMPFILE_PATH)");
      abort();
    } else {
      // Hack to allow default path to be continually overwritten.
   /*   if (!(get_val_bool((char *)"OVERWRITE_DUMP_DIRECTORY")) && (std::string("RUN_OUTPUT") != rigel::DUMPFILE_PATH.c_str())) {
        fprintf(stderr, "ERROR: Unable to create dumpfile-path ('%s').  Directory already exists.",
          rigel::DUMPFILE_PATH.c_str());
        abort();
      }*/
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
// CommandLineArgs::print_help()
////////////////////////////////////////////////////////////////////////////////
// prints a "help" block of text at the commandline that explains usage and
// runtime switches/options
void CommandLineArgs::print_help() {
  std::cout.setf(std::ios::left);
  std::cout << "Usage: " << CommandLineArgs::exec_name << " <options> <rigel ELF file> " << "\n";
  std::cout << std::setw(40) << "  -h, --help" << "\n" <<  "      " << "Print help message." << "\n";
  std::cout << std::setw(40) << "  -argfile <ARGFILE>" << "\n" << "      "
    << "Read in arguments from ARGFILE instead of argv.  "
    << "-argfile must be passed as first argument." << "\n";
  std::cout << std::setw(40) << "  -i" << "\n" <<  "      " << "Enable interactive mode." << "\n";
  std::cout << std::setw(40) << "  -m <filename>" << "\n" <<  "      "
    << "Dump memory after run to 'filename'." << "\n";
  std::cout << std::setw(40) << "  -d <filename>" << "\n" <<  "      "
    << "Write full debug output to 'filename'." << "\n";
  std::cout << std::setw(40) << "  -n <N>" << "\n" <<  "      "
    << "Execute N instructions before exiting" << "\n";
  std::cout << std::setw(40) << "  --args <NARGS> ARG1 ARG2 .. ARGN" << "\n" << "      "
    << "Pass NARGS arguments to the target binary via argc/argv."
    << "argv[0] will always be the benchmark binary name.  --args can be passed "
    << "multiple times, and the target arguments given will be concatenated." << "\n";
  std::cout << std::setw(40) << "  -ndb" << "\n" << "      " << "Even if CouchDB support is enabled,"
    << "do not dump this run to the database.  Use this for e.g. 'make test' runs." << "\n";
  std::cout << std::setw(40) << "  -threads<N> " << "\n" <<  "      "
    << "sets the number of threads per core to N" << "\n";
  std::cout << std::setw(40) << "  -c, -clusters <N> " << "\n" <<  "      "
    << "sets the number of clusters per tile to N" << "\n";
  std::cout << std::setw(40) << "  -t, -tiles <N> " << "\n" <<  "      "
    << "sets the number of tiles simulated to N" << "\n";
  std::cout << std::setw(40) << "  -memchannels <N> " << "\n" <<  "      "
    << "sets the number of memory controllers/channels to N (must be >= 1)"
    << "Default: 1 per tile" << "\n";
  std::cout << std::setw(40) << "  -heartbeat_print_pcs <N>" << "\n" <<  "      "
    << "print the pcs of all cores at heartbeat=N intervals (for debugging deadlocks)" << "\n";
  std::cout << std::setw(40) << "  -(no-)mem-sched-row" << "\n" <<  "      "
    << "Used open-row scheduling for memory accesses. On by default." << "\n";
  std::cout << std::setw(40) << "  -mem-sched-pend-count <N>" << "\n" <<  "      "
    << "Size of the per-bank memory scheduling window." << "\n";
  std::cout << std::setw(40) << "  -mem-sched-policy <policy>" << "\n" <<  "      "
    << "Set memory controller scheduling policy (options: single, perbank)"
    << "Default: perbank (one pending request per DRAM bank)" << "\n";
  std::cout << std::setw(40) << "  -mem-monolithic-scheduler" << "\n" << "      "
    << "Memory controller has one monolithic buffer of -mem-sched-pend-count requests"
    << "Default: off (per-DRAM-bank buffers)" << "\n";
  std::cout << std::setw(40) << "  --row-cache <N>" << "\n" << "      "
    << "Size of the per-DRAM-bank row cache (WCDRAM).  "
    << "Default: 1 (no row cache, traditional DRAM interface)" << "\n";
  std::cout << std::setw(40) << "  --row-cache-replacement <lru|tristage|mru>" << "\n" << "      "
    << "Replacement policy for row cache (if enabled)  "
    << "Default: tristage (LRU clean row, then open dirty row, then LRU dirty row)" << "\n";
  std::cout << std::setw(40) << "  -no-stats-dump" << "\n" <<  "      "
    << "Inhibit printing of global statistics at exit" << "\n";
  std::cout << std::setw(40) << "  -regfile-init" << "\n" <<  "      "
    << "Initialize all register files to the contents of a file \"rf.in\"" << "\n";
  std::cout << std::setw(40) << "  -regfile-dump" << "\n" <<  "      "
    << "Print the contents of each register file at exit" << "\n";
  std::cout << std::setw(40) << "  -regfile-trace-dump" << "\n" <<  "      "
    << "Dump the contents of each register file write into a file named rf.<coreid>.<tid>.trace" << "\n";
  std::cout << std::setw(40) << "  -dump-elf-image" << "\n" <<  "      "
    << "Dump dump the ELF image in <ADDR,DATA> pairs to a elfimage.hex" << "\n";
  std::cout << std::setw(40) << "  -load-checkpoint" << "\n" <<  "      "
    << "Load a Checkpoint (ALPHA status)" << "\n";
  std::cout << std::setw(40) << "  -memprof <bin size in cycles>" << "\n" <<  "      "
    << "Generate per-memory operation histogram of actions at the global caches." << "\n";
  std::cout << std::setw(40) << "  -dumpfile-path <path>" << "\n" <<  "      "
    << "Set the path to which to output dump files. Default: ./RUN_OUTPUT" << "\n";
  std::cout << std::setw(40) << "  -overwrite-dump-directory" << "\n" <<  "      "
    << "Continue simulation (overwriting previous files) if non-default -dumpfile-path exists" << "\n";
  std::cout << std::setw(40) << "  -(no-)network-core-zero-prio" << "\n" <<  "      "
    << "Give priority to messages in network from core zero, cluster zero.  "
    << "Off by default." << "\n";
  std::cout << std::setw(40) << "  -per-pc-stats" << "\n" <<  "      "
    << "Enable dumping of per-pc data. Default: "
    << rigel::profiler::CMDLINE_DUMP_PER_PC_STATS
    << " File: " << rigel::profiler::per_pc_filename << "\n";
  std::cout << std::setw(40) << "  -dumpfile-prefix" << "\n" <<  "      "
    << "Prefix value to put on output files. Default: "
    << rigel::profiler::DUMPFILE_PREFIX << "\n";
  std::cout << std::setw(40) << "  -perfect-[l1i|l1d|l2i|l2d|global-cache|global-ops]" << "\n" <<  "      "
    << "Enable perfect caching for limits studies. "
    << "and these commands do not currently effect global operations/atomics. "
    << "Default: Disabled."  << "\n";
  std::cout << std::setw(40) << "  -free-writes" << "\n" <<  "      "
    << "Writes to the cluster cache cost nothing.  Similar to a highly idealized write buffer."
    << "Default: Disabled."  << "\n";
  std::cout << std::setw(40) << "  -model-contention-off" << "\n" <<  "      "
    << "Model contention for the cluster-level instruction and data caches."
    << "Default: Contention on."  << "\n";
  std::cout << std::setw(40) << "  --gcache-access-delay <cycles of delay GCache access>" << "\n" <<  "      "
    << "Global cache access latency."
    << "Default: " << rigel::cache::GCACHE_ACCESS_LATENCY << "\n";
  std::cout << std::setw(40) << "  -l2d-ports-per-bank <num ports>" << "\n" <<  "      "
    << "Number of ports into the cluster cache."
    << "Default: " << rigel::cache::L2D_PORTS_PER_BANK << "\n";
  std::cout << std::setw(40) << "  -no-rigelprint" << "\n" <<  "      "
    << "Suppress output from RigelPrint()/printreg actions."
    << "Default: " << rigel::CMDLINE_SUPPRESS_RIGELPRINT << "\n";
  std::cout << std::setw(40) << "  -short-rigelprint" << "\n" <<  "      "
    << "Short form RigelPrint()/printreg output."
    << "Default: " << rigel::CMDLINE_SHORT_RIGELPRINT << "\n";
  std::cout << std::setw(40) << "  -non-blocking-pref" << "\n" <<  "      "
    << "Do not block prefetches that cannot allocat MSHRs.  Drop instead."
    << "Default: " << rigel::cache::CMDLINE_NONBLOCKING_PREFETCH << "\n";
  std::cout << std::setw(40) << "  -gcache-prefetch <N>" << "\n" <<  "      "
    << "Prefetch the next N lines into the global cache when a miss is found."
    << "Default: " << rigel::cache::CMDLINE_GCACHE_PREFETCH_SIZE << "\n";
  std::cout << std::setw(40) << "  -no-gcache-bulk-prefetch" << "\n" << "      "
    << "Do not pend all G$ prefetches as one MSHR/DRAM request, truncated to DRAM row."
    << "Default: " << rigel::cache::GCACHE_BULK_PREFETCH << "\n";
  std::cout << std::setw(40) << "  -cc-prefetch <N>" << "\n" <<  "      "
    << "Prefetch the next N lines into the cluster cache when a miss is found."
    << "Default: " << rigel::cache::CMDLINE_CCPREFETCH_SIZE << "\n";
  std::cout << std::setw(40) << "  -dump-sharing-stats" << "\n" <<  "      "
    << "Track and dump sharing stats for reads and writes between tasks."
    << "Default: Off (!!EXPENSIVE!!)" << "\n";
  std::cout << std::setw(40) << "  --stack-smash-check" << "\n" <<  "      "
    << "Turn on stack smash checking.  If a core reads or writes to another core's stack, "
    << "abort the simulation."
    << "Default: " << rigel::CMDLINE_CHECK_STACK_ACCESSES << "\n";
  std::cout << std::setw(40) << "  --memory-alias-test" << "\n" <<  "      "
    << "Regression test to make sure target addresses do not alias in the memory model."
    << "Default: Off (!!EXPENSIVE!!)" << "\n";
  std::cout << std::setw(40) << "  -dramtrace" << "\n" <<  "      "
    << "Dump all DRAM accesses to file"
    << "Default: Off" << "\n";
  std::cout << std::setw(40) << "  -coherencetrace" << "\n" <<  "      "
    << "Dump all directory-side coherence actions to file"
    << "Default: Off" << "\n";
  std::cout << std::setw(40) << "  -direvicttrace" << "\n" << "      "
    << "Dump all directory cache evictions to file"
    << "Default: Off" << "\n";
  std::cout << std::setw(40) << "  -verbose-dram-stats" << "\n" <<  "      "
    << "Dump verbose (per-bank) DRAM stats at end of run"
    << "Default: Only aggregate DRAM stats" << "\n";
  std::cout << std::setw(40) << "  -tqlat_enqueue_[one|loop] <N> " << "\n" <<  "      "
    << "sets the latency for enqueing one or a loop of tasks to  N" << "\n";
  std::cout << std::setw(40) << "  -tqlat_dequeue <N> " << "\n" <<  "      "
    << "sets the latency for dequeing one task to  N" << "\n";
  std::cout << std::setw(40) << "  -tqlat_block_sync <N> " << "\n" <<  "      "
    << "sets the latency for a global sync on TQ empty to N" << "\n";
  std::cout << std::setw(40) << "  " << tq_type_string << "\n" <<  "      "
    << "sets the type of task queue used.  See task_queue.h for details." << "\n";
  std::cout << std::setw(40) << "  --(no-)overflow-directory" << "\n" <<  "      "
    << "Turn (off) on overflow directory for coherence that is backed my target memory "
    << "instead of invalidations."
    << "Default: " << rigel::ENABLE_OVERFLOW_DIRECTORY << "\n";
  std::cout << std::setw(40) << "  --(no-)overflow-timing" << "\n" <<  "      "
    << "Turn (off) on overflow timing modeling."
    << "Default: " << rigel::ENABLE_OVERFLOW_MODEL_TIMING << "\n";
  std::cout << std::setw(40) << "  --(no-)coherence" << "\n" <<  "      "
    << "Turn (off) on coherence."
    << "Default: " << rigel::ENABLE_EXPERIMENTAL_DIRECTORY << "\n";
  std::cout << std::setw(40) << "  --gnet2cc-lat <N> " << "\n" <<  "      "
    << "Latency between GNET and Cluster Cache in the Tile Network."
    << "Default: " << std::dec << rigel::TILENET_GNET2CC_LAT_L1 << "\n";
  std::cout << std::setw(40) << "  --cc2gnet-lat <N> " << "\n" <<  "      "
    << "Latency between Cluster Cache and GNET in the Tile Network."
    << "Default: " << std::dec << rigel::TILENET_CC2GNET_LAT_L1 << "\n";
  std::cout << std::setw(40) << "  --btb-entries <N> " << "\n" <<  "      "
    << "Number of BTB entries."
    << "Default: " << std::dec << rigel::NUM_BTB_ENTRIES << "\n";
  std::cout << std::setw(40) << "  --directory-sets <N> " << "\n" <<  "      "
    << "Number of sets in the directory (per GCache bank)."
    << "Default: " << std::dec << rigel::COHERENCE_DIR_SETS << "\n";
  std::cout << std::setw(40) << "  --directory-ways <N> " << "\n" <<  "      "
    << "Number of ways in the directory (per GCache bank)."
    << "Default: " << std::dec << rigel::COHERENCE_DIR_WAYS << "\n";
  std::cout << std::setw(40) << " --directory-replacement <lru|lfu|lu|lra|ls> " << "\n" << "      "
    << "Sparse directory replacement policy.  lru = Least Recently Touched, "
    "lfu = Least Frequently Touched (since allocated), lu = Least Touched (since allocated), "
    "lra = Least Recently Allocated, ls = Least Shared"
    << "Default: lru" << "\n"; //FIXME: Should have an array of string names so it will still
                               //print the correct default if we change it.
  std::cout << std::setw(40) << "  --gcache-ports-per-bank <N> " << "\n" <<  "      "
    << "Number of request/replies serviced per cycle (per GCache bank)."
    << "Default: " << std::dec << rigel::cache::GCACHE_REQUEST_PORTS_PER_BANK_DEFAULT << "\n";
  std::cout << std::setw(40) << "  --sigint-stats " << "\n" <<  "      "
    << "Dump all profile information when SIGINT (Ctrl+C) is caught."
    << "Default: off" << "\n";
  std::cout << std::setw(40) << "  --sleepy-cores " << "\n" <<  "      "
    << "Turn cores off when they are stalling on a C$ MSHR "
    << "(will affect timing and yield inaccurate core-level statistics) "
    << "Default: off" << "\n";
  std::cout << std::setw(40) << "  --start-awake-not-asleep " << "\n" <<  "      "
    << "Bring up simulator with all cores awake, NOT in SIM_SLEEP mode initially "
    << "has implications for MT: a core should not be put into SIM_SLEEP with active instructions in the pipeline... "
    << "Default: off" << "\n";
	std::cout << std::setw(40) << "  -st " << "\n" << "      "
		<< "Halt all threads except 0.  "
		<< "Default: off" << "\n";
  std::cout << std::setw(40) << "  --limited-directory " << "\n" <<  "      "
    << "Use a limited directory sceme with fewer pointers than a full directory." << "\n";
  std::cout << std::setw(40) << "  --directory-max-ptrs <N>" << "\n" <<  "      "
    << "Use <N> pointers for the limited directory (if enabled) "
    << "Default: " << std::dec << rigel::cache::DEFAULT_MAX_LIMITED_PTRS << "\n";
  std::cout << std::setw(40) << "  --disable-wr2wr-noalloc" << "\n" <<  "      "
    << "Disable write to write transfers to not allocate at the global cache in coherent RigelSim. "
    << "Default: (enabled)" << "\n";
  std::cout << std::setw(40) << "  --perfect-alus" << "\n" <<  "      "
    << "Enable single-cycle execution for all ALU operations."
    << "Default: (disabled)" << "\n";
  std::cout << std::setw(40) << "  -nbl" << "\n" << "      "
    << "Enable nonblocking loads and stores in the core" << "\n" << "      "
    << "Default: (" << rigel::DEFAULT_NONBLOCKING_MEM << ")" << "\n";
  std::cout << std::setw(40) << "  --enable-hybrid-coherence" << "\n" <<  "      "
    << "Enable mixed software-hardware coherence and incoherent malloc. "
    << "Default: (disabled)" << "\n";
  std::cout << std::setw(40) << "  --probe-filter-directory" << "\n" <<  "      "
    << "Turn the directory into a probe filter. "
    << "Default: (disabled)" << "\n";
  std::cout << std::setw(40) << "  --bcast-network" << "\n" <<  "      "
    << "Turn the broadcast network for probe filtering filter. "
    << "Default: (disabled)" << "\n";
  std::cout << std::setw(40) << "  --max-bcasts <N>" << "\n" <<  "      "
    << "Set the number of broadcasts to allow outstanding at a time.  0 is infinite."
    << "Default: (disabled)" << "\n";

  //std::cout << std::setw(40) << "  --enable-incoherent-malloc" << "\n" <<  "      "
  //  << "Enable malloc that is not tracked by coherence. "
  //  << "Default: (disabled)" << "\n";
  std::cout << std::setw(40) << "  --enable-gcache-mru" << "\n" <<  "      "
    << "Enable MRU replacement policy at the global cache. "
    << "Default: (disabled)" << "\n";
  std::cout << std::setw(40) << "  -lt" << "\n" << "      "
    << "Enable DRAM locality tracking throughout the simulator. "
    << "Default: (disabled)" << "\n";
  std::cout << std::setw(40) << "  --dram-batch-perbank <N>" << "\n" << "      "
    << "Batch DRAM requests w/ max of N requests per bank. "
    << "Default: No batching" << "\n";
  std::cout << std::setw(40) << "  --dram-batch-perchannel <N>" << "\n" << "      "
    << "Batch DRAM requests w/ max of N requests per channel. "
    << "Default: No batching" << "\n";
  std::cout << std::setw(40) << "  --ideal-dram" << "\n" << "      "
    << "Enable ideal DRAM (fixed BW/latency) "
    << "Default: Off (real model)" << "\n";
  std::cout << std::setw(40) << "  --ideal-dram-lat <N>" << "\n" << "      "
    << "All DRAM requests take N DRAMClk cycles. "
    << "Default: Zero cycles, but ideal DRAM is turned off unless --ideal-dram is passed" << "\n";  
  std::cout << std::setw(40) << "  --track-bit-patterns" << "\n" << "      "
    << "Track the bit patterns of values written into RFs and caches to "
    << "evaluate e.g. dynamic zero compression "
    << "Default: Off" << "\n";
    std::cout << std::setw(40) << "  --pipeline-bypass <full|none>" << "\n" << "      "
    << "Specify the set of bypass paths available in all core pipelines.  "
    << "Options are full bypassing ('full') and no bypassing ('none') "
    << "Default: full" << "\n";
  std::cout << std::setw(40) << "  --stdin-string <\"STRING\">" << "\n" << "      "
    << "Pipe STRING into target stdin" << "\n";
  std::cout << std::setw(40) << "  -hstdin <FILE>" << "\n" << "      "
    << "Connect FILE to host stdin" << "\n";
  std::cout << std::setw(40) << "  -hstdout <FILE>" << "\n" << "      "
    << "Redirect host stdout to FILE" << "\n";
  std::cout << std::setw(40) << "  -hstderr <FILE>" << "\n" << "      "
    << "Redirect host stderr to FILE" << "\n";
  std::cout << std::setw(40) << "  -tstdout <FILE>" << "\n" << "      "
    << "Redirect target stdout to FILE" << "\n";
  std::cout << std::setw(40) << "  -tstderr <FILE>" << "\n" << "      "
    << "Redirect target stderr to FILE" << "\n";
  std::cout << std::setw(40) << "  --tags TAG1,TAG2,TAG3,..." << "\n" << "      "
    << "Tag this run with a comma-delimited list of identifying strings."
    << "These strings can be used by grep or CouchDB views after the fact"
    << "to identify groups of runs with common high-level characteristics."
    << "  Tags given in this manner will be stored in the CouchDB record with "
    << "the TAGS key." << "\n";
  std::cout << std::setw(40) << "  -db <HOSTNAME> <PORT> <DB NAME>" << "\n" << "      "
    << "Modify CouchDB server, port, or db name from default." << "\n" << "      "
    << "Example: -db http://corezilla.crhc.illinois.edu 5984 rigelsim-test" << "\n" << "       "
    << "Default: " << rigel::COUCHDB_HOSTNAME << " " << rigel::COUCHDB_PORT
    << " " << rigel::COUCHDB_DBNAME << "\n";
	std::cout << std::setw(40) << "  --hostrng-seed <SEED FOR HOST RNG>" << "\n" << "      "
		<< "Seed the host random number generator with a number." << "\n" << "      "
		<< "Default: 0\n";
  std::cout << std::setw(40) << "  --targetrng-seed <SEED FOR TARGET RNG>" << "\n" << "      "
    << "Seed the target random number generator with a number." << "\n" << "       "
    << "Default: 0\n";
}

