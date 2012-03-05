#ifndef __SHELL_H__
#define __SHELL_H__

#include "util/linenoise.h"
#include "util/util.h"
#include <inttypes.h>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <inttypes.h>
#include <cstring>

/// command string is key used for storing command_t structs
struct command_t{

  command_t( const char* cmd_,
             int num_tokens_,
             const char* abbr_,
             const int (*handler_) (std::vector<std::string> const&),
             const char* help_str
  ) : 
      cmd(cmd_),
      num_tokens(num_tokens_),
      abbr(abbr_),
      handler( handler_ ),
      help_string(help_str)
  { 
  };

  /// command name string
  const std::string cmd;
  /// required number of tokens
  int num_tokens;
  /// abbreviated command
  const std::string abbr;
  /// handler function pointer
  /// handlers should return true to get another command
  /// false to leave the shell
  const int (*handler) (std::vector<std::string> const &tokens);
  /// help information for this command
  const std::string help_string;
};

///////////////////////////////////////////////////////////////////////////////
/// implements an interactive commandline shell
///////////////////////////////////////////////////////////////////////////////
class InteractiveShell {

  public: 

    /// constructor
    InteractiveShell() 
      : last_tokens(1,"") 
    {
      InitCommands();
    }

    /// command processing loop
    /// process commands until an exit signal is received
    /// NOTE: Commandline Interactive Mode Operation i
    ///       ???
    /// NOTE: BreakPoints
    ///       ???
    void DoInteractiveShellLoop() {
        
      if (!rigel::CMDLINE_INTERACTIVE_MODE) {
        return;
      }

      if (break_cycle > rigel::CURR_CYCLE) {
        return;
      }

      printSeparator();
      printf(">>> Begin Cycle: %"PRIu64"\n", rigel::CURR_CYCLE);
      while( ProcessLine() ) {
        continue;
      }

    }

    /// consume a line from the interactive commandline
    /// tokenize and call command handler
    /// return 0 if shell loop should exit
    int ProcessLine() {

      std::vector<std::string> tokens;
      std::string delimiters(" ");
      std::string error_msg(" ");
      
      // Display Prompt
      char *input = linenoise(" # ");
      assert(input != NULL && "linenoise ran out of memory\n");

      // if nothing entered, rerun the last command
      if (strlen(input) == 0) {
        DPRINT(true, "repeating last command %s\n", last_tokens[0].c_str());
        tokens = last_tokens; // last command repeat
      } else {
        const std::string buf(input);
        /* Parse new command */
        StringTokenizer::tokenize(buf, tokens, delimiters);
        // save new tokens for next time
        last_tokens = tokens;
        // add to history
        linenoiseHistoryAdd(input);
      }

      // I don't like this, linenoise allocating memory...
      free(input);
     
      std::map<std::string,command_t*>::iterator cmd;
      // now that we have tokens, process the commandline 
      // if the command is not found, notify
      cmd = commands.find(tokens[0]);
      if( cmd == commands.end() ) {
        printf("invalid command \n"); 
      } else {
        /// call the handler
        int retval = (cmd->second->handler)(tokens);
        return retval;
      }

      // by default, don't exit interactive shell
      return 1;

    }

    /// register a new command
    void AddCommand(command_t* c) {
      // long form command
      if (commands.find(c->cmd) != commands.end()) {
        printf("error, %s already exists in commands[]!\n", c->cmd.c_str());
      } else {
        commands[c->cmd] = c;
      }
    }

    /// command handlers
    static const int Help( std::vector<std::string> const &tokens );
    static const int Step( std::vector<std::string> const &tokens );
    static const int Continue( std::vector<std::string> const &tokens );

  /// private methods
  private:

    void InitCommands();
    static const void PrintCommand(command_t* c);

    static void printSeparator() {
      printf("=====================================================================\n");
    }

  /// private member data
  private:
    
    std::vector<std::string> last_tokens; /// last set of tokens

    /// map holding available commands and handlers
    static std::map<std::string,command_t*>      commands; 

    static uint64_t break_cycle;

};


#endif
