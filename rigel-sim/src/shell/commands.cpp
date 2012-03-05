
#include "shell/shell.h"
#include "util/util.h"
#include "sim/componentbase.h"
#include <string>
#include <vector>
#include <cstdlib>

/// shell commands -- static member 
std::map<std::string,command_t*> InteractiveShell::commands;

///
/// command handlers
///

const int 
InteractiveShell::Help( std::vector<std::string> const &tokens ) {

  std::map<std::string,command_t*>::iterator i;

  printSeparator();

  for (i=commands.begin(); i!= commands.end(); ++i) {
    PrintCommand(i->second);   
  }

  return 1;
};

const int 
InteractiveExit( std::vector<std::string> const &tokens ) {
  printf("%s \n",__func__);
  throw ExitSim((char *)"User Exited Simulation");
  return 1;
};

const int 
InteractiveShell::Step( std::vector<std::string> const &tokens ) {
  if (tokens.size() > 1) {
    int c = atoi(tokens[1].c_str());
    break_cycle = rigel::CURR_CYCLE + c;
  } else { // step one cycle if nothing specified
    break_cycle = rigel::CURR_CYCLE + 1;
  }
  //DPRINT("break at %llu\n",break_cycle);
  return 0;
};


const int 
InteractiveShell::Continue( std::vector<std::string> const &tokens ) {
  printf("%s \n",__func__);
  assert(0&&"unimplemented");
  return 0;
};

const int 
InteractivePrintHierarchy( std::vector<std::string> const &tokens ) {
  printf("%s \n",__func__);
  for (int i = 0; ComponentBase::rootComponent(i) != 0; ++i) {
    ComponentBase::rootComponent(i)->printHierarchy(); 
  }
  printf("\n");
  return 1;
};

const int 
InteractiveDumpHierarchy( std::vector<std::string> const &tokens ) {
  printf("%s \n",__func__);
  for (int i = 0; ComponentBase::rootComponent(i) != 0; ++i) {
    ComponentBase::rootComponent(i)->dumpHierarchy(); 
  }
  printf("\n");
  return 1;
};

const int 
InteractiveDumpComponent( std::vector<std::string> const &tokens ) {
  printf("%s \n",__func__);
  for(size_t i=1; i<tokens.size(); i++) {
    //printf("%zu: %s\n",i,tokens[i].c_str());
    int c = atoi(tokens[i].c_str());
    if (ComponentBase::component(c)) {
      ComponentBase::component(c)->Dump();
    } else {
      printf("Invalid component ID (%s,%d) supplied\n", tokens[i].c_str(),c);
    }
  }
  printf("\n");
  return 1;
};

const void
InteractiveShell::PrintCommand( command_t* const c ) {
  printf("%-12s %s\n", 
    c->cmd.c_str(), 
    c->help_string.c_str()
  );
}

void
InteractiveShell::InitCommands() {

  AddCommand( new command_t("help",     1, "h", &(InteractiveShell::Help), "prints help" ) );
  AddCommand( new command_t("exit",     1, "q", &InteractiveExit, "exits the simulator" ) );
  AddCommand( new command_t("step",     1, "s", &InteractiveShell::Step, "step simulator" ) );
  AddCommand( new command_t("continue", 1, "c", &InteractiveShell::Continue, "leave interactive mode" ) );
  AddCommand( new command_t("dc",       1, "",  &InteractiveDumpComponent, "calls component's dump method" ) );
  AddCommand( new command_t("dh",       1, "",  &InteractiveDumpHierarchy, "calls dump on hierarchy" ) );
  AddCommand( new command_t("ph",       1, "",  &InteractivePrintHierarchy, "prints the Component hierarchy" ) );

};



