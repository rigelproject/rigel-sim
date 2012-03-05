#ifndef __SYSCALL_H__
#define __SYSCALL_H__
////////////////////////////////////////////////////////////////////////////////
// syscall.h
////////////////////////////////////////////////////////////////////////////////
//
// System call information including the name, documentation of what each call
// does, and a textual identifier.  These are triggered by executing a syscall
// instruction inside the simulation.
//
////////////////////////////////////////////////////////////////////////////////

#include "sim.h"
#include "util.h"
#include <string>
#include <google/protobuf/repeated_field.h>

#include "sim.h"
#include "rigelsim.pb.h"

typedef ::google::protobuf::RepeatedPtrField< rigelsim::TargetFileDescriptorState > RepeatedTFDS;

#define DB_SYSCALL 0

class Syscall;
class BackingStoreType;

namespace rigel {
  void RigelSYSCALL(uint32_t addr, 
                    Syscall *syscall_handler, 
                    rigel::GlobalBackingStoreType *mem) ;
}

 ///////////////////////////////////////////////////////////////////
 // syscall_name
 ///////////////////////////////////////////////////////////////////
 // Enumerated type used for keeping track of the which system call
 // is being made by simulated code
 ///////////////////////////////////////////////////////////////////
typedef enum {
  // To reduce the cost of file I/O, the app can issue this system call with a
  // file descriptor of an open file, a pointer where to put the data in the
  // target's address space, the offset, and the length in bytes of files to
  // read in.  That data will be automagically put into the target's memory
  // space starting at the pointer passed in.
  SYSCALL_FILEMAP = 0x12,
  SYSCALL_FILESLURP = 0x13,
  SYSCALL_FILEDUMP= 0x14,

  // Cycle Timers
  // START_TIMER: logs start time for requested timer
  // If timer is not in use, clears and logs start time
  // If timer is already started, has no effect (indempotent)
  SYSCALL_STARTTIMER = 0x20,

  // GET_TIMER: returns the number of cycles elapsed since initialized
  SYSCALL_GETTIMER   = 0x21,

  // STOP_TIMER: stops th
  // e timer from counting, but does not clear
  // timer state (subsequent GET_TIMER calls return last count)
  // If the timer was not running, has no effect
  SYSCALL_STOPTIMER  = 0x22,
  SYSCALL_CLEARTIMER  = 0x23,

  SYSCALL_RANDOMF = 0x3D,
  SYSCALL_RANDOMI = 0x3E,
  SYSCALL_RANDOMU = 0x3F,
  SYSCALL_SRAND = 0x40,

  // Standard (ish) UNIX system calls.  RTFM for details.
  SYSCALL_WRITE = 0x83,
  SYSCALL_READ = 0x82,
  SYSCALL_OPEN = 0x80,
  SYSCALL_CLOSE = 0x81,
  SYSCALL_LSEEK = 0x84,
  SYSCALL_UNLINK = 0x85,
  SYSCALL_LINK = 0x86,
  SYSCALL_STAT = 0x87,
  SYSCALL_FSTAT = 0x88,
  SYSCALL_LSTAT = 0x89,
  SYSCALL_CHMOD = 0x90,
  SYSCALL_RENAME = 0x91,
  SYSCALL_MKSTEMP= 0x92,
  SYSCALL_FTRUNCATE = 0x93
} syscall_name_t;

extern const char *SYSCALL_NAME_TABLE[];

typedef union { float f; uint32_t u; int32_t i; } fui_union;

struct syscall_struct {
   uint32_t syscall_num;
   fui_union result;
   fui_union arg1;
   fui_union arg2;
   fui_union arg3;
};


 ///////////////////////////////////////////////////////////////////
 // class syscall
 ///////////////////////////////////////////////////////////////////
 // Contains the IO calls and other standard systems calls used
 ///////////////////////////////////////////////////////////////////
