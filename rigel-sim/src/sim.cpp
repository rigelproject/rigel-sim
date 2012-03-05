////////////////////////////////////////////////////////////////////////////////
// sim.cpp
////////////////////////////////////////////////////////////////////////////////
//
// the sim STARTS HERE!
// the main simulator file (with main())
// 'sim.cpp' Main starting point for the simulation.
//
////////////////////////////////////////////////////////////////////////////////

#include <cassert>                      // for assert
//execinfo.h is not found on all platforms
//it's used for backtraces.
//FIXME do a more robust check in configure.ac
#ifdef __linux__
#include <execinfo.h>                   // for backtrace, etc
#endif
#include <inttypes.h>                   // for PRIu64, PRIx64
#include <cmath>                        // for ceil
#include <signal.h>                     // for signal, SIGINT, SIG_DFL
#include <cstddef>                      // for NULL, size_t
#include <stdint.h>                     // for uint64_t
#include <cstdio>                       // for fprintf, stderr, fopen, etc
#include <cstdlib>                      // for exit, srand
#include <cstring>                      // for strncpy
#include <time.h>                       // for time
#include <fstream>                      // for operator<<, basic_ostream, etc
#include <iomanip>                      // for operator<<, setfill, setw, etc
#include <iostream>                     // for cerr, cout, dec, hex
#include <string>                       // for string, char_traits, etc
#include "../user.config"   // for ENABLE_VERIF
#include "RandomLib/Random.hpp"  // for Random
#include "broadcast_manager.h"  // for BroadcastManager
#include "caches/global_cache.h"  // for GlobalCache, etc
#include "caches/hybrid_directory.h"  // for HybridDirectory, etc
//#include "core.h"           // for CoreInOrderLegacy
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for DEBUG_HEADER, etc
#include "memory/dram.h"           // for CONTROLLERS, RANKS, ROWS, etc
#include "util/event_track.h"    // for EventTrack, global_tracker
#include "global_network.h"  // for GlobalNetworkNew, etc
#include "instr.h"          // for InstrLegacy, etc
#include "profile/profile.h"        // for ProfileStat, Profile, etc
#include "profile/profile_names.h"  // for ::STATNAME_COMMIT_FU_ALU, etc
#include "sim.h"            // for ClusterType, NUM_TILES, etc
#include "util/task_queue.h"     // for TaskSystemBaseline
#include "tile.h"           // for Tile, etc
#include "util/util.h"           // for CommandLineArgs, ExitSim
#include "util/value_tracker.h"  // for ZeroTracker
#include "memory_timing.h"       // for MemoryTimingType definition
#include <google/protobuf/stubs/common.h> // for GOOGLE_PROTOBUF_VERIFY_VERSION macro
#include "rigelsim.h"
////////////////////////////////////////////////////////////////////////////////

InstrLegacy TempInstr(0xF000000F,           // PC
                    rigel::POISON_TID,  // tid
                    0xF000000F,           // pred_pc
                    0x00000000,           // raw_instr
                    I_NULL                // i_type
                   );
InstrLegacy * rigel::NullInstr;
rigel::GlobalBackingStoreType * rigel::GLOBAL_BACKING_STORE_PTR;
namespace rigel {
	RandomLib::Random RNG(0);
	RandomLib::Random TargetRNG(0); //For generating random numbers for
	                                //the target (via syscalls)
}

////////////////////////////////////////////////////////////////////////////////
// Helper functions for main()
////////////////////////////////////////////////////////////////////////////////
// these are all implemented in this file to keep the main() function concise
// and readable
static void helper_setup_other(CommandLineArgs &cmdline, char * argv[]);
static void helper_setup_fileio(CommandLineArgs &cmdline);
static void helper_init_rng();
static void helper_init_profiler(TileBase **tiles);
static void helper_close_files();

void sigint_handler(int sig);
void sigsegv_handler(int sig);
////////////////////////////////////////////////////////////////////////////////

