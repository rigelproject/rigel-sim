#ifndef __RIGELSIM_H__
#define __RIGELSIM_H__

#include <fcntl.h>
#include <sys/stat.h>
#include <string>
#include <unistd.h>
#include <sstream>

#include "sim.h"
#include "chip/chip.h"
#include "shell/shell.h"
#include "broadcast_manager.h"  // for BroadcastManager
#include "memory/dram.h"           // for CONTROLLERS, RANKS, ROWS, etc
#include "caches_legacy/global_cache.h"  // for GlobalCache, etc
#include "memory/backing_store.h"

#include "util/construction_payload.h"
#include "rigelsim.pb.h"
#include "util/syscall.h"
#include "RandomLib/Random.hpp"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>

namespace rigel {
  extern RandomLib::Random RNG, TargetRNG;
}

////////////////////////////////////////////////////////////////////////////////
/// class RigelSim
/// the main simulator object
/// TODO: this should be a strictly singleton class
////////////////////////////////////////////////////////////////////////////////
class RigelSim {

  public:

    ////////////////////////////////////////////////////////////////////////////
    // public methods
    ////////////////////////////////////////////////////////////////////////////

    /// constructor
    RigelSim(CommandLineArgs* cl) :
      _cmdline( cl ) 
    {
      rigel::ConstructionPayload cp;
      cp.component_count = NULL;

      // beware of order dependencies between initialization steps
      init_protobuf(cp); // Initialize protobuf object (possibly from a file), create ChipState
      setup_dram();      // Initialize parameters for the DRAM.
      init_memory(cp);   // initialize the main memory model (GDDR, etc)
      init_sim(cp);      // other simulator initialization steps
      init_chip(cp);     // initialize the Chip object, holds chip components
      load_checkpoint(); // optionally, load a checkpoint
      init_shell();
    };

    /// destructor
    ~RigelSim() {
      delete memory_timing;
      delete backing_store;
      delete chip_;
      delete chip_state;
    }

    static const int protobuf_compression_level = 9;

    ////////////////////////////////////////////////////////////////////////////
    // EndSim()
    ////////////////////////////////////////////////////////////////////////////
    /// called to clean up at the end of simulation, dump stats, etc.
    /// TODO: could this just be the destructor for RigelSim?
    void EndSim() {

      save_state();

      chip_->EndSim();

      if(!protobuf_out_filename.empty()) {
        using namespace google::protobuf::io; 
        int fd = open(protobuf_out_filename.c_str(),
                      O_WRONLY | O_CREAT | O_TRUNC, 
                      S_IRUSR | S_IWUSR); 
        if (fd == -1)
        {
          throw "open failed on output file"; 
        } 
        FileOutputStream file_stream(fd); 
        GzipOutputStream::Options options; 
        options.format = GzipOutputStream::GZIP; 
        options.compression_level = protobuf_compression_level; 
        GzipOutputStream gzip_stream(&file_stream, options); 
        if (!chip_state->SerializeToZeroCopyStream(&gzip_stream)) { 
          std::cerr << "Failed to dump protobuf to file\n"; 
        }
        printf("%s",chip_state->DebugString().c_str());
        close(fd); 
      } 

    }

    ////////////////////////////////////////////////////////////////////////////
    // initializers for member classes
    ////////////////////////////////////////////////////////////////////////////

    void init_syscall(rigel::ConstructionPayload &cp) {
      syscall = new Syscall(backing_store);
      cp.syscall = syscall;
    }

    /// Load a Checkpoint
    void load_checkpoint() {
      // check if we should recover a checkpoint
      if (rigel::LOAD_CHECKPOINT) {
        printf("Recovering a Checkpoint...\n");
        // load memory image
        printf("Recovering Memory Image...\n");
        for(int i = 0; i < rigel::CORES_TOTAL * rigel::THREADS_PER_CORE; i++) {
          // set PCs
          printf("Recovering PCs...\n");
          //set_pcs(i);
          // load regfiles
          printf("Recovering Register File state...\n");
          //load_register_file(i);
        }
        throw ExitSim((char *)"Checkpoint Recovery is Alpha"); 
      }
    }


