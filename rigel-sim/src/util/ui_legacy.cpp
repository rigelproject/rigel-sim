////////////////////////////////////////////////////////////////////////////////
// ui.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the code related to the interface a user sees when
//  running a benchmark in interactive mode ('-i' option on the command line to
//  RigelSim).  Breakpoint management is also handled here.
////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for printf, fprintf, sscanf, etc
#include <stdlib.h>                     // for atoi, strtoul, free
#include <cstring>                      // for NULL, strlen
#include <iomanip>                      // for operator<<, std::setw, _std::setw, etc
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <string>                       // for basic_string, char_traits, etc
#include <vector>                       // for vector
#include "core.h"           // for CoreInOrderLegacy
#include "util/debug.h"          // for DebugSim, GLOBAL_debug_sim
#include "define.h"         // for HEX_std::cout, InstrSlot
#include "instr.h"          // for InstrLegacy
#include "util/linenoise.h"      // for linenoise, etc
#include "core/regfile_legacy.h"        // for RegisterFile, etc
#include "core/scoreboard.h"     // for ScoreBoard
#include "sim.h"            // for CORES_PER_CLUSTER, etc
#include "util/ui_legacy.h"             // for UserInterfaceLegacy 
#include "util/util.h"           // for ExitSim, StringTokenizer
#include "memory/backing_store.h" // for GlobalBackingStoreType definition

extern "C" {
}

// Defined in sim.h
namespace rigel {
  bool BREAKPOINT_PENDING = false;
}

