
////////////////////////////////////////////////////////////////////////////////
// memory_model_gddr4.cpp
////////////////////////////////////////////////////////////////////////////////
//        class MemoryModelGDDR4 (Implementations)
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIX32, PRIu64
#include <stddef.h>                     // for NULL, size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for fprintf, stderr, printf, etc
#include <stdlib.h>                     // for exit, malloc
#include <cmath>                        // for floor
#include <fstream>                      // for basic_ostream, operator<<, etc
#include <iomanip>                      // for operator<<, setfill, setw, etc
#include <iostream>                     // for hex, std::cerr
#include <map>                          // for map, etc
#include <set>                          // for set, set<>::iterator
#include <string>                       // for string, char_traits, etc
#include <utility>                      // for pair
#include <vector>                       // for vector, etc
#include "RandomLib/Random.hpp"  // for Random
#include "memory/address_mapping.h"  // for AddressMapping
#include "memory/dram.h"           // for CONTROLLERS, BANKS, RANKS, etc
#include "memory/dram_channel_model.h"  // for DRAMChannelModel, etc
#include "memory/dram_controller.h"  // for DRAMController, etc
#include "locality_tracker.h"  // for LocalityTracker, etc
#include "memory/memory_timing_dram.h"
#include "mshr.h"           // for MissHandlingEntry
#include "profile/profile.h"        // for ProfileStat
#include "sim.h"            // for TARGET_ARGS, LINESIZE, etc
#include "util/util.h"           // for ELFAccess, ExitSim
#include "tlb.h"            // for TLB
#include "memory/backing_store.h" //for GlobalBackingStoreType definition

namespace rigel {
  extern RandomLib::Random RNG;
}