    /// init shell
    void init_shell() {
      shell = new InteractiveShell();
    }

    void init_protobuf(rigel::ConstructionPayload &cp) {
      protobuf_in_filename = _cmdline->get_val((char *)"protobuf_in_filename");
      protobuf_out_filename = _cmdline->get_val((char *)"protobuf_out_filename");

      //ChipState is the top-level protobuf entity for now.
      //In future we will probably have SimState, which will encapsulate
      //multiple ChipStates, a HostState, and maybe some other metadata. 
      chip_state = new rigelsim::ChipState();
      cp.chip_state = chip_state;

      if(!protobuf_in_filename.empty())
      {
        using namespace google::protobuf::io;
        int fd = open(protobuf_in_filename.c_str(),
                      O_RDONLY, 
                      S_IRUSR); 
        if (fd == -1)
        {
          throw "open failed on input file"; 
        } 
        FileInputStream file_stream(fd);
        GzipInputStream gzip_stream(&file_stream, GzipInputStream::AUTO); 
        if (!chip_state->ParseFromZeroCopyStream(&gzip_stream)) { 
          std::cerr << "Failed to parse input protobuf file\n"; 
        }
        printf("Input protobuf state:\n\n\n");
        printf("%s",chip_state->DebugString().c_str());
        close(fd);
      }
    }
  
    void do_shell_loop() {
      shell->DoInteractiveShellLoop();
    };


    ////////////////////////////////////////////////////////////////////////////////
    // clock_sim()
    ////////////////////////////////////////////////////////////////////////////////
    // Clock all of the subblocks of the design
    // FIXME: only clock what we have, not what we might have...
    ////////////////////////////////////////////////////////////////////////////////
    void clock_sim() {
      
        using namespace rigel;
        using namespace rigel::cache;
      
      
        // Clock the profiler.
        ProfileStat::PerCycle();
      
        // Clock the hardware task queues, if necessary.
        GlobalTaskQueue->PerCycle();
      
        // Clock the memory timing model.
        memory_timing->PerCycle();

        bool halted = false;
        halted = chip_->PerCycle();

        // Check for all cores being halted.
        if (halted) {
          throw ExitSim((char *)"System-wide halt"); 
        }

        // Print the heartbeat information.
        if (rigel::CURR_CYCLE % rigel::HEARTBEAT_INTERVAL == 0) {
          fprintf(stderr, "CYCLE: %12"PRIu64" (COMMITS) ALU: %12"PRIu64" FPU: %12"PRIu64"\n",
            rigel::CURR_CYCLE,
            profiler::stats[STATNAME_COMMIT_FU_ALU].total(),
            profiler::stats[STATNAME_COMMIT_FU_FPU].total()
            );
          if (_cmdline->get_val_bool((char *)"heartbeat_print_pcs")) { heartbeat(); }
        }

    }


    ////////////////////////////////////////////////////////////////////////////////
    /// helper_heartbeat()
    /// Print the heartbeat information for all of the cores.  Input parameter is
    /// the array of tiles for the simulation.
    ////////////////////////////////////////////////////////////////////////////////
    void heartbeat() {
      std::cerr << "Heartbeat: Current PCs:";
      chip_->Heartbeat();
    }

    Chip* chip() { return chip_; }

    void printHierarchy() {
      // TODO: list of children not just chip
      chip_->printHierarchy();
    }

  /// private methods
  private:

    ////////////////////////////////////////////////////////////////////////////
    // initializers for member classes
    ////////////////////////////////////////////////////////////////////////////
    
    /// init_sim
    void init_sim(rigel::ConstructionPayload &cp) {
      init_syscall(cp);
    }

    /// init_chip
    /// instantiate and setup/configure the Chip object
    void init_chip(rigel::ConstructionPayload &cp) {
      // the chip has no parent, it's the root object
      cp.parent = NULL;
      cp.component_name = "Chip";
      cp.component_index = 0;
      chip_ = new Chip(cp);
      chip_->setRootComponent();       // designate this as a root component
    }