////////////////////////////////////////////////////////////////////////////////
// UserInterfaceLegacy::run_interface
////////////////////////////////////////////////////////////////////////////////
// Get input from the user to control interactive mode
// Checks for breakpoints while in interactive mode, also allows 
// the user to skip instructions. 
// Input: instrs[], the instructions fetched.
void 
UserInterfaceLegacy::run_interface(InstrSlot instrs[rigel::ISSUE_WIDTH]) 
{
  // Check on pending breakpoint even if non-interactive.  Dump to interactive
  // mode as soon as we are sure the breakpoint is non-speculative.
  
  static std::vector<std::string> last_tokens;
  //char *up_arrow = "^[[A";

  if (rigel::BREAKPOINT_PENDING) {
    std::cerr << "PENDING!!!!" << "\n";
    // XXX: May want to reset skip count?
    rigel::BREAKPOINT_PENDING = false;
    interactive = true;

  } else {
    //if (!interactive) { return; }
    if (!rigel::CMDLINE_INTERACTIVE_MODE) { return; }
    // For custom breakpoints at a specific PC keep on fast forwarding until we
    // hit an instruction with the PC where we need to break
    if (custom_break) {
      for(int i=0;i<rigel::ISSUE_WIDTH;i++){
        if(instrs[i] == rigel::NullInstr) skip_count = 1;
        else if(instrs[i]->get_currPC() != break_PC) skip_count = 1;
        else { skip_count = 0; break; }
      }
    }
    /* Allow user to fast forward */
    if (skip_count) {
      skip_count--;
      goto done_wb_ui;
    }
  }
  if (custom_break)
    std::cout << "Encountered breakpoint at PC: 0x" << HEX_COUT << break_PC << " Cycle " << std::dec << rigel::CURR_CYCLE << "\n";
  while(1) 
  {
    custom_break = false;
    std::vector<std::string> tokens;
		std::string delimiters(" ");
		std::string error_msg(" ");

    //Display Prompt
    char *input = linenoise(" # ");
		assert(input != NULL && "linenoise ran out of memory\n");
    /* User did not type anything */
    if (strlen(input) == 0) { 
      printf("repeating last command\n");
      tokens = last_tokens; // last command repeat
      //continue; 
    } else {
      const std::string buf(input);
      /* Parse new command */
      StringTokenizer::tokenize(buf, tokens, delimiters);
      // save new tokens for next time
      last_tokens = tokens;
      // add to history
      linenoiseHistoryAdd(input);
    }
    free(input);

    if (!tokens[0].compare("w") || 
        !tokens[0].compare("watch"))  
    {
      if (!tokens.size() == 2 ) { 
        error_msg = "Invalid number of arguments";
        goto invalid_cmd; 
      }
      int  core_num = atoi(tokens[1].c_str());
      rigel::INTERACTIVE_CORENUM=core_num;
      goto done_wb_ui;

    }

    if (!tokens[0].compare("m") || 
        !tokens[0].compare("mem"))  
    {
      if (!(tokens.size() == 5 || tokens.size() == 4)) { 
        error_msg = "Invalid number of arguments";
        goto invalid_cmd; 
      }
      int  core_num = atoi(tokens[2].c_str());
      uint32_t addr;
      if (tokens[3].substr(0, 2) == std::string("0x")) {
        addr = strtoul(tokens[3].c_str(), (char **)NULL, 16);
      } else {
        addr = strtoul(tokens[3].c_str(), NULL, 0);
      }
      uint32_t count = 1;

      if (core_num < 0 || core_num > rigel::CORES_PER_CLUSTER) {
        error_msg = "Invalid core number";
        goto invalid_cmd;
      }
//      if (addr > (uint32_t)cores[core_num]->getMemoryModel()->size()) {
//        error_msg = "Invalid address";
//        goto invalid_cmd;
//      }
      if (!tokens[1].compare("r")) {
        if (!(tokens.size() == 5 || tokens.size() == 4)) { 
          error_msg = "Invalid number of arguments";
          goto invalid_cmd; 
        }
        if (tokens.size() > 4) {
          count = strtoul(tokens[4].c_str(), NULL, 0);
        }
        for (uint32_t i = 0; i < count; i++) {
          std::cout << "0x" << std::hex << std::setfill('0') << std::setw(8) << addr << ": ";
          uint32_t val;
          val = cores[core_num]->getBackingStore()->read_word(addr);
          std::cout << std::hex << std::setfill('0') << std::setw(8) << val << "\n";
          addr+=4;
        }
      } else if (!tokens[1].compare("w")) {
        if (tokens.size() != 5) {
          error_msg = "Invalid number of arguments";
          goto invalid_cmd; 
        }

        uint32_t data = strtoul(tokens[4].c_str(), NULL, 0);
        cores[core_num]->getBackingStore()->write_word(addr, data);
      } else {
          error_msg = "Invalid command";
          goto invalid_cmd; 
      }
      continue;
    }
    if (!tokens[0].compare("i") ||
        !tokens[0].compare("int") ||
	!tokens[0].compare("interactive")) {
      interactive=true; goto done_wb_ui;
    }

    if (!tokens[0].compare("ni") ||
        !tokens[0].compare("non")) {
      interactive=false; goto done_wb_ui;
    }

    if (!tokens[0].compare("t") ||
        !tokens[0].compare("time")) {
      fprintf(stderr,"%d\n",(int)rigel::CURR_CYCLE); goto done_wb_ui;
    }
    /* Dump the register file for a core */
    if (!tokens[0].compare("r") ||
        !tokens[0].compare("reg"))
    {
      int core_num;
      int tid;
      if(tokens.size() == 3 )  {
        core_num = atoi(tokens[1].c_str());
        tid      = atoi(tokens[2].c_str());
      } else {
        core_num = 0;
        tid = 0;
        printf("core/thread not specified, assuming core %d and thread %d\n", core_num, tid);
      }

      if (core_num < 0 || core_num >= rigel::CORES_PER_CLUSTER) {
        error_msg = "Invalid core number";
        goto invalid_cmd;
      }
      if (tid < 0 || tid >= rigel::THREADS_PER_CORE) {
        error_msg = "Invalid thread number";
        goto invalid_cmd;
      }
      
      printf("Regfile core %d thread %d\n", core_num, tid);
      cores[core_num]->get_regfile(tid)->dump_reg_file(std::cout);
      printf("Accumulator file\n");
      cores[core_num]->get_accumulator_regfile(tid)->dump_reg_file(std::cout);
      // since there is only one of these now, no core_num index
      // TODO: make the function take it as a parameter but just ignore it for
      // future-proof safety
      cores[core_num]->get_sprf(tid)->dump_reg_file(std::cout);
      continue;
    }
    /* Dump the accumulator scoreboard state for a core */
    if (!tokens[0].compare("acc_sb"))
    {
      int core_num;
      int tid;
      if(tokens.size() == 3 )  {
        core_num = atoi(tokens[1].c_str());
        tid = atoi(tokens[2].c_str());
      } else {
        core_num = 0;
        tid = 0;
        printf("core/thread not specified, assuming core %d and thread %d\n", core_num, tid);
      }

      if (core_num < 0 || core_num >= rigel::CORES_PER_CLUSTER) {
        error_msg = "Invalid core number";
        goto invalid_cmd;
      }
      if (tid < 0 || tid >= rigel::THREADS_PER_CORE) {
        error_msg = "Invalid thread number";
        goto invalid_cmd;
      }
      printf("Accumulator Scoreboard (thread %d): \n", tid);
      cores[core_num]->get_ac_scoreboard(tid)->dump();
      continue;
    }
    /* Dump the scoreboard state for a core */
    if (!tokens[0].compare("sb"))
    {
      int core_num;
      int tid;
      if(tokens.size() == 3 )  {
        core_num = atoi(tokens[1].c_str());
        tid = atoi(tokens[2].c_str());
      } else {
        core_num = 0;
        tid = 0;
        printf("core/thread not specified, assuming core %d and thread %d\n", core_num, tid);
      }

      if (core_num < 0 || core_num >= rigel::CORES_PER_CLUSTER) {
        error_msg = "Invalid core number";
        goto invalid_cmd;
      }
      if (tid < 0 || tid >= rigel::THREADS_PER_CORE) {
        error_msg = "Invalid thread number";
        goto invalid_cmd;
      }
      printf("Scoreboard (thread %d): \n", tid);
      cores[core_num]->get_scoreboard(tid)->dump();
      continue;
    }

    /* Dump the cluster cache state for a core */
    if (!tokens[0].compare("ccache")) {
      rigel::GLOBAL_debug_sim.dump_ccache_ctrl();
    }

    // Continue execution until specific PC
    if (!tokens[0].compare("b") ||
       !tokens[0].compare("break"))
    {
      if (tokens.size() != 2) {
        error_msg = "Invalid number of arguments";
        goto invalid_cmd; 
      }
      custom_break = 1;
      int temp_input;
      sscanf(tokens[1].c_str(),"%x",&temp_input);
      break_PC = temp_input & 0xfffffffc;
      //interactive = false;
      goto done_wb_ui;
    }
    // Step through to next instruction and stop at the same core (skip 7)
    if (!tokens[0].compare("n") ||
       !tokens[0].compare("next"))
    {
       if( tokens.size() == 2 )
         //skip_count = atoi(tokens[1].c_str())*rigel::CORES_PER_CLUSTER - 1 ;
         skip_count = atoi(tokens[1].c_str()) -1;
       else
         //skip_count = rigel::CORES_PER_CLUSTER - 1;
         skip_count = 0;
       goto done_wb_ui;
    }
    /* Fast forward n instructions */
    if (!tokens[0].compare("f") ||
        !tokens[0].compare("skip"))
    {
      if ( tokens.size() != 2 ) {
        error_msg = "Invalid number of instructions to skip";
        goto invalid_cmd;
      }
      skip_count = atoi(tokens[1].c_str());
      goto done_wb_ui;
    }
    /* Step to next instruction */
    if (!tokens[0].compare("s") ||
        !tokens[0].compare("step"))
    {
      goto done_wb_ui;
    }
    /* Dump ScoreBoard*/
    if (!tokens[0].compare("sbdump")) 
    {
      for (int i = 0; i < rigel::CORES_PER_CLUSTER; i++) {
        std::cerr << "Core " << std::dec << i << "\n";
        for (int j = 0; j < rigel::THREADS_PER_CORE; j++) {
        std::cerr << "  Thread " << std::dec << j << "\n";
          cores[i]->get_scoreboard(j)->dump();
        }
      }
      goto done_wb_ui;
    }
    /* Continue to the end */
    if (!tokens[0].compare("c") ||
        !tokens[0].compare("cont") ||
        !tokens[0].compare("continue"))
    {
      //this->skip_count = 0xFFFFFFFF;
      interactive = false;
      goto done_wb_ui;
    }
    /* Quit simulation */
    if (!tokens[0].compare("q") ||
        !tokens[0].compare("quit") ||
        !tokens[0].compare("exit"))
    {
      std::cerr << "Exiting simulation." << "\n";
      throw ExitSim("User quit");
    }
    /* Force a checkpoint now */
#if 0
    if (!tokens[0].compare("checkpoint") )
    {
      Cluster *cl = this->cores[0]->get_cluster();
      cl->ckpt->take();
      std::cout << "Checkpoint triggered" << "\n";
      continue;
    }
    /* Force a rollback now */
    if (!tokens[0].compare("rollback") )
    {
      Cluster *cl = this->cores[0]->get_cluster();
      cl->ckpt->rollback(rigel::CURR_CYCLE - 1);
      std::cout << "Rollback triggered" << "\n";
      continue;
    }
#endif
      /* Thread swap */
    if (!tokens[0].compare("t") || 
      !tokens[0].compare("swap"))
    {
      for(int i=0; i < rigel::CORES_PER_CLUSTER; ++i) {
        cores[i]->forceSwap = true;
      }
    }

    /* Help Info */
    if (!tokens[0].compare("h") ||
        !tokens[0].compare("help") ||
        !tokens[0].compare("?"))
    {
      print_help();
      continue;
    }

    invalid_cmd:
    std::cout << error_msg << ". Type 'help' for assistance..." << "\n";
    continue;
  }
done_wb_ui:
//  while (cin.get() != '\n'); 
  return;
}

