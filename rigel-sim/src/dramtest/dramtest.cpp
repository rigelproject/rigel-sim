////////////////////////////////////////////////////////////////////////////////
// sim.cpp
////////////////////////////////////////////////////////////////////////////////
// TODO: COMMENT ME
// TODO: COMMENT ME
// TODO: COMMENT ME
// TODO: COMMENT ME
////////////////////////////////////////////////////////////////////////////////

#include <cassert>                     // for assert
#include <cmath>                       // for ceil
#include <stdint.h>                     // for uint64_t
#include <cstdio>                      // for printf, fprintf, FILE, NULL, etc
#include <cstdlib>                     // for exit, free, srand
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <string>                       // for char_traits, operator<<, etc
#include "../user.config"   // for ENABLE_VERIF
#include "RandomLib/Random.hpp"  // for Random
#include "define.h"         // for InstrLegacy (ptr only), etc
#include "memory/dram.h"           // for CONTROLLERS, ROWS, BANKS, etc
#include "memory/dram_driver.h"    // for DRAMDriver
#include "memory_timing.h"
#include "memory/backing_store.h"
#include "instr.h"          // for InstrLegacy, etc
#include "profile/profile.h"        // for Profile, ProfileStat
#include "read_mem_trace_c.h"  // for destroy_mem_trace_reader, etc
#include "sim.h"            // for THREADS_TOTAL, CURR_CYCLE, etc
#include "stream_generator.h"  // for GzipRequestGenerator, etc
#include "util/task_queue.h"     // for TaskSystemBaseline
#include "util/util.h"           // for CommandLineArgs, ExitSim

extern "C" {
}


                  // PC        tid  pred_pc     raw_instr   i_type
InstrLegacy TempInstr(0xF000000F, -1, 0xF000000F, 0x00000000, I_NULL); // this is the NullInstr
InstrLegacy * rigel::NullInstr;

namespace rigel {
  RandomLib::Random RNG(0);
	RandomLib::Random TargetRNG(0);
}

rigel::GlobalBackingStoreType * rigel::GLOBAL_BACKING_STORE_PTR;

static void helper_setup_other(CommandLineArgs &cmdline);
static void helper_setup_dram(int numMemoryControllers);

////////////////////////////////////////////////////////////////////////////////
/// helper_setup_other()
/// Initialize core and general configuration parameters.
////////////////////////////////////////////////////////////////////////////////
static void
helper_setup_other(CommandLineArgs &cmdline)
{
  using namespace rigel;
  using namespace mem_sched;

  // Parse all of the command line arguments.
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

  HEARTBEAT_INTERVAL = cmdline.get_val_int((char *)"HEARTBEAT_INTERVAL");

  CLUSTERS_PER_TILE = cmdline.get_val_int((char *)"NUM_CLUSTERS");
  THREADS_PER_CORE  = cmdline.get_val_int((char *)"NUM_THREADS");
  NUM_TILES         = cmdline.get_val_int((char *)"NUM_TILES");

  NUM_CLUSTERS        = CLUSTERS_PER_TILE * NUM_TILES;
  THREADS_PER_CLUSTER = CORES_PER_CLUSTER * THREADS_PER_CORE;
  CORES_TOTAL         = NUM_CLUSTERS * CORES_PER_CLUSTER;
  THREADS_TOTAL       = CORES_TOTAL * THREADS_PER_CORE;

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
}


void static
helper_setup_dram(int numMemoryControllers)
{
  using namespace rigel::DRAM;
  if(numMemoryControllers == -1)
    CONTROLLERS = rigel::NUM_TILES;
  else
  {
    //cannot be negative
    if(numMemoryControllers <= 0)
    {
			std::cerr << "Error: -memchannels value (" << numMemoryControllers << ") must be > 0\n";
      assert(0);
    }
    //If none of the above asserts fired, the value given is OK.
    CONTROLLERS = numMemoryControllers;
  }
  rigel::NUM_GCACHE_BANKS = rigel::cache::GCACHE_BANKS_PER_MC * CONTROLLERS;
  ROWS = 4096;
  // The DRAM rank bits "fill in" whatever the other bit ranges do
  // not cover.  They go in between row and bank bits (see address_mapping.h)
  RANKS = (unsigned int)((uint64_t)(1ULL << 32) / CONTROLLERS / BANKS / ROWS / COLS / COLSIZE);
  ROWS = ceil(((double)(1ULL << 32)) / ((double)CONTROLLERS * BANKS * RANKS * COLS * COLSIZE));
}