////////////////////////////////////////////////////////////////////////////////
// MemoryModelGDDR4() (constructor)
// RETURNS: ---
////////////////////////////////////////////////////////////////////////////////
MemoryTimingDRAM::MemoryTimingDRAM(bool _collisionChecking)
{
  using namespace rigel::DRAM;
  DRAMModelArray = (DRAMChannelModel **) malloc(CONTROLLERS*sizeof(*DRAMModelArray));
  if(NULL == DRAMModelArray)
  {
    std::cerr << "Error allocating DRAM Model array (out of memory)" << "\n";
    assert(0);
  }
  Controllers = (DRAMController **) malloc(CONTROLLERS*sizeof(*Controllers));
  if(NULL == Controllers)
  {
    std::cerr << "Error allocating DRAM Controller array (out of memory)" << "\n";
    assert(0);
  }
  lines_serviced_buffer = new uint64_t **[CONTROLLERS];
  requests_serviced_buffer = new uint64_t **[CONTROLLERS];
  
  for(unsigned int i = 0; i < CONTROLLERS; i++)
  {
    DRAMModelArray[i] = new DRAMChannelModel(i);
    Controllers[i] = new DRAMController(i, DRAMModelArray[i], _collisionChecking);
    lines_serviced_buffer[i] = new uint64_t *[RANKS];
    requests_serviced_buffer[i] = new uint64_t *[RANKS];
    for(unsigned int j = 0; j < RANKS; j++)
    {
      lines_serviced_buffer[i][j] = new uint64_t [BANKS];
      requests_serviced_buffer[i][j] = new uint64_t [BANKS];
      for(unsigned int k = 0; k < BANKS; k++)
      {
        lines_serviced_buffer[i][j][k] = 0;
        requests_serviced_buffer[i][j][k] = 0;
      }
    }
  }

  //Make sure the block of cache lines contiguously mapped to a single G$ bank
  //is not larger than a DRAM row.  This would cause problems, since we stride across
  //memory controllers every DRAM row, and every G$ bank can only cache the address space
  //covered by a single memory controller.
  assert(rigel::cache::LINESIZE * rigel::cache::GCACHE_CONTIGUOUS_LINES < (COLS*COLSIZE) &&
      "LINESIZE*GCACHE_CONTIGUOUS_LINES must be less than 1 DRAM row");

  // Dump to log the memory size we are simulating
  uint64_t actual_memsize = (uint64_t) COLSIZE * COLS * ROWS * BANKS * RANKS * CONTROLLERS;
  fprintf(stderr, "%d, %d, %d, %d, %"PRIu64", (ROWS, BANKS, RANKS, CONTROLLERS, MEMORY SIZE BYTES)\n",
    ROWS, BANKS, RANKS, CONTROLLERS, actual_memsize);

  // Open the debug trace file
  // Open the dump file for the histogram data
  char file_name[1024];

  // FILENAME: <dumpfile-path>/<dumpfile-prefix><name>.csv
  snprintf(file_name, 1024, "%s/%sdebug-trace.out",
           rigel::DUMPFILE_PATH.c_str(),
           rigel::profiler::DUMPFILE_PREFIX.c_str());
  FILE_memory_trace = fopen(file_name, "w");
#ifndef _MSC_VER
  setlinebuf(FILE_memory_trace);
#endif // _MSC_VER
  if (NULL == FILE_memory_trace) {
    perror("fopen()");
    assert(0 && "Failed to load memory model debug file.");
  }

  // Perform check of memory if enabled.
  if (rigel::CMDLINE_ENUMERATE_MEMORY) {
    if (alias_check()) {
      fprintf(stderr, "Memory alias check PASSED.\n");
      exit(0);
    } else {
      fprintf(stderr, "Memory alias check FAILED.\n");
      exit(1);
    }
  }
#if 0
  //Open BLP/MLP tracking file
  FILE_load_balance_trace = fopen("RUN_OUTPUT/lb.csv", "w");
#endif
  //Initialize locality tracker
  if(rigel::ENABLE_LOCALITY_TRACKING)
  {
    aggregateLocalityTracker = new LocalityTracker("DRAM Accesses", 8, LT_POLICY_RR,
      rigel::DRAM::CONTROLLERS, rigel::DRAM::RANKS,
      rigel::DRAM::BANKS, rigel::DRAM::ROWS);
  }

  //Load target argc and argv into memory.  Passed into the rigelsim binary via
  //--args <NARGS> ARG1 ARG2 ... ARGN
  //They are laid out as follows:
  //ARG1 ARG2 .. ARGN ARGV 0x400000
  //Where ARGV is an array of N pointers to the arguments themselves.  Note that
  //ARGV is right before the beginning of the heap at (arbitrarily) 0x400000.
  //Note also that everything should be word-aligned.
  size_t totalArgSize = sizeof(uint32_t)*rigel::TARGET_ARGS.size();
  for(std::vector<std::string>::const_iterator it = rigel::TARGET_ARGS.begin(),
                                               end = rigel::TARGET_ARGS.end();
      it != end; ++it)
  {
    size_t chars = it->length()+1;//Need a NULL terminator, argv are strings.
    size_t words = (chars+3)/4;
    size_t bytes = words*4;
    totalArgSize += bytes;
  }
  assert(rigel::CODEPAGE_HIGHWATER_MARK+totalArgSize < 0x400000 && "Code+Args too large");
  uint32_t argvBaseAddr = 0x400000-(sizeof(uint32_t)*rigel::TARGET_ARGS.size());
  uint32_t argumentsBaseAddr = 0x400000-totalArgSize;
  for(size_t j = 0; j < rigel::TARGET_ARGS.size(); j++)
  {
    //Write argument (byte at a time :( 
    const char *arg = rigel::TARGET_ARGS[j].c_str();
    const unsigned char *argu = (const unsigned char *)arg;
    const unsigned char *end = argu + strlen(arg) + 1; //Need to write out the NULL terminator
    printf("Writing 0x%08X to MEM[0x%08X] as part of argv[%zu]\n", argumentsBaseAddr, argvBaseAddr, j);
    rigel::GLOBAL_BACKING_STORE_PTR->write_word(argvBaseAddr, argumentsBaseAddr);
    argvBaseAddr += 4;
    uint32_t data;
    while(argu != end)
    {
      data = 0U;
      data |= ((*argu++) << 0);
      if(argu != end) {
        data |= ((*argu++) << 8);
        if(argu != end) {
          data |= ((*argu++) << 16);
          if(argu != end) {
            data |= ((*argu++) << 24);
          }
        }
      }
      rigel::GLOBAL_BACKING_STORE_PTR->write_word(argumentsBaseAddr, data);
      printf("Writing 0x%08X to MEM[0x%08X] as part of *argv[%zu]\n", data, argumentsBaseAddr, j);
      argumentsBaseAddr += 4;
    }
  }
}
////////////////////////////////////////////////////////////////////////////////

//#define DEBUG_PRINT_DRAM_TRACE
// Will not be set until after LoadELF is called and has loaded code
uint32_t rigel::CODEPAGE_HIGHWATER_MARK = 0x0;
uint32_t rigel::CODEPAGE_LIBPAR_BEGIN = 0x0;
uint32_t rigel::CODEPAGE_LIBPAR_END = 0x0;
uint32_t rigel::LOCKS_REGION_BEGIN = 0x0;
uint32_t rigel::LOCKS_REGION_END = 0x0;