////////////////////////////////////////////////////////////////////////////////
// UserInterfaceLegacy::print_help()
////////////////////////////////////////////////////////////////////////////////
// prints a "help" block of text at the console when in interactive mode
void UserInterfaceLegacy::print_help() {
  std::cout.setf(std::ios::left);
  std::cout << std::setfill(' ');
  std::cout << "The rigel-sim help menu: " << "\n";
  std::cout.setf(std::ios::left);
  std::cout << std::setw(40) << "  watch <corenum>"   << std::setw(40) << "Set corenum to watch/trace (0 default)" << "\n";
  std::cout << std::setw(40) << "  ni || non "        << std::setw(40) << "disable interactive trace dumps, shuts off trace output" << "\n";
  std::cout << std::setw(40) << "  i || interactive " << std::setw(40) << "enable interactive trace dumps, turn on trace output" << "\n";
  std::cout << std::setw(40) << "  t || time "        << std::setw(40) << "Display simulator cycle number" << "\n";
  std::cout << std::setw(40) << "  break <PC>"        << std::setw(40) << "Continue execution until the specified PC on watched corenum" << "\n";
  std::cout << std::setw(40) << "  help"              << std::setw(40) << "Print help message" << "\n";
  std::cout << std::setw(40) << "  mem r <core> <address> ?count?" << std::setw(40) << 
    "Read simulation memory." << "\n";
  std::cout << std::setw(40) << "  mem w <core> <address> <data>" << std::setw(40) << 
    "Write simulation memory." << "\n";
  std::cout << std::setw(40) << "  n[ext] [num_instr]"  << std::setw(40) <<
    "Skips num_instr cycles on the core where the instruction is entered." << "\n";
  std::cout << std::setw(40) << "  reg <core> <thread>" << std::setw(40) << "Dump register file for core." << "\n";
  std::cout << std::setw(40) << "  sb <core> <thread>"  << std::setw(40) << "Dump scoreboard state for core." << "\n";
  std::cout << std::setw(40) << "  ccache"              << std::setw(40) << "Dump ccache state" << "\n";
  std::cout << std::setw(40) << "  step"                << std::setw(40) << "Execute current instruction." << "\n";
  std::cout << std::setw(40) << "  skip <N>"            << std::setw(40) << "Skip N instructions." << "\n";
  std::cout << std::setw(40) << "  continue"            << std::setw(40) << 
    "Continue to next breakpoint or end."     << "\n";
  std::cout << std::setw(40) << "  swap"                << std::setw(40) << "Force a thread swap." << "\n";
  std::cout << std::setw(40) << "  quit"                << std::setw(40) << "Exit the simulation." << "\n";
  //std::cout.setf(std::ios::right);

}