////////////////////////////////////////////////////////////////////////////////
/// main()
/// This is THE MAIN for the simulator.  The simulator starts HERE!
/// 
/// RETURNS: Never since proper simulation ends with an ExitSim() call.
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char ** argv)
{
  // timestamp greeting! for version tracking and debugging
  fprintf(stdout,"RigelSim Compiled by %s on %s at %s from Git commit:\n%s\n",
    BUILD_USER,__DATE__, __TIME__, GIT_REV); // values defined in Make-rules
  rigel::NullInstr = &TempInstr;

  CommandLineArgs cmdline(argc, argv);

  srand(0); //We shouldn't use rand() or rand48(), but seed it just in case.

  // Default is 1 controller and 4 associated G$ banks per tile, user can specify a specific number
  // with -memchannels <N> on the command line
  int numMemoryControllers = cmdline.get_val_int((char *)"MEMCHANNELS");

  // Initialize core parameters not covered above.
  helper_setup_other(cmdline);

  // Initialize parameters for the DRAM.
  helper_setup_dram(numMemoryControllers);
  // Dummy, just need to have it so dump_global_profile() doesn't segfault
  GlobalTaskQueue = new TaskSystemBaseline;
 
  ConstructionPayload dummy; //For now, we don't do anything with this for dramtest.
  rigel::GlobalBackingStoreType backing_store(dummy, 0);
  rigel::GLOBAL_BACKING_STORE_PTR = &backing_store;

  // We don't want collision checking if we're a DRAM simulator, so 2nd argument is false.
  rigel::MemoryTimingType memory_timing(NULL, false);

  DRAMDriver dd(memory_timing);
  StrideReadGenerator srg1(0x00000000, 32);
  StrideWriteGenerator srg2(0x20000000, 32);
  StrideAlternatingReadWriteGenerator srg3(0x40000000, 32, 8, true);
  StrideWriteGenerator srg4(0x60000000, 32);
  StrideReadGenerator srg5(0x80000000, 32);
  StrideRandomReadWriteGenerator srg6(0xA0000000, 32, 0.5);
  StrideReadGenerator srg7(0xC0000000, 32);
  StrideWriteGenerator srg8(0xE0000000, 32);
  struct mem_trace_reader* mtr = init_trace_reader("memstream.computeFH.gz");

  FILE **dineroPipes = new FILE *[rigel::NUM_CLUSTERS];
  printf("HI! %d %d \n", rigel::NUM_CLUSTERS, rigel::THREADS_TOTAL);
  for(int i = 0; i < rigel::NUM_CLUSTERS; i++)
  {
    char command[1024];
    sprintf(command, "/FIXME/PATH/TO/d4-7/dineroIV -informat d -l1-dsize 64k -l1-dbsize 32 -l1-dassoc 8 1>c%d.out 2>c%d.err", i, i);
    printf("%s\n", command);
    dineroPipes[i] = popen(command, "w");
    if(dineroPipes[i] == NULL)
    {
      printf("Error: dineroPipes[%d] == NULL\n", i);
      exit(1);
    }
  }
  GzipRequestGenerator **gz = new GzipRequestGenerator *[rigel::THREADS_TOTAL];
  for(int i = 0; i < rigel::THREADS_TOTAL; i++)
     gz[i] = new GzipRequestGenerator(mtr, i, rigel::THREADS_TOTAL);
  int *numRequests = new int[rigel::THREADS_TOTAL];
  for(int i = 0; i < rigel::THREADS_TOTAL; i++)
    numRequests[i] = 0;
  while(1)
  {
    int numNulls = 0;
    for(int i = 0; i < rigel::THREADS_TOTAL; i++)
    {
      MemoryRequest *mr;
      if((mr = gz[i]->getNextRequest()) == NULL)
        numNulls++;
      else
      {
        fprintf(dineroPipes[i/rigel::THREADS_PER_CLUSTER], "%c %8x\n", ((mr->readNotWrite) ? '0' : '1'), mr->addr);
        free(mr);
        numRequests[i]++;
      }
    }
    if(numNulls == rigel::THREADS_TOTAL) break;
  }
  for(int i = 0; i < rigel::THREADS_TOTAL; i++)
    printf("Thread %4d: %8d requests\n", i, numRequests[i]);
  for(int i = 0; i < rigel::NUM_CLUSTERS; i++)
    pclose(dineroPipes[i]);
  free(dineroPipes);
  free(numRequests);
  exit(1);
  
  
  GzipRequestGenerator gz1(mtr, 0, 4);
  GzipRequestGenerator gz2(mtr, 1, 4);
  GzipRequestGenerator gz3(mtr, 2, 4);
  GzipRequestGenerator gz4(mtr, 3, 4);

  
  
  ConcurrentStreamGenerator csg1(&dd, 0, gz1, 4);
  ConcurrentStreamGenerator csg2(&dd, 1, gz2, 4);
  ConcurrentStreamGenerator csg3(&dd, 2, gz3, 4);
  ConcurrentStreamGenerator csg4(&dd, 3, gz4, 4);
  dd.addStream(&csg1);
  dd.addStream(&csg2);
  dd.addStream(&csg3);
  dd.addStream(&csg4);
  /*
  ConcurrentStreamGenerator csg1(&dd, 0, srg1, 1);
  ConcurrentStreamGenerator csg2(&dd, 1, srg2, 1);
  ConcurrentStreamGenerator csg3(&dd, 2, srg3, 1);
  ConcurrentStreamGenerator csg4(&dd, 3, srg4, 1);
  ConcurrentStreamGenerator csg5(&dd, 4, srg5, 1);
  ConcurrentStreamGenerator csg6(&dd, 5, srg6, 1);
  ConcurrentStreamGenerator csg7(&dd, 6, srg7, 1);
  ConcurrentStreamGenerator csg8(&dd, 7, srg8, 1);
  dd.addStream(&csg1);
  dd.addStream(&csg2);
  dd.addStream(&csg3);
  dd.addStream(&csg4);
  dd.addStream(&csg5);
  dd.addStream(&csg6);
  dd.addStream(&csg7);
  dd.addStream(&csg8);
*/
  // Initialize the global profiler and use STDERR as the output stream.
  ProfileStat::init(stderr);
  Profile profiler(rigel::GLOBAL_BACKING_STORE_PTR, 0);
  profiler.start_sim();
  ProfileStat::activate();
  // Handle all of the bookkeeping prior to the first cycle

  // Start simulation proper
  //uint32_t addr = 0;
  //MissHandlingEntry<32> mshr;
  try {
    for (rigel::CURR_CYCLE = 0; ; rigel::CURR_CYCLE++)
    {
      using namespace rigel;
      // XXX: This is where all of the PerCycle magic happens.
      using namespace rigel::cache;
      ProfileStat::PerCycle();
      memory_timing.PerCycle();
      dd.PerCycle();
      /*
        if(rigel::CURR_CYCLE % 500 == 0)
        {
        mshr.set(addr, 1, true, IC_MSG_READ_REQ, -10, 1, 1);
        if(!memory_model.Schedule(addr, 32, mshr, &dd, rigel::CURR_CYCLE))
        {
          addr += 2048; addr -= addr % 2048;
        }
        else
        {
          addr += 32;
        }
        }
        */
      if (CURR_CYCLE == U64_MAX_VAL || CURR_CYCLE >= CYC_COUNT_MAX || CURR_CYCLE >= 1000000) throw ExitSim("Early Exit");
    }
  }
  catch (ExitSim e)
  {
    profiler.end_sim();
    profiler.accumulate_stats();
    profiler.global_dump_profile(argc, argv);
		std::cerr << "EXIT REASON, " << e.reason << "\n";
    destroy_mem_trace_reader(mtr);
    exit(0);
  }
  /* End simulation proper */

	std::cerr << "!!! EXITING SIMULATION EARLY AT CYCLE ";
	std::cerr << std::dec << rigel::CURR_CYCLE << " !!!\n";
  return 0;
}