    /// memory model initialization helper
    void init_memory(rigel::ConstructionPayload &cp) {

      cp.memory_state = chip_state->mutable_memory(); //FIXME should this go here?

      //Initialize backing store
      backing_store = new rigel::GlobalBackingStoreType(cp, 0); //0-fill
      rigel::GLOBAL_BACKING_STORE_PTR = backing_store;
      cp.backing_store = backing_store;

      //Load binary into backing store
      ELFAccess Elf;
      Elf.LoadELF(_cmdline->get_val((char *)"objfile"), backing_store);
      //Initialize timing model
      memory_timing = new rigel::MemoryTimingType(true); //we *do* want collision checking
      cp.memory_timing = memory_timing;
    };

    /// Setup DRAM parameters
    void setup_dram() {

      using namespace rigel::DRAM;

      // Default is 1 controller and 4 associated G$ banks per tile, user can specify a specific number
      // with -memchannels <N> on the command line
      int numMemoryControllers = _cmdline->get_val_int((char *)"MEMCHANNELS");
    
      if(numMemoryControllers == -1) {
        CONTROLLERS = rigel::NUM_TILES;
      } else {
        //cannot be negative
        if(numMemoryControllers <= 0) {
					std::cerr << "Error: -memchannels value (" << numMemoryControllers << ") must be > 0\n";
          assert(0);
        }
        //If none of the above asserts fired, the value given is OK.
        CONTROLLERS = numMemoryControllers;
      }
    
      rigel::NUM_GCACHE_BANKS = rigel::cache::GCACHE_BANKS_PER_MC * CONTROLLERS;
      ROWS = 4096; // FIXME: constant values :'( (maybe this is ok, comment me)
    
      // The DRAM rank bits "fill in" whatever the other bit ranges do
      // not cover.  They go in between row and bank bits (see address_mapping.h)
      if(ALWAYS_4GB) {
        RANKS = (unsigned int)((uint64_t)(1ULL << 32) / CONTROLLERS / BANKS / ROWS / COLS / COLSIZE);
      } else {
        RANKS = PHYSICALLY_ALLOWED_RANKS;
      }
      ROWS = ceil(((double)(1ULL << 32)) / ((double)CONTROLLERS * BANKS * RANKS * COLS * COLSIZE));
    }

    
    virtual void save_state() const {
      backing_store->save_state();
      chip_->save_state();
      rigelsim::HostState *hs = chip_state->mutable_host();

      //Get CWD
      char cwdtemp[1024];
      char *result = getcwd(cwdtemp, 1024); //FIXME Handle situation where dirname is too long to fit?
      assert(result == cwdtemp);
      hs->set_current_working_dir(cwdtemp);
      //Save target FD metadata
      syscall->save_to_protobuf(hs->mutable_files());

      //Save host and target RNG state
      std::stringstream rng;
      rng << rigel::RNG;
      hs->mutable_host_rng()->set_state(rng.str());

      std::stringstream trng;
      trng << rigel::TargetRNG;
      hs->mutable_target_rng()->set_state(trng.str());
    }

    virtual void restore_state() {

      backing_store->restore_state();
      //FIXME Need to use the mutable_ variants bc we're not making copies of the TFDS'.
      syscall->restore_from_protobuf(chip_state->mutable_host()->mutable_files());

      //Restore host and target RNG state
      std::stringstream rng(chip_state->host().host_rng().state());
      rng >> rigel::RNG;

      std::stringstream trng(chip_state->host().target_rng().state());
      trng >> rigel::TargetRNG;
    }

  private:

    /// private members 

    CommandLineArgs   * _cmdline;
    InteractiveShell  * shell;

    /// chip component
    Chip              * chip_;
    rigel::MemoryTimingType *memory_timing;
    rigel::GlobalBackingStoreType *backing_store;

    Syscall             *syscall;

    /// for checkpointing
		rigelsim::ChipState *chip_state;
    std::string protobuf_out_filename;
    std::string protobuf_in_filename;

};

#endif
