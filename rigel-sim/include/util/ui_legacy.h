////////////////////////////////////////////////////////////////////////////////
// ui_legacy.h
////////////////////////////////////////////////////////////////////////////////
//
//  Definition of the UserInterfaceLegacy class.  The UI is used for displaying
//  pipeline state when stepping through execution in interactive model.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _UI_H_
#define _UI_H_

class CoreInOrderLegacy;

////////////////////////////////////////////////////////////////////////////////
/// UserInterface
/// Class is used for diplaying core pipeline state to the screen
////////////////////////////////////////////////////////////////////////////////
class UserInterfaceLegacy
{
	public:
		UserInterfaceLegacy(CoreInOrderLegacy ** c, bool interactive) :
			cores(c), interactive(interactive), custom_break(false), skip_count(0)
		{
		}

    //Check breakpoint on fetch stage
		void run_interface(InstrSlot instrs[rigel::ISSUE_WIDTH]);

    //Display help meu
		void print_help();

    //Function checks if simulator is running in interactive mode
    bool is_interactive() { return interactive; }

    //Function returns how many instructions are being skipped
    uint32_t get_skip_count() { return skip_count; }

  public:                
		CoreInOrderLegacy ** cores;
  
  private:
    //Running in interactive mode
		bool interactive;
    bool custom_break;
    //user defined break point
    uint32_t break_PC;
    //instructions to skip
		uint32_t skip_count;
	
};
#endif