void timestamp_greeting() {
#ifndef _WIN32
  // Timestamp greeting! for version tracking and debugging
  fprintf(stdout,"RigelSim Compiled by %s on %s at %s from Git commit:\n%s\n",
    BUILD_USER,__DATE__, __TIME__, GIT_REV); // values defined in Make-rules
#endif
}

////////////////////////////////////////////////////////////////////////////////
// main()
////////////////////////////////////////////////////////////////////////////////
//
// This is THE MAIN for the simulator.  The simulator starts HERE!
//
// RETURNS: Never since proper simulation ends with an ExitSim() call.
//
////////////////////////////////////////////////////////////////////////////////
// FIXME: OMFG this is too long!
int main(int argc, char * argv[])
{
  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  timestamp_greeting();  // dump rev# tracking, compile user and date/time

  // Make sure command line arguments are parsed first since they provide the
  // initial arguments for many of the other global structures (task queues,
  // memory model, etc.) 
  // Initialize the command line class that parsed the arguments passed to
  // RigelSim.  It also fills in many of the runtime constants that the
  // simulator uses.
  // TODO: use getopts package instead... 
  CommandLineArgs cmdline(argc, argv);

  // Initialize parameters
  // FIXME: move this or reorg, horribly messy inside
  helper_setup_other(cmdline, argv);

  RigelSim sim( &cmdline );      // instantiate main simulator

  // finished with sim setup, aux setup stuff follows


  // Setup the SIGINT handler which throws an ExitSim("SIGINT CAUGHT") instead of quitting
  // only if --sigint-stats has been passed
  if (rigel::SIGINT_STATS) { 
    (void) signal(SIGINT, sigint_handler); 
  }

  // Open the file that will get the CSV memory trace for the clusters
  helper_setup_fileio(cmdline);

  helper_init_rng(); // init random number generator
  GLOBAL_ZERO_TRACKER_PTR = new ZeroTracker;

  // initialize profiler objects
  helper_init_profiler( sim.chip()->tiles() );

  // SIMULATOR Components that should be handled elsewhere
  // Initialize the hybrid directory. FIXME: conditional
  hybrid_directory.init();

  // Needs to be initialized after we know how many cores we have (3096!)
  // FIXME: conditional
  GlobalTaskQueue = new TaskSystemBaseline;
  // end SIM Components which should be handled elsewhere

  // print out our object tree before we start
  sim.chip()->printHierarchy();

  ////////////////////////////////////////////////////////////////////////////// 
  // Start simulation proper
  ////////////////////////////////////////////////////////////////////////////// 
  bool caught_exitsim = false; // set to true when we catch
  try {
    for (rigel::CURR_CYCLE = 0; ; rigel::CURR_CYCLE++) {
      using namespace rigel;
      // TODO: interactive mode should be handled here
      if (RIGEL_CFG_NUM==2) {
        sim.do_shell_loop();
      }
      sim.clock_sim();
      //fprintf(stdout,"Cycle %llu \n",CURR_CYCLE);
      //if(CURR_CYCLE == 2000000) { rigel::GLOBAL_debug_sim.dump_all(); assert(0); }
      if (CURR_CYCLE == U64_MAX_VAL || CURR_CYCLE >= CYC_COUNT_MAX) {
         throw ExitSim("Early Exit");
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////// 
  // catch ExitSim
  ////////////////////////////////////////////////////////////////////////////// 
  catch (ExitSim e) {

    caught_exitsim = true; 

    rigel::SIM_END_TIME = time(NULL);

    // finish up sim for tiles and contained objects
    sim.EndSim();

    // One of the clusters need to call global_dump_profile(), doesn't matter which.
    // FIXME disabled unmodular, and also a bad idea this way
#if 1
    sim.chip()->tiles()[0]->getClusters()[0]->getProfiler()->global_dump_profile(argc, argv);
#endif

    delete [] InstrLegacy::LAST_INSTR_NUMBER;

		std::cerr << "EXIT REASON, " << e.reason << "\n";
  }
  //////////////////////////////////////////////////////////////////////////////
  // end catch ExitSim
  //////////////////////////////////////////////////////////////////////////////
  if (!caught_exitsim) {
    // NOTE: should never reach here, but handle if we don't catch an ExitSim
	  std::cerr << "!!! ERROR: EXITING SIMULATION without catching ExitSim at cycle ";
	  std::cerr << std::dec << rigel::CURR_CYCLE << " !!!" << std::endl;
  }

  helper_close_files(); // close any open files

  return 0;
}


//////////////////////////////////////////e/////////////////////////////////////
// close any open files
////////////////////////////////////////////////////////////////////////////////
static void helper_close_files() {
  if (rigel::GENERATE_CCTRACE) { 
    fclose(rigel::memory::cctrace_out);
  }
  if (rigel::GENERATE_DRAMTRACE) { 
    fclose(rigel::memory::dramtrace_out);
  }
  if (rigel::GENERATE_COHERENCETRACE) {
     fclose(rigel::memory::coherencetrace_out);
  }
  if (rigel::GENERATE_DIREVICTTRACE) {
    fclose(rigel::memory::direvicttrace_out);
  }
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// helper_init_rng()
////////////////////////////////////////////////////////////////////////////////
static void helper_init_rng() {
  srand(0); //We shouldn't use rand() or rand48(), but seed it just in case.
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// init_profiler
////////////////////////////////////////////////////////////////////////////////
static void helper_init_profiler(TileBase **tiles) {

  // Initialize the global profiler and use STDERR as the output stream.
  ProfileStat::init(stderr);

  //TODO could just use config and put this junk in profile_init.cpp
  using namespace rigel::profiler;
  stats[STATNAME_DIRECTORY_EVICTIONS].set_num_items(
    rigel::COHERENCE_DIR_SETS*rigel::NUM_GCACHE_BANKS);
  stats[STATNAME_DIRECTORY_EVICTION_SHARERS].set_num_items(
    rigel::COHERENCE_DIR_SETS*rigel::NUM_GCACHE_BANKS);
  stats[STATNAME_DIRECTORY_REREQUESTS].set_num_items(
    rigel::COHERENCE_DIR_SETS*rigel::NUM_GCACHE_BANKS);

  ProfileStat::retired_instrs = new uint64_t[rigel::THREADS_TOTAL];
  for(int i = 0; i < rigel::THREADS_TOTAL; i++) {
    ProfileStat::retired_instrs[i] = 0;
  }
  // Initialize the event tracking system.
  rigel::event::global_tracker.init();

  // Track number of cores that touch each cacheline in CCache.
  rigel::profiler::ccache_access_histogram = new uint64_t[2*rigel::THREADS_PER_CLUSTER];
  rigel::profiler::icache_access_histogram = new uint64_t[2*rigel::THREADS_PER_CLUSTER];

  // Clear the CCacheProfiler counters:
  for (int i = 0; i < 2*rigel::THREADS_PER_CLUSTER; i++) {
    rigel::profiler::ccache_access_histogram[i] = 0;
    rigel::profiler::icache_access_histogram[i] = 0;
  }

  // Start profiler for each cluster
  for (int tile = 0; tile < rigel::NUM_TILES; tile++) {
    tiles[tile]->PreSimInit();
  }

}
////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
/// helper_setup_fileio()
/// Initialize debug/profile IO.
////////////////////////////////////////////////////////////////////////////////
static void
helper_setup_fileio(CommandLineArgs &cmdline)
{
  if (rigel::REDIRECT_STDIN) {
    std::string filename = cmdline.get_val((char *)"stdin_file");
    FILE *tmp = freopen(filename.c_str(), "r", stdin);
    if(tmp == NULL)
    {
			std::cerr << "Error: " << filename << " could not be opened to redirect to stdin.\n";
      exit(1);
    }
  }
  if (rigel::REDIRECT_STDOUT) {
		std::string filename = cmdline.get_val((char*)"stdout_file");
    FILE *tmp = freopen(filename.c_str(), "w", stdout);
    if(tmp == NULL)
    {
			std::cerr << "Error: " << filename << " could not be opened to capture stdout.\n";
      exit(1);
    }
  }
  if (rigel::REDIRECT_STDERR) {
		std::string filename = cmdline.get_val((char*)"stderr_file");
    FILE *tmp = freopen(filename.c_str(), "w", stderr);
    if(tmp == NULL)
    {
			std::cerr << "Error: " << filename << " could not be opened to capture stderr.\n";
      exit(1);
    }
  }

  if (cmdline.get_val_bool((char *)"cctrace")) { rigel::GENERATE_CCTRACE = true; }
  if (rigel::GENERATE_CCTRACE) {
		std::string filename =   rigel::DUMPFILE_PATH + std::string("/")
                      + rigel::profiler::DUMPFILE_PREFIX
                      + std::string("cctrace.out");
    rigel::memory::cctrace_out = fopen(filename.c_str(), "w");
  }

  if (cmdline.get_val_bool((char *)"dramtrace")){ rigel::GENERATE_DRAMTRACE = true; }
  if (rigel::GENERATE_DRAMTRACE) {
		std::string filename =   rigel::DUMPFILE_PATH + std::string("/")
                      + rigel::profiler::DUMPFILE_PREFIX
                      + std::string("dramtrace.out");
    rigel::memory::dramtrace_out = fopen(filename.c_str(), "w");
  }

  if (cmdline.get_val_bool((char *)"coherencetrace")) rigel::GENERATE_COHERENCETRACE = true;
  if (rigel::GENERATE_DRAMTRACE) {
		std::string filename =   rigel::DUMPFILE_PATH + std::string("/")
                      + rigel::profiler::DUMPFILE_PREFIX
                      + std::string("coherencetrace.out");
    rigel::memory::coherencetrace_out = fopen(filename.c_str(), "w");
  }
  if (cmdline.get_val_bool((char *)"direvicttrace")) rigel::GENERATE_DIREVICTTRACE = true;
  if (rigel::GENERATE_DIREVICTTRACE) {
		std::string filename =   rigel::DUMPFILE_PATH + std::string("/")
                      + rigel::profiler::DUMPFILE_PREFIX
                      + std::string("direvicttrace.out");
    rigel::memory::direvicttrace_out = fopen(filename.c_str(), "w");
  }

  // Open the file that will hold the output from print regs.
	std::string rigelprint_fn;
  rigelprint_fn =   rigel::DUMPFILE_PATH + std::string("/")
                  + rigel::profiler::DUMPFILE_PREFIX
                  + std::string(rigel::RIGEL_PRINT_FILENAME);

  rigel::RIGEL_PRINT_FILEP = fopen(rigelprint_fn.c_str(), "w");

  if (NULL == rigel::RIGEL_PRINT_FILEP) {
    perror("fopen(RIGEL_PRINT_FILE)");
    assert(0);
  }

  setvbuf(rigel::RIGEL_PRINT_FILEP,NULL,_IOLBF,32);
}

////////////////////////////////////////////////////////////////////////////////
// helper_setup_other()
// Initialize core and general configuration parameters.
////////////////////////////////////////////////////////////////////////////////
// FIXME: use getopt or something else...
static void
helper_setup_other(CommandLineArgs &cmdline, char * argv[])
{
  using namespace rigel;
  using namespace mem_sched;

  // Parse all of the command line arguments.
 
  // set_rigel_vars 
  // Set the binary name so the profiler can get to it.
  rigel::RIGELSIM_BINARY_PATH = argv[0];
  
  // Get the time (seconds since UNIX epoch)
  rigel::SIM_START_TIME = time(NULL);
  
  // Set the minimum address for the stack.
  rigel::STACK_MIN_BASEADDR = 0xFFFFFFFF; // FIXME: constants :'(
  
  // Sentinal instruction value.
  rigel::NullInstr = &TempInstr;
  // end set_rigel_vars
 
  // ??? 
  PENDING_PER_BANK  = cmdline.get_val_int((char *)"PENDING_PER_BANK");
	std::string sched_policy = cmdline.get_val((char *)"DRAM_SCHEDULING_POLICY");
  if(sched_policy.compare("single") == 0)
    DRAM_SCHEDULING_POLICY = single;
  else if(sched_policy.compare("perbank") == 0)
    DRAM_SCHEDULING_POLICY = perbank;
  else
  {
		std::cerr << "Error: " << sched_policy << " is not a valid -mem-sched-policy value."
         << "  Try 'single' or 'perbank'.\n";
    assert(0);
  }

  // ??
	std::string rowcache_rep = cmdline.get_val((char *)"ROW_CACHE_REPLACEMENT_POLICY");
  if(rowcache_rep.compare("lru") == 0)
    ROW_CACHE_REPLACEMENT_POLICY = rc_replace_lru;
  else if(rowcache_rep.compare("tristage") == 0)
    ROW_CACHE_REPLACEMENT_POLICY = rc_replace_tristage;
  else if(rowcache_rep.compare("tristage_b") == 0)
    ROW_CACHE_REPLACEMENT_POLICY = rc_replace_tristage_b;
  else if(rowcache_rep.compare("mru") == 0)
    ROW_CACHE_REPLACEMENT_POLICY = rc_replace_mru;
  else if(rowcache_rep.compare("lfu") == 0)
    ROW_CACHE_REPLACEMENT_POLICY = rc_replace_lfu;
  else
  {
		std::cerr << "Error: " << rowcache_rep << " is not a valid row cache replacement policy "
         << "{lru, tristage, mru}.\n";
    assert(0);
  }

  // ??
  DRAM_BATCHING_CAP  = cmdline.get_val_int((char *)"DRAM_BATCHING_CAP");
	std::string batching_policy = cmdline.get_val((char *)"DRAM_BATCHING_POLICY");
  if(batching_policy.compare("batch_none") == 0)
    DRAM_BATCHING_POLICY = batch_none;
  else if(batching_policy.compare("batch_perbank") == 0)
    DRAM_BATCHING_POLICY = batch_perbank;
  else if(batching_policy.compare("batch_perchannel") == 0)
    DRAM_BATCHING_POLICY = batch_perchannel;
  else
  {
		std::cerr << "Error: " << batching_policy << " is not a valid batching policy "
         << "{batch_none, batch_perbank, batch_perchannel}.\n";
    assert(0);
  }


  HEARTBEAT_INTERVAL = cmdline.get_val_int((char *)"HEARTBEAT_INTERVAL");

  CLUSTERS_PER_TILE = cmdline.get_val_int((char *)"NUM_CLUSTERS");
  THREADS_PER_CORE  = cmdline.get_val_int((char *)"NUM_THREADS");
  NUM_TILES         = cmdline.get_val_int((char *)"NUM_TILES");

  //FIXME why is this commented out?
  /*
	std::string pipeline_bypass = cmdline.get_val((char *)"PIPELINE_BYPASS_PATHS");
  if(batching_policy.compare("full") == 0)
    PIPELINE_BYPASS_PATHS = pipeline_bypass_full;
  else if(batching_policy.compare("none") == 0)
    PIPELINE_BYPASS_PATHS = pipeline_bypass_none;
  else
  {
    cerr << "Error: " << pipeline_bypass << " is not a pipeline bypass configuration "
    << "{none, full}." << endl;
    assert(0);
  }
  */
  
  //--sleepy-cores doesn't work w/ MT or nonblocking memory ops.  The sleepy cores
  //implementation can be extended to make it work, but for now sleepy cores shuts down
  //a core as soon as it generates an L2D miss.
  if(rigel::SLEEPY_CORES && (rigel::THREADS_PER_CORE > 1 || rigel::NONBLOCKING_MEM))
  {
    fprintf(stderr, "Error: --sleepy-cores cannot be used with multithreading or nonblocking memory ops\n");
    assert(0);
  }
  
  NUM_CLUSTERS        = CLUSTERS_PER_TILE * NUM_TILES;
  THREADS_PER_CLUSTER = CORES_PER_CLUSTER * THREADS_PER_CORE;
  CORES_TOTAL         = NUM_CLUSTERS * CORES_PER_CLUSTER;
  THREADS_TOTAL       = CORES_TOTAL * THREADS_PER_CORE;

  //L1DS_PER_CORE     = THREADS_PER_CORE; // NOTE: we may want this to be 1 for shared L1D caches even with MT
  L1DS_PER_CORE       = 1;
  L1DS_PER_CLUSTER    = L1DS_PER_CORE * CORES_PER_CLUSTER;

  // Initialize any remaining runtime constants
  CYC_COUNT_MAX = cmdline.get_val_int((char *)"instr_count");

  InstrLegacy::LAST_INSTR_NUMBER = new uint64_t[THREADS_TOTAL];
  for (int i = 0; i < THREADS_TOTAL; i++) {
    InstrLegacy::LAST_INSTR_NUMBER[i] = 0;
  }

  // Set all of the TQ latency parameters TODO: We could make CommandLineArgs global
  // and move a lot of these initialization routines into the constructors
  using namespace rigel::task_queue;
  LATENCY_ENQUEUE_ONE = cmdline.get_val_int((char *)"LATENCY_ENQUEUE_ONE");
  LATENCY_ENQUEUE_LOOP = cmdline.get_val_int((char *)"LATENCY_ENQUEUE_LOOP");
  LATENCY_DEQUEUE = cmdline.get_val_int((char *)"LATENCY_DEQUEUE");
  LATENCY_BLOCKED_SYNC = cmdline.get_val_int((char *)"LATENCY_BLOCKED_SYNC");

  // default corenum to watch/trace in Interactive Mode
  INTERACTIVE_CORENUM=0;

  //Number of rows in per-DRAM-bank Row Cache (akin to WC-DRAM)
  rigel::DRAM::OPEN_ROWS = (unsigned int)cmdline.get_val_int((char *)"DRAM_OPEN_ROWS");

  // memdump file?
  int str_len = cmdline.get_val((char *) "memdump").length() + 1;
  strncpy(rigel::MEM_DUMP_FILE, cmdline.get_val((char *)"memdump").c_str(), str_len);

}

////////////////////////////////////////////////////////////////////////////////
// If --sigint-stats is passed, this will be registered as the SIGINT handler.
// It will throw an ExitSim exception and end the simulation gracefully,
// printing out all stats, instead of using the built-in OS handler and dying.
// SIGINT is the signal thrown when CTRL+C is pressed in a terminal window,
// and can be passed to SLURM jobs via "scancel -s INT <job #>
// TODO: Add other signal handlers to enable, for instance, going in and out of
// interactive mode
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// sigint handler
////////////////////////////////////////////////////////////////////////////////
void sigint_handler(int sig)
{
  //rigel::GLOBAL_debug_sim.dump_all();
  throw ExitSim("SIGINT CAUGHT");
  (void) signal(SIGINT, SIG_DFL);
}

////////////////////////////////////////////////////////////////////////////////
/// sigsegv handler
////////////////////////////////////////////////////////////////////////////////
void sigsegv_handler(int sig)
{
  // Print out the backtrace and die.
  DEBUG_HEADER();
  fprintf(stderr, "\nSIGSEGV caught.  Signal number: %d\n", sig);
//FIXME make this check more robust
#ifdef __linux__
  void *array[20];
  size_t size;
  size = backtrace(array, 20);
  backtrace_symbols_fd(array, size, 2);
#endif
  helper_close_files();
  exit(1);
}