class Syscall {
  public:
    Syscall(rigel::GlobalBackingStoreType *mmodel);
    ~Syscall();
    /// doSystemCall
    /// passthrough to local system call implementations
    /// throw an exception on an illegal system SYSCALL
    /// optionally writes return value into syscall_struct via reference
    void doSystemCall( syscall_struct &sc_data ) {

      DPRINT(DB_SYSCALL,"syscall '%s'(0x%x){%08x,%08x,%08x}\n",
				SYSCALL_NAME_TABLE[sc_data.syscall_num],
        sc_data.syscall_num, sc_data.arg1.u, sc_data.arg2.u, sc_data.arg3.u);

      switch (sc_data.syscall_num) {
        case (SYSCALL_FILEMAP):    syscall_filemap(sc_data);    break;
        case (SYSCALL_FILESLURP):  syscall_fileslurp(sc_data);  break;
        case (SYSCALL_FILEDUMP):   syscall_filedump(sc_data);   break;

        case (SYSCALL_WRITE):      syscall_write(sc_data);      break; 
        case (SYSCALL_READ):       syscall_read(sc_data);       break; 
        case (SYSCALL_OPEN):       syscall_open(sc_data);       break;
        case (SYSCALL_CLOSE):      syscall_close(sc_data);      break;
        case (SYSCALL_LSEEK):      syscall_lseek(sc_data);      break;
        case (SYSCALL_STARTTIMER): syscall_starttimer(sc_data); break;
        case (SYSCALL_GETTIMER):   syscall_gettimer(sc_data);   break;
        case (SYSCALL_STOPTIMER):  syscall_stoptimer(sc_data);  break;
        case (SYSCALL_CLEARTIMER): syscall_cleartimer(sc_data); break;

        case SYSCALL_RANDOMF:     syscall_randomf(sc_data);     break;
        case SYSCALL_RANDOMI:     syscall_randomi(sc_data);     break;
        case SYSCALL_RANDOMU:     syscall_randomu(sc_data);     break;
        case SYSCALL_SRAND:       syscall_srand(sc_data);       break;

        case (SYSCALL_UNLINK): // unlink()
        case (SYSCALL_LINK): // link()
        case (SYSCALL_STAT): // stat()
        case (SYSCALL_FSTAT): // fstat()
        case (SYSCALL_LSTAT): // lstat()
        case (SYSCALL_CHMOD): // chmod()
        case (SYSCALL_RENAME): // rename()
        case (SYSCALL_MKSTEMP): //  mkstemp()
        case (SYSCALL_FTRUNCATE): // ftruncate()
        default:
					fprintf(stderr, "Error, syscall %d not yet implemented\n", sc_data.syscall_num);
          throw ExitSim("SYSCALL not yet implemented!");
          break;
    }
  } // end doSystemCall()

  /// for saving, restoring state (checkpointing)
  void save_to_protobuf(RepeatedTFDS *rtfds);
  void restore_from_protobuf(RepeatedTFDS *rtfds);

  /// private syscall handlers
  /// the syscall struct specifies the syscall to be handled, so don't expose
  /// these names externally 
  /// handlers of form: syscall_SYSCALLNAME( reference to syscall_struct )
  private:

    void syscall_write(syscall_struct &syscall_data);
    void syscall_read(syscall_struct &syscall_data);
    void syscall_open(syscall_struct &syscall_data);
    void syscall_close(syscall_struct &syscall_data);
    void syscall_lseek(syscall_struct &syscall_data);
    void syscall_unlink(syscall_struct &syscall_data);
    void syscall_link(syscall_struct &syscall_data);
    void syscall_stat(syscall_struct &syscall_data);
    void syscall_fstat(syscall_struct &syscall_data);
    void syscall_lstat(syscall_struct &syscall_data);
    void syscall_chmod(syscall_struct &syscall_data);
    void syscall_rename(syscall_struct &syscall_data);
    void syscall_mkstemp(syscall_struct &syscall_data);
    void syscall_ftruncate(syscall_struct &syscall_data);

    void syscall_strtof(syscall_struct &syscall_data);
    void syscall_ftostr(syscall_struct &syscall_data);
    void syscall_filemap(syscall_struct &syscall_data);
    void syscall_fileslurp(syscall_struct &syscall_data);
    void syscall_filedump(syscall_struct &syscall_data);

    void syscall_starttimer(syscall_struct &syscall_data);
    void syscall_gettimer(syscall_struct &syscall_data);
    void syscall_stoptimer(syscall_struct &syscall_data);
    void syscall_cleartimer(syscall_struct &syscall_data);

    // Randomization Functions
    void syscall_srand(syscall_struct &syscall_data);
    void syscall_randomf(syscall_struct &syscall_data);
    void syscall_randomi(syscall_struct &syscall_data);
    void syscall_randomu(syscall_struct &syscall_data);

  /// other private methods
  /// helper methods, called by the syscall handlers?
  private:

    std::string read_string(uint32_t address);

    void read_memory(void *dst, uint32_t address, uint32_t len);
    void write_memory(const void *src, uint32_t address, uint32_t len);

    uint32_t syscall_read4(const char *fmt, syscall_struct &syscall_data);
    uint32_t syscall_read1(const char *fmt, syscall_struct &syscall_data);

    template <typename type>
    uint32_t syscall_write(const char *fmt, syscall_struct &syscall_data);
    uint32_t syscall_write1(const char *fmt, syscall_struct &syscall_data);

    rigel::GlobalBackingStoreType *backing_store; //
};
#endif