/////////////////////////////////////////////////////////////////////////////////
/// alias_check()
/////////////////////////////////////////////////////////////////////////////////
/// Check for two addresses in the target address space both mapping to the same
/// host address by enumerating all of memory, checking for poisoning, then
/// poisoning as we go. Assumes a 4 GiB address space and allocation granularity
/// of at least 32 words in the target address space.
/// RETURNS: true if no aliasing occurs, dumps state and returns false otherwise.
/////////////////////////////////////////////////////////////////////////////////
bool MemoryTimingDRAM::alias_check()
{
  fprintf(stderr, "Begining Enumeration...\n");
     using namespace rigel::DRAM;
     fprintf(stderr, "COLSIZE:         %2d\n", COLSIZE);
     fprintf(stderr, "COLS:            %2d\n", COLS);
     fprintf(stderr, "ROWS:            %2d\n", ROWS);
     fprintf(stderr, "BANKS:           %2d\n", BANKS);
     fprintf(stderr, "RANKS:           %2d\n", RANKS);
     fprintf(stderr, "CONTROLLERS:     %2d\n", CONTROLLERS);
 for (uint32_t addr = 0; addr < 0xcfffffc0; addr+=32) {
   uint32_t data = 0;
   if (0 == (addr % 0x08000000)) {
     fprintf(stderr, "%4d MiB\n", addr >> 20);
   }
   data = rigel::GLOBAL_BACKING_STORE_PTR->read_word(addr);
   if (data != rigel::memory::MEMORY_INIT_VALUE) {
     fprintf(stderr, "Found an alias at addr: 0x%08x\n", addr);
     fprintf(stderr, "data: 0x%08x\n", data);
     fprintf(stderr, "COL : %3d\n", AddressMapping::GetCol(addr));
     fprintf(stderr, "ROW : %3d\n", AddressMapping::GetRow(addr));
     fprintf(stderr, "BANK: %3d\n", AddressMapping::GetBank(addr));
     fprintf(stderr, "RANK: %3d\n", AddressMapping::GetRank(addr));
     fprintf(stderr, "CTRL: %3d\n", AddressMapping::GetController(addr));
     // FAILURE!
     return false;
   }
   // Poison!
   rigel::GLOBAL_BACKING_STORE_PTR->write_word(addr, 0xdeadbeef);
 }
 // SUCCESS!
 return true;
}

