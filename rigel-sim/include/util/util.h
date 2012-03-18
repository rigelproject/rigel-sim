#ifndef __UTIL_H__
#define __UTIL_H__
////////////////////////////////////////////////////////////////////////////////
// util.h
////////////////////////////////////////////////////////////////////////////////
//
// Class declaration for utility functions used in RigelSim such as timers and
// system exit functions.  Metafunctionality... This include is safe to include
// elsewhere since it does not include any files itself.
//
////////////////////////////////////////////////////////////////////////////////

#include "sim.h" //FIXME this would seem to violate "does not include any files itself" :)
#include <cstdlib>
#include <cassert>
#include <string>

//Use e.g. "STRINGIZE(__LINE__)" to emit the current line as a string literal
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

////////////////////////////////////////////////////////////////////////////////
// Class ExitSim
////////////////////////////////////////////////////////////////////////////////
//
// Class for exiting early. Dumps a reason for exiting to the screen.
// TODO: move to somewhere better than catch-all util file (own file?)
//
////////////////////////////////////////////////////////////////////////////////
class ExitSim {
	public:

	/* Default exit point for simulation */
	ExitSim(const char *s) : reason(s) {
		std::cerr << "Simulation exiting.  Reason: " << s << "\n";
	}
	ExitSim(const char *s, const int err) : reason(s) {
		std::cerr << "!!!ERROR!!!! Simulation exiting.  Reason: " << s << "\n";
		exit(err);
	}
	/* Normal pre-simulation exit */
	ExitSim(int err) : reason("SUCCESS") {
		exit(err);
	}
  const std::string reason;
};

////////////////////////////////////////////////////////////////////////////////
// Class SimTimer
////////////////////////////////////////////////////////////////////////////////
//
// SimTimer is used to track the number of cycles a particular object is live in
// the simulator.
//
////////////////////////////////////////////////////////////////////////////////
class SimTimer
{
  //public member functions
	public:
		SimTimer()
		{
			clear();
		}
		void StartTimer()
		{
      if (is_running) {
	DEBUG_HEADER();
        fprintf(stderr, "[BUG] Trying to start a running timer!\n");
//FIXME make this check more robust
#ifdef __linux__
        void *array[20];
        size_t size;
        size = backtrace(array, 20);
        // Print out the backtrace and die.
        backtrace_symbols_fd(array, size, 2);
#endif
        assert(0);
      }
			last_start = rigel::CURR_CYCLE;
			is_running = true;
		}
		void StopTimer()
		{
      if (!is_running) {
	DEBUG_HEADER();
        fprintf(stderr, "[BUG] Trying to stop a non-running timer!\n");
//FIXME make this check more robust
#ifdef __linux__
        void *array[20];
        size_t size;
        size = backtrace(array, 20);
        // Print out the backtrace and die.
        backtrace_symbols_fd(array, size, 2);
#endif
        assert(0);
      }
			accum_time += (rigel::CURR_CYCLE - last_start);
			is_running = false;
		}
		void ResetTimer()
		{
			clear();
		}
		uint64_t GetRunningTime() const
		{
                    if(is_running)
			return (accum_time + (rigel::CURR_CYCLE - last_start));
                    else
                        return accum_time;
		}
		uint64_t GetTime() const
		{
      if (is_running != false) {
        DEBUG_HEADER();
        fprintf(stderr, "[BUG] Trying to get the time of a non-running timer!\n");
//FIXME make this check more robust
#ifdef __linux__
        void *array[20];
        size_t size;
        size = backtrace(array, 20);
        // Print out the backtrace and die.
        backtrace_symbols_fd(array, size, 2);
#endif
        assert(0);
      }
			return accum_time;
		}
    bool GetIsRunning() const { return is_running; }

  //private member functions
	private:
		void clear()
		{
			accum_time = 0;
			last_start = 0;
			is_running = false;
		}

  //private data members
	private:

		uint64_t accum_time;
		uint64_t last_start;
		bool is_running;
};

////////////////////////////////////////////////////////////////////////////////
// Class CommandLineArgs
////////////////////////////////////////////////////////////////////////////////
//
// Handles parsing and using command line parameters to the simulator,
// which are used to set system parameters and options.
//
////////////////////////////////////////////////////////////////////////////////
class CommandLineArgs
{
  //public member functions
	public:
		static std::string exec_name;
		CommandLineArgs();
		CommandLineArgs(int argc, char * argv[]);

		bool get_val_bool(char *s);
		int get_val_int(char *s);
		std::string get_val(char *s);

		static void print_help();


		class CommandLineError {
			public:
				CommandLineError() {
					std::cerr << "Bad Command Line\n";
					CommandLineArgs::print_help();
					exit(1);
				}
				CommandLineError(std::string s) {
					std::cerr << "Bad Command Line: " << s << "\n";
					CommandLineArgs::print_help();
					exit(1);
				}
		};

  //private data members
	private:
		std::map<std::string, std::string> cmdline_table;
};

////////////////////////////////////////////////////////////////////////////////
// Class ELFAccess
////////////////////////////////////////////////////////////////////////////////
//
// Used for loading the binary Elf file into the simulator.
//
////////////////////////////////////////////////////////////////////////////////
class ELFAccess {
	public:
		void LoadELF(std::string bin_name, rigel::GlobalBackingStoreType *mem);

};

////////////////////////////////////////////////////////////////////////////////
// Class CallbackInterface
////////////////////////////////////////////////////////////////////////////////
//
// Used for providing thin callback interfaces to reduce coupling.
// First used in RigelSim to decouple DRAMController and GlobalCache.
//
////////////////////////////////////////////////////////////////////////////////
class CallbackInterface {
public:
  // For now, just declare a pure virtual (int, int) callback.
  // TODO: Generalize this or use a callback/signals-and-slots library.
  // Examples are libsigc++ (LGPL), Boost.Signals (Boost, similar to BSD/MIT),
  // Qt Signals & Slots (LGPL)
  virtual void callback(int, int, uint32_t) = 0;
};

////////////////////////////////////////////////////////////////////////////////
// tokenize
////////////////////////////////////////////////////////////////////////////////
// Parses the input string, and breaks up into tokens. Used in getting commands
// from the user in interactive mode.
// Inputs: string, delimiters
// Outputs: tokens
/* Borrowed from:
 * http://www.oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html
 */
class StringTokenizer {
  public:
  static void tokenize(const std::string &str, std::vector<std::string> &tokens,
                       const std::string &delimiters = " ")
  {
		std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
		std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos) {
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      lastPos = str.find_first_not_of(delimiters, pos);
      pos = str.find_first_of(delimiters, lastPos);
    }
  }
};

#endif