////////////////////////////////////////////////////////////////////////////////
// PerCycle()
////////////////////////////////////////////////////////////////////////////////
// Called every cycle to clock the GDDR memory controller.  It selects requests
// and schedules them.
// RETURNS: ---
////////////////////////////////////////////////////////////////////////////////
void
MemoryTimingDRAM::PerCycle() {
  using namespace rigel::DRAM;
  using namespace rigel::cache;
  using namespace rigel::mem_sched;
#if 0
  CollectStats();
#endif
  int numClks = floor(CLK_RATIO);
  float fractionalPart = CLK_RATIO - (float) numClks;
  //Use this implementation if you want to use rand()
  //if(((float)(rand()) / (float)(RAND_MAX)) < fractionalPart)
  //  numClks++;
  //This implementation uses RandomLib (which uses a Mersenne Twister PRNG)
  if(RNG.Prob(fractionalPart))
    numClks++;

  for (int clk = 0; clk < numClks; clk++)
  {
    // Increment clock for DRAM (may move faster than CURR_CYCLE)
    DRAMChannelModel::DRAM_CURR_CYCLE++;
    //TODO: Doing this from here is a hack
    if(ProfileStat::is_active())
      ProfileStat::increment_active_dram_cycles();

    // Clock our DRAM chips and controllers
    for (unsigned int i = 0; i < CONTROLLERS; i++)
    {
      Controllers[i]->PerCycle();
      //FIXME for now we just gate clocking the channel model because it is not used
      //when the row cache is on (the DRAM controller handles stats itself).
      //we should really extend the model to let it handle the row cache case
      //(really, we should rewrite the thing to be more useful, or cut it)
      if(rigel::DRAM::OPEN_ROWS == 1)
        DRAMModelArray[i]->PerCycle();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// CollectStats()
////////////////////////////////////////////////////////////////////////////////
//Called from PerCycle()
////////////////////////////////////////////////////////////////////////////////
void MemoryTimingDRAM::CollectStats() {
  using namespace rigel::DRAM;
  using namespace rigel::cache;
  using namespace rigel::mem_sched;
  if(rigel::CURR_CYCLE % 1000 != 0) return;
  int mlp = 0;
  int blp = 0;
  int ctrlmlp;
  std::map<int,int> bank_occupancy_hist, ctrl_occupancy_hist;
  printf("Occupancies:\n");
  for(unsigned int i = 0; i < CONTROLLERS; i++)
  {
    ctrlmlp = 0;
    printf("Ctrl %d: ", i);
    for(unsigned int j = 0; j < RANKS; j++)
    {
      printf("(");
      for(unsigned int k = 0; k < BANKS; k++)
      {
//         printf("%2d ", Controllers[i]->requestBufferOccupancy[j][k]);
//         occupancy_hist[Controllers[i]->requestBufferOccupancy[j][k]]++;
//         ctrlmlp += Controllers[i]->requestBufferOccupancy[j][k];

        int linesServicedSinceLastSample = Controllers[i]->linesServiced[j][k]-lines_serviced_buffer[i][j][k];
        int requestsServicedSinceLastSample = Controllers[i]->requestsServiced[j][k]-requests_serviced_buffer[i][j][k];
        printf("%3d,%1.1f ", linesServicedSinceLastSample, ((double)linesServicedSinceLastSample/(double)requestsServicedSinceLastSample));
        fprintf(FILE_load_balance_trace, "%3d", linesServicedSinceLastSample);
        if(!((i == (CONTROLLERS-1)) && (j == (RANKS-1)) && (k == (BANKS-1))))
          fprintf(FILE_load_balance_trace, ",");
        
        bank_occupancy_hist[linesServicedSinceLastSample]++;
        ctrlmlp += linesServicedSinceLastSample;
        
        //if(Controllers[i]->requestBufferOccupancy[j][k] != 0)
        //  blp++;
        if(linesServicedSinceLastSample != 0)
          blp++;
        lines_serviced_buffer[i][j][k] = Controllers[i]->linesServiced[j][k];
        requests_serviced_buffer[i][j][k] = Controllers[i]->requestsServiced[j][k];
      }
      printf(")");
    }
    printf(" %d\n", ctrlmlp);
    mlp += ctrlmlp;
    ctrl_occupancy_hist[ctrlmlp]++;
  }
  fprintf(FILE_load_balance_trace, "\n");
  //Approximate Gini coefficient (aka Gini index)
  double s = 0.0f;
  double summation_term = 0.0f;

  //int numNonzeroBanks = (CONTROLLERS*RANKS*BANKS)-occupancy_hist[0];
  
  for(std::map<int,int>::const_iterator it = bank_occupancy_hist.begin(), end = bank_occupancy_hist.end();
      it != end; ++it)
  {
    const int reqs = it->first;
    //const double fractionOfBanks = (double)(it->second)/numNonzeroBanks;
    const double fractionOfBanks = (double)(it->second)/(CONTROLLERS*RANKS*BANKS);
    const double si = fractionOfBanks * reqs;
    summation_term += fractionOfBanks * (s + (s + si));
    s += si;
  }
  double bankGini = 1.0f - (summation_term / s);

  s = 0.0f; summation_term = 0.0f;
  for(std::map<int,int>::const_iterator it = ctrl_occupancy_hist.begin(), end = ctrl_occupancy_hist.end();
      it != end; ++it)
  {
    const int reqs = it->first;
    //const double fractionOfBanks = (double)(it->second)/numNonzeroBanks;
    const double fractionOfBanks = (double)(it->second)/CONTROLLERS;
    const double si = fractionOfBanks * reqs;
    summation_term += fractionOfBanks * (s + (s + si));
    s += si;
  }
  double ctrlGini = 1.0f - (summation_term / s);
  
  printf("MLP %d BLP %d BANKGINI %f CTRLGINI %f\n", mlp, blp, bankGini, ctrlGini);
}


////////////////////////////////////////////////////////////////////////////////
// Schedule()
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool MemoryTimingDRAM::Schedule(const std::set<uint32_t> addrs, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  CallbackInterface *requestingEntity, int requestIdentifier)
{
  //FIXME: Deal with addrs that spans controllers.
  //It should be an error, but it is currently checked so many other places that I don't bother here.
  int controller = AddressMapping::GetController(*(addrs.begin()));
  bool ret = Controllers[controller]->Schedule(addrs, size, MSHR, requestingEntity, requestIdentifier);

  //If it was scheduled successfully, get locality stats
  if(rigel::ENABLE_LOCALITY_TRACKING)
  {
    if(ret)
    {
      for(std::set<uint32_t>::iterator it = addrs.begin(); it != addrs.end(); ++it)
      {
        aggregateLocalityTracker->addAccess(*it);
      }
    }
  }
        
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void MemoryTimingDRAM::dump()
{
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;

  fprintf(stderr, "#### MEMORY MODEL #####\n");
  for (unsigned int ctrl = 0; ctrl < CONTROLLERS; ctrl++)
  {
    Controllers[ctrl]->dump(false);
  }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void MemoryTimingDRAM::dump_locality() const
{
  assert(rigel::ENABLE_LOCALITY_TRACKING &&
         "Locality tracking disabled, MemoryModel::dump_locality() called.");
  aggregateLocalityTracker->dump();
}
