////////////////////////////////////////////////////////////////////////////////
// syscalls.cpp
////////////////////////////////////////////////////////////////////////////////
//
// This file contains implementations for various system calls
// supported/emulated/hooked by RigelSim
//
// These include file I/O, float/str conversions, and timers
//
////////////////////////////////////////////////////////////////////////////////

#include <cassert>                     // for assert
#include <fcntl.h>                      // for open, O_CREAT
#include <cstddef>                     // for size_t, NULL
#include <stdint.h>                     // for uint32_t, uint8_t
#include <cstdio>                      // for fprintf, fclose, fopen, etc
#include <cstdlib>                     // for calloc, free, strtof
#include <cstring>                     // for memset
#include <sys/stat.h>                   // for S_IRUSR, S_IWUSR
#include <sys/types.h>                  // for int32_t
#include <unistd.h>                     // for read, write, close, lseek, etc
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <sstream>                      // for ostringstream
#include <string>                       // for string, basic_string, etc
#include <inttypes.h>                   // for PRIu32
#include "define.h"                     // for DEBUG_HEADER
#include "sim.h"                        // for MemoryModelType, etc
#include "util/syscall.h"                    // for Syscall, syscall_struct
#include "memory/backing_store.h"       // for GlobalBackingStoreType definition
#include "RandomLib/Random.hpp"         // for Random
#include "util/target_file_descriptor.h"
#include "rigel_fcntl.h"                // Currently need to peek at a few O_* and S_* macros.
                                        // FIXME this is bad practice, and would go away if we supported
                                        // 3-arg open() (with a 'mode' argument)

// List of textual descriptions of system calls to dump on syscall errors.
const char *SYSCALL_NAME_TABLE[] = {
  "", "", "", "", "", "", "", "", // 0x00-0x07
  "", "", "", "", "", "", "", "", // 0x08-0x0F
  "", "", "filemap", "fileslurp", "filedump", "", "", "", // 0x10-0x17
  "", "", "", "", "", "", "", "", // 0x18-0x1F
  "starttimer", "gettimer", "stoptimer", "cleartimer", "", "", "", "", // 0x20-0x27
  "", "", "", "", "", "", "", "", // 0x28-0x2F
  "", "", "", "", "", "", "", "", // 0x30-0x37
  "", "", "", "", "", "", "", "", // 0x38-0x3F
  "", "", "", "", "", "", "", "", // 0x40-0x47
  "", "", "", "", "", "", "", "", // 0x48-0x4F
  "", "", "", "", "", "", "", "", // 0x50-0x57
  "", "", "", "", "", "", "", "", // 0x58-0x5F
  "", "", "", "", "", "", "", "", // 0x60-0x67
  "", "", "", "", "", "", "", "", // 0x68-0x6F
  "", "", "", "", "", "", "", "", // 0x70-0x77
  "", "", "", "", "", "", "", "", // 0x78-0x7F
  "open", "close", "read", "write", "lseek", "unlink", "link", "stat", // 0x80-0x87
  "fstat", "lstat", "chmod", "rename", "mkstemp", "ftruncate", "", "", // 0x88-0x8F
  "", "", "", "", "", "", "", "", // 0x90-0x97
  "", "", "", "", "", "", "", "", // 0x98-0x9F
};

namespace rigel {
  /// handle a syscall instruction from a core
  void 
  RigelSYSCALL(uint32_t addr, 
               Syscall *syscall_handler, 
               rigel::GlobalBackingStoreType *mem) 
  {
    syscall_struct syscall_data;
    //uint32_t addr = instr->regval(DREG).u32(); // Pointer to syscall_struct
    // Read in the syscall parameters
    // read words 0, 2, 3, and 4 of the __suds_syscall_struct
    // word 1 is reserved for the result
    syscall_data.syscall_num = mem->read_data_word(addr + (0 * sizeof(uint32_t)) );
    syscall_data.arg1.u      = mem->read_data_word(addr + (2 * sizeof(uint32_t) ));
    syscall_data.arg2.u      = mem->read_data_word(addr + (3 * sizeof(uint32_t) ));
    syscall_data.arg3.u      = mem->read_data_word(addr + (4 * sizeof(uint32_t) ));
    DPRINT(DB_SYSCALL,"syscall addr:0x%08x syscall[0x%x] {%08x,%08x,%08x}\n",
      addr, syscall_data.syscall_num, 
      syscall_data.arg1.u, syscall_data.arg2.u, syscall_data.arg3.u);
  
    // call the syscall handler, which just emulates for now
    syscall_handler->doSystemCall(syscall_data);
  
    // record the syscall return value (emulated, just write memory)
    mem->write_word(addr + 1*sizeof(uint32_t), syscall_data.result.u);
  };
}

// TODO: move these to sim.h or somewhere global?

// Number of bytes we allow in a string (1024 in this case)
static const int MAX_IOSTR_SIZE = 0x400;
typedef union WordByte { uint32_t w; uint8_t b[4]; } WordByte;

// Allow up to a 32 MiB file to be slurped in
static int MAX_FILEMAP_SIZE = 512*1024*1024;

// Allow up to a 32 MiB file to be dumped out
static int MAX_FILEDUMP_SIZE = 512*1024*1024;

namespace rigel {
	extern RandomLib::Random TargetRNG;
}

static TargetFDMap target_fd_map; //FIXME Should this be in RigelSim, some per-core structure, ???

static int get_hostfd(int targetfd) {
  TargetFDMap::const_iterator it = target_fd_map.find(targetfd);
  if(it == target_fd_map.end()) {
    fprintf(stderr, "Error: Target attempted to access target fd %d, which does not exist\n", targetfd);
    assert(0 && "Invalid target fd");
  }
  return it->second->get_hostfd();
}

//Return lowest available target fd
//Iterate through the map in increasing order of fd, and stop when
//the ith key is not 'i' (implying that fd 'i' is available).
static int lowest_available_targetfd() {
  int cur = 0;
  for(TargetFDMap::const_iterator it = target_fd_map.begin(), end = target_fd_map.end();
      it != end;
      ++it) {
    if(it->first != cur)
      return cur;
    cur++;
  }
  return cur; //If fd's 0-8 are allocated, for example, return 9.
              //FIXME could check to make sure *all* fd's (64k) aren't allocated.
}

////////////////////////////////////////////////////////////////////////////////
// Syscall Constructor
//
// Description: Initialize Syscall objects
//
// inputs: mmodel (pointer to a backing store containing memory values)
//                      
////////////////////////////////////////////////////////////////////////////////
Syscall::Syscall(rigel::GlobalBackingStoreType *mmodel) : 
  backing_store(mmodel) 
{
  if (backing_store == NULL) {
    throw ExitSim("NULL backing store pointer in Syscall handler class!");
  }

  int stdin_fd = STDIN_FILENO;
  if(rigel::STDIN_STRING_ENABLE) {
    char filename_template[128] = "/tmp/rigelsim_stdin_XXXXXX";
    stdin_fd=mkstemp(filename_template);
    rigel::STDIN_STRING_FILENAME = filename_template;
    ssize_t bytes_written = write(stdin_fd, rigel::STDIN_STRING.c_str(), rigel::STDIN_STRING.size()); //Write string to file
    assert(bytes_written >= 0 && "FIXME: handle errors from write()");
    lseek(stdin_fd, 0, SEEK_SET); //Rewind to beginning of file
  }

  //FIXME Should track these files and close them in ~Syscall()
  int stdout_fd = STDOUT_FILENO;
  if(rigel::REDIRECT_TARGET_STDOUT) {
    FILE *out = fopen(rigel::TARGET_STDOUT_FILENAME.c_str(), "w");
    assert(out && "Could not open target stdout redirect file");
    stdout_fd = fileno(out);
  }

  int stderr_fd = STDERR_FILENO;
  if(rigel::REDIRECT_TARGET_STDERR) {
    FILE *err = fopen(rigel::TARGET_STDERR_FILENAME.c_str(), "w");
    assert(err && "Could not open target stderr redirect file");
    stderr_fd = fileno(err);
  }  

  //FIXME Passing in the tfds from the outside now seems a bit strange..
  //see if there's a way we can, at least sometimes, allocate it internally
  rigelsim::TargetFileDescriptorState *tfds = new rigelsim::TargetFileDescriptorState();
  TargetFileDescriptor *stdin_tfd = new TargetFileDescriptor(tfds, stdin_fd);
  target_fd_map[STDIN_FILENO] = stdin_tfd;
  tfds = new rigelsim::TargetFileDescriptorState();
  TargetFileDescriptor *stdout_tfd = new TargetFileDescriptor(tfds, stdout_fd);
  target_fd_map[STDOUT_FILENO] = stdout_tfd;
  tfds = new rigelsim::TargetFileDescriptorState();
  TargetFileDescriptor *stderr_tfd = new TargetFileDescriptor(tfds, stderr_fd);
  target_fd_map[STDERR_FILENO] = stderr_tfd;
}

///Destructor: Remove mkstemp()-created file from --stdin-string if necessary
Syscall::~Syscall() {
  if (rigel::STDIN_STRING_ENABLE) {
    remove(rigel::STDIN_STRING_FILENAME.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_filemap()
//
// Description: Moves all the bytes from a file into the rigelsim space
//
// Inputs: syscall_struct syscall_data - holds the file pointer as well as the
// destination in rigel and the number of bytes to read
//
// Outputs: Returns the number of bytes read
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_filemap(struct syscall_struct &syscall_data) {
#ifndef _MSC_VER
  // An open file that we can slurp data in from
  const int fdesc = syscall_data.arg1.i;
  const int hostfd = get_hostfd(fdesc);
  // Address of where to put the file data in the target's memory
  uint32_t base_addr = syscall_data.arg2.u;
  // The number of bytes to attempt to read in.  The app should make sure that
  // there is a big enough buffer there for all of the data otherwise the host
  // will overwrite what ever is there, even code pages.
  int num_bytes = syscall_data.arg3.i;
  // The number of bytes we read in which is <= num_bytes
  int bytes_read = 0;
  // A buffer that we will use to load in the file data
  char *buf;
  // Track offset into read buffer and update after every write to target's
  // memory.  This will be returned to the target app telling it how big the
  // actual file is.
  uint32_t buf_offset = 0;

  //cerr << "syscall_filemap: fdesc: " << dec << fdesc << " base_addr " << base_addr << " num_bytes " << num_bytes << endl;

  // Do not allow target to slurp in too huge a file
  assert( (num_bytes < MAX_FILEMAP_SIZE) 
    && "Attempting to read a file that is larger than MAX_FILEMAP_SIZE");

  // Allocate the buffer...remember to be tidy up and delete the memory on exit
  buf = new char[num_bytes];

  // Read the data in in chunks (probably 16 KiB or so at a time).  If there is
  // an error, just kill the simulation since something is funky.
  while ( (num_bytes - bytes_read) > 0) {
    int retval = pread(hostfd, buf+bytes_read, num_bytes-bytes_read, bytes_read);
    if (retval < 0) {
      perror("read"); 
      assert(0 && "An error ocurred while trying to filemap a file.");
    }
    bytes_read += retval;
    //cerr << " READ IN " << dec << retval << " bytes" << endl;
    // It will return zero when finished
    if (retval == 0) break;
  }

  num_bytes = bytes_read;

  /******** BEGIN SYS_READ() YOINK ***********/
  // Do unaligned start
  while ( (base_addr % 4) && (num_bytes > 0) ) {
    WordByte mem_data;

    mem_data.w = backing_store->read_data_word( base_addr & 0xFFFFFFFC);
    mem_data.b[base_addr%4] = buf[buf_offset++];
    //cerr << "base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    backing_store->write_word( base_addr & 0xFFFFFFFC, mem_data.w);
    num_bytes--;
    base_addr++;
  }

  // Do whole-word accesses first and stop when we hit the end
  while (num_bytes >= 4) {
    assert((base_addr % 4 == 0) && "base_addr must be aligned here");
    WordByte mem_data;

    mem_data.b[0] = buf[buf_offset++];
    mem_data.b[1] = buf[buf_offset++];
    mem_data.b[2] = buf[buf_offset++];
    mem_data.b[3] = buf[buf_offset++];
    backing_store->write_word(base_addr, mem_data.w);
    //cerr << "base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    // Go to next word
    base_addr += 4;
    num_bytes -= 4;
  }

  // Do unaligned end
  while (num_bytes > 0) {
    WordByte mem_data;

    mem_data.w = backing_store->read_data_word( base_addr & 0xFFFFFFFC);
    //cerr << "(READ) base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    mem_data.b[base_addr%4] = buf[buf_offset++];
    backing_store->write_word( base_addr & 0xFFFFFFFC, mem_data.w);
    //cerr << "(WRITE) base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    num_bytes--;
    base_addr++;
  }
  /******** END SYS_READ() YOINK ***********/

  delete [] buf;

  // Return the actual number of bytes read in from the file
  //cerr << "Returning " << buf_offset << " bytes read." << endl;
  syscall_data.result.u =  buf_offset;
#else
  syscall_data.result.u =  0;
#endif
}

 

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_fileslurp()
//
// Description: Moves all the bytes from a file into the rigelsim space
//
// Inputs: syscall_struct syscall_data - holds the file pointer as well as the
// destination in rigel and the number of bytes to read
//
// Outputs: Returns the number of bytes read
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_fileslurp(struct syscall_struct &syscall_data) {
  // An open file that we can slurp data in from
  const int fdesc = syscall_data.arg1.i;
  const int hostfd = get_hostfd(fdesc);
  // Address of where to put the file data in the target's memory
  uint32_t base_addr = syscall_data.arg2.u;
  // The number of bytes to attempt to read in.  The app should make sure that
  // there is a big enough buffer there for all of the data otherwise the host
  // will overwrite what ever is there, even code pages.
  int num_bytes = syscall_data.arg3.i;
  // The number of bytes we read in which is <= num_bytes
  int bytes_read = 0;
  // A buffer that we will use to load in the file data
  char *buf;
  // Track offset into read buffer and update after every write to target's
  // memory.  This will be returned to the target app telling it how big the
  // actual file is.
  uint32_t buf_offset = 0;

  //cerr << "syscall_filemap: fdesc: " << dec << fdesc << " base_addr " << base_addr << " num_bytes " << num_bytes << endl;

  // Do not allow target to slurp in too huge a file
  assert( (num_bytes < MAX_FILEMAP_SIZE) 
    && "Attempting to read a file that is larger than MAX_FILEMAP_SIZE");

  // Allocate the buffer...remember to be tidy up and delete the memory on exit
  buf = new char[num_bytes];

  // Read the data in in chunks (probably 16 KiB or so at a time).  If there is
  // an error, just kill the simulation since something is funky.
  while ( (num_bytes - bytes_read) > 0) {
    int retval = read(hostfd, buf+bytes_read, num_bytes-bytes_read);
    if (retval < 0) {
      perror("Fileslurp"); 
      assert(0 && "An error ocurred while trying to fileslurp a file.");
    }
    bytes_read += retval;
    //cerr << " READ IN " << dec << retval << " bytes" << endl;
    // It will return zero when finished
    if (retval == 0) break;
  }

  num_bytes = bytes_read;

  /******** BEGIN SYS_READ() YOINK ***********/
  // Do unaligned start
  while ( (base_addr % 4) && (num_bytes > 0) ) {
    WordByte mem_data;

    mem_data.w = backing_store->read_data_word( base_addr & 0xFFFFFFFC);
    mem_data.b[base_addr%4] = buf[buf_offset++];
    //cerr << "base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    backing_store->write_word( base_addr & 0xFFFFFFFC, mem_data.w);
    num_bytes--;
    base_addr++;
  }

  // Do whole-word accesses first and stop when we hit the end
  while (num_bytes >= 4) {
    assert((base_addr % 4 == 0) && "base_addr must be aligned here");
    WordByte mem_data;

    mem_data.b[0] = buf[buf_offset++];
    mem_data.b[1] = buf[buf_offset++];
    mem_data.b[2] = buf[buf_offset++];
    mem_data.b[3] = buf[buf_offset++];
    backing_store->write_word(base_addr, mem_data.w);
    //cerr << "base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    // Go to next word
    base_addr += 4;
    num_bytes -= 4;
  }

  // Do unaligned end
  while (num_bytes > 0) {
    WordByte mem_data;

    mem_data.w = backing_store->read_data_word( base_addr & 0xFFFFFFFC);
    //cerr << "(READ) base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    mem_data.b[base_addr%4] = buf[buf_offset++];
    backing_store->write_word( base_addr & 0xFFFFFFFC, mem_data.w);
    //cerr << "(WRITE) base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    num_bytes--;
    base_addr++;
  }
  /******** END SYS_READ() YOINK ***********/

  delete [] buf;

  // Return the actual number of bytes read in from the file
  //cerr << "Returning " << buf_offset << " bytes read." << endl;
  syscall_data.result.u = buf_offset;
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_filedump()
//
// Description: Moves all the bytes to a file from the rigelsim space
//
// Inputs: syscall_struct syscall_data - holds the file pointer as well as the
// destination in host and the number of bytes to write out
//
// Outputs: Returns the number of bytes written
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_filedump(struct syscall_struct &syscall_data) {
  // An open file that we dump data to
  const int fdesc = syscall_data.arg1.i;
  const int hostfd = get_hostfd(fdesc);
  // Address of where to read the data from on rigel
  uint32_t base_addr = syscall_data.arg2.u;
  // The number of bytes to attempt to write out
  int num_bytes = syscall_data.arg3.i;
  // The number of bytes we read in which is <= num_bytes
  int bytes_written = 0;
  // A buffer that we will use to load in the data from Rigel
  char *buf;
  // Track offset into read buffer and update after every write to target's
  // memory.  This will be returned to the target app telling it how big the
  // actual file is.
  uint32_t buf_offset = 0;

  //cerr << "syscall_filedump: fdesc: " << dec << fdesc << " base_addr " << base_addr << " num_bytes " << num_bytes << endl;

  // Do not allow target to dump in too huge a file
  assert( (num_bytes < MAX_FILEDUMP_SIZE) 
    && "Attempting to dump a file that is larger than MAX_FILEDUMP_SIZE");

  // Allocate the buffer...remember to be tidy up and delete the memory on exit
  buf = new char[num_bytes];

  // Pull data out of Rigel memory and place in buffer
  /******** BEGIN SYS_READ() YOINK ***********/
  // Do unaligned start
  while ( (base_addr % 4) && (num_bytes > 0) ) {
    WordByte mem_data;

    mem_data.w = backing_store->read_data_word( base_addr & 0xFFFFFFFC);
    buf[buf_offset] = mem_data.b[base_addr%4];
    buf_offset++;
    //cerr << "base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    num_bytes--;
    base_addr++;
  }

  // Do whole-word accesses first and stop when we hit the end
  while (num_bytes >= 4) {
    assert((base_addr % 4 == 0) && "base_addr must be aligned here");
    WordByte mem_data;
    mem_data.w = backing_store->read_data_word(base_addr);
    buf[buf_offset++] = mem_data.b[0];
    buf[buf_offset++] = mem_data.b[1];
    buf[buf_offset++] = mem_data.b[2]; 
    buf[buf_offset++] = mem_data.b[3];
    //cerr << "base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    // Go to next word
    base_addr += 4;
    num_bytes -= 4;
  }

  // Do unaligned end
  while (num_bytes > 0) {
    WordByte mem_data;

    mem_data.w = backing_store->read_data_word(base_addr & 0xFFFFFFFC);
    //cerr << "(READ) base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    buf[buf_offset++] = mem_data.b[base_addr%4]; 
    num_bytes--;
    base_addr++;
  }
  /******** END SYS_READ() YOINK ***********/

  num_bytes = buf_offset;
  // Write out the buf to the file
  // XXXXXXXXXXXXXXXXXXXXXXX
  // Read the data in in chunks (probably 16 KiB or so at a time).  If there is
  // an error, just kill the simulation since something is funky.
  while ( (num_bytes - bytes_written) > 0) {
    int retval = write(hostfd, buf+bytes_written, num_bytes-bytes_written);
    if (retval < 0) {
      perror("Filedump"); 
      assert(0 && "An error ocurred while trying to filedump a file.");
    }
    bytes_written += retval;
    //cerr << " READ IN " << dec << retval << " bytes" << endl;
    // It will return zero when finished
    if (retval == 0) break;
  }


  delete [] buf;

  // Return the actual number of bytes written in from the file
//cerr << "Returning " << buf_offset << " bytes written." << endl;
  syscall_data.result.u = buf_offset;
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_lseek()
//
// Description: The lseek() function repositions the offset of the file 
// descriptor to the argument offset, according to the directive whence
// The argument fildes must be an open file descriptor. See "man lseek"
// for more details
//
// Inputs: fdesc (syscall_data.arg1.i) - file descriptor
//         offset (syscall_data.arg2.i)
//         whence (syscall_data.arg3.i) - directive to 
//
// Outputs: uint32_t offset lseek moved the file pointer
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_lseek(struct syscall_struct &syscall_data) {
  const int fdesc = syscall_data.arg1.i;
  const int hostfd = get_hostfd(fdesc);
  int32_t offset = syscall_data.arg2.i;
  int32_t whence = syscall_data.arg3.i;
  int32_t retval = -1;

  retval = lseek(hostfd, offset, whence);

  assert(retval != -1 && "lseek() has failed...this should probably not happen");

  syscall_data.result.u = retval;
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_write()
//
// Description: The write() function writes some number of bytes, starting 
// at the head of the file descriptor. The source is specific in rigel memory 
// space, starting at a user specified address. 
//
// Inputs: fdesc (syscall_data.arg1.i) - file descriptor
//         base_addr (syscall_data.arg2.i) - address in rigel to write from
//         num_bytes (syscall_data.arg3.i) - number of bytes to write
//
// Outputs: uint32_t number of bytes written
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_write(struct syscall_struct &syscall_data) {

  const int fdesc = syscall_data.arg1.i;
  const int hostfd = get_hostfd(fdesc);
  uint32_t base_addr = syscall_data.arg2.i;
  int bytes_to_write = syscall_data.arg3.i;
	if(fdesc != STDOUT_FILENO && fdesc != STDERR_FILENO) {
	  fprintf(stderr, "syscall_write(fd=%d target/%d host, base_addr=0x%08"PRIx32", num_bytes=%d)...",
            fdesc, hostfd, base_addr, bytes_to_write);
  }

  char *buf = (char *)calloc(bytes_to_write,sizeof(*buf));
  size_t buf_offset = 0;
  int num_bytes = bytes_to_write;
  /******** BEGIN SYS_READ() YOINK ***********/
  // Do unaligned start
  while ( (base_addr % 4) && (num_bytes > 0) ) {
    WordByte mem_data;
    mem_data.w = backing_store->read_data_word( base_addr & 0xFFFFFFFC);
    buf[buf_offset] = mem_data.b[base_addr%4];
    buf_offset++;
    //cerr << "base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    num_bytes--;
    base_addr++;
  }

  // Do whole-word accesses first and stop when we hit the end
  while (num_bytes >= 4) {
    assert((base_addr % 4 == 0) && "base_addr must be aligned here");
    WordByte mem_data;
    mem_data.w = backing_store->read_data_word(base_addr);
    buf[buf_offset++] = mem_data.b[0];
    buf[buf_offset++] = mem_data.b[1];
    buf[buf_offset++] = mem_data.b[2];
    buf[buf_offset++] = mem_data.b[3];
    //cerr << "base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    // Go to next word
    base_addr += 4;
    num_bytes -= 4;
  }

  // Do unaligned end
  while (num_bytes > 0) {
    WordByte mem_data;

    mem_data.w = backing_store->read_data_word( base_addr & 0xFFFFFFFC);
    //cerr << "(READ) base_addr: 0x" << hex << base_addr << " data: 0x" << mem_data.w << endl;
    buf[buf_offset++] = mem_data.b[base_addr%4];
    num_bytes--;
    base_addr++;
  }
  /******** END SYS_READ() YOINK ***********/

	//Let's always try to write the whole thing, rather than return a smaller number of bytes and ask the target to retry.
	//TODO It might be good to have this behavior be configurable, in case we actually want to model I/O timing.
  int bytes_written = 0;
  while (bytes_written < bytes_to_write) {
    int b = write(hostfd, &buf[bytes_written], bytes_to_write-bytes_written);
    assert(b >= 0 && "write() returned an error!");
    bytes_written += b;
  }

	if(fdesc != STDOUT_FILENO && fdesc != STDERR_FILENO) {
	  fprintf(stderr, " wrote %d bytes\n", bytes_written);
	}
  syscall_data.result.u = bytes_written;
  free(buf);
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_read()
//
// Description: The read() function reads some number of bytes from a file 
// and puts them into a buffer on rigel. 
// 
// Inputs: fdesc (syscall_data.arg1.i) - file descriptor (location to read from)
//         base_addr (syscall_data.arg2.i) - base address on rigel to store into
//         num_bytes (syscall_data.arg3.i) - amount of bytes to copy
//
// Outputs: uint32_t offset lseek moved the file pointer
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_read(struct syscall_struct &syscall_data) {
  const int fdesc = syscall_data.arg1.i;
  const int hostfd = get_hostfd(fdesc);
  uint32_t base_addr = syscall_data.arg2.i;
  int bytes_requested = syscall_data.arg3.i;
	int bytes_read;
  if(fdesc != STDIN_FILENO) {
  //fprintf(stderr, "syscall_read(fd=%d target/%d host, base_addr=0x%08"PRIx32", num_bytes=%d)...",
  //        fdesc, hostfd, base_addr, bytes_requested);
  }
	char *buf = new char[bytes_requested+1]; //+1 for a NULL terminator in the stdin case
  memset(buf, 0x0, bytes_requested+1);
  // Track offset into read buffer and update after every write to target's
  // memory
  uint32_t buf_offset = 0;

	// FIXME I'm not sure I like the idea of only reading in one line unconditionally
	// if the file descriptor is stdin.  This prevents us from reading a large chunk of a
	// multiline file piped into stdin from outside.  I believe the reason this is here is
	// to support interactive mode, where you need to enter parameters after the simulation has started.
	// I think we can handle this with the stdin-related command line flags to RigelSim instead of
	// enabling this usage model.
  bytes_read = read(hostfd, buf, bytes_requested);
  assert(bytes_read >= 0 && "FIXME: handle errors from read()");

  if(fdesc != STDIN_FILENO) {
	  //fprintf(stderr, " read %d of %d bytes\n", bytes_read, bytes_requested);
  }

  // Do unaligned start
  while ( (base_addr % 4 != 0) && (bytes_read > 0) ) {
    WordByte mem_data;
    mem_data.w = backing_store->read_data_word(base_addr & 0xFFFFFFFC);
    mem_data.b[base_addr%4] = buf[buf_offset++];
    //cerr << "syscall_read(): writing 0x" << std::hex << mem_data.w << " to mem[0x" << base_addr << "]" << std::endl;
    backing_store->write_word(base_addr & 0xFFFFFFFC, mem_data.w);
    bytes_read--;
    base_addr++;
  }

  // Do whole-word accesses first and stop when we hit the end
  while (bytes_read >= 4) {
    assert((base_addr % 4 == 0) && "base_addr must be aligned here");
    WordByte mem_data;

    mem_data.b[0] = buf[buf_offset++];
    mem_data.b[1] = buf[buf_offset++];
    mem_data.b[2] = buf[buf_offset++];
    mem_data.b[3] = buf[buf_offset++];
		//cerr << "syscall_read(): writing 0x" << std::hex << mem_data.w << " to mem[0x" << base_addr << "]" << std::endl;
    backing_store->write_word(base_addr, mem_data.w);
    // Go to next word
		bytes_read -= 4;
    base_addr += 4;
  }

  // Do unaligned end
  while (bytes_read > 0) {
    WordByte mem_data;

    mem_data.w = backing_store->read_data_word( base_addr & 0xFFFFFFFC);
    mem_data.b[base_addr%4] = buf[buf_offset++];
		//cerr << "syscall_read(): writing 0x" << std::hex << mem_data.w << " to mem[0x" << base_addr << "]" << std::endl;
    backing_store->write_word( base_addr & 0xFFFFFFFC, mem_data.w);
    bytes_read--;
    base_addr++;
  }

  syscall_data.result.u = buf_offset;
	delete[] buf;
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_open()
//
// Description: The open() function opens a file provided a filename and
// the flags. by default, the file is created if it doesn't exist. 
// 
// Inputs: pathname (syscall_data.arg1.u) - file location
//         flags (syscall_data.arg2.u)
//
// Outputs: Returns the file descriptor. 
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_open(struct syscall_struct &syscall_data) {
  uint32_t pathname = syscall_data.arg1.u;
  uint32_t flags = syscall_data.arg2.u;
  mode_t mode = syscall_data.arg3.u;
  char buf[MAX_IOSTR_SIZE];
  memset(buf, 0x0, MAX_IOSTR_SIZE);
  int buf_offset = 0;
  

  do {
    WordByte mem_data;
    mem_data.w = backing_store->read_data_word((pathname) & 0xFFFFFFFC);
    buf[buf_offset] = mem_data.b[pathname % 4];
    pathname++;

    assert(buf_offset < MAX_IOSTR_SIZE && "Overrunning pathname buffer!");
  } while (buf[buf_offset++]);// Terminate when zero is found

  fprintf(stderr, "SYSCALL open(\"%s\", %d)... ", buf, flags);
  // FIXME: We used to have some fixup code like what's below for running RigelSim on Windows.
  // It seems to be a bit simplistic, and we may need to do something more sophisticated to remap
  // between target and host mode bits if host==Windows.
//#ifdef _MSC_VER
//  mode = RS_O_CREAT;
//#endif
//#else
 
  rigelsim::TargetFileDescriptorState *tfds = new rigelsim::TargetFileDescriptorState();
  TargetFileDescriptor *tfd = new TargetFileDescriptor(tfds, std::string(buf), flags, mode);
  int hostfd = tfd->open();
  int fd = lowest_available_targetfd();
  target_fd_map[fd] = tfd;

  fprintf(stderr, "returning fd=%d (hostfd=%d)\n", fd, hostfd);
  syscall_data.result.u = fd;
}

////////////////////////////////////////////////////////////////////////////////
// Method Name: syscall_close()
//
// Description: The close() function closes a file that had previously 
// been opened. 
//
// Inputs: fd (syscall_data.arg1.i) - file descriptor 
//
// Outputs: Returns the file descriptor. 
//
////////////////////////////////////////////////////////////////////////////////
void
Syscall::syscall_close(struct syscall_struct &syscall_data) {
  int fd = syscall_data.arg1.i;
  const int hostfd = get_hostfd(fd);
	fprintf(stderr, "syscall_close(%d) (hostfd=%d)...", fd, hostfd);
	if(fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) { //Don't let the target close the simulator's stdin/stdout/stderr
		fprintf(stderr, "ignored\n");
		fd = 0;
	}
	else {
    fd = close(fd);
    if (-1 == fd) {
      fprintf(stderr, "close() returned error\n");
    }
		else {
      //Remove TargetFileDescriptor from map
      //FIXME I'd like to erase this from the map, but I'm not sure if it always gets erased on the target side.
      //In particular, I saw a case where stdin was closed and we assigned fd=0 to write an output file.
      //I'm not sure if the target did the right thing in this case.
      //target_fd_map.erase(fd);
			fprintf(stderr, "success\n");
	  }
	}
  syscall_data.result.u = fd;
	//TODO FIXME Do we need to set the target's errno according to our errno?
}

//Reseed TargetRNG with the 64-bit vector specified by the first
//two syscall arguments.
void Syscall::syscall_srand(syscall_struct &syscall_data)
{
  unsigned int v[2] = {syscall_data.arg1.u, syscall_data.arg2.u};
	rigel::TargetRNG.Reseed(v, v+2);
  syscall_data.result.u = 0;
}

//Get a random float between the two floats specified in the first
//two syscall arguments.  We assume a *closed* interval [arg1, arg2].
void Syscall::syscall_randomf(syscall_struct &syscall_data)
{
  syscall_data.result.f = syscall_data.arg1.f + ((syscall_data.arg2.f-syscall_data.arg1.f)*rigel::TargetRNG.FloatN<float>());
}

void Syscall::syscall_randomi(syscall_struct &syscall_data)
{
  syscall_data.result.i = rigel::TargetRNG.IntegerC<int32_t>(syscall_data.arg1.i, syscall_data.arg2.i);
}

void Syscall::syscall_randomu(syscall_struct &syscall_data)
{
  syscall_data.result.u = rigel::TargetRNG.IntegerC<uint32_t>(syscall_data.arg1.u, syscall_data.arg2.u);
}

void Syscall::save_to_protobuf(RepeatedTFDS *rtfds) {
  for(TargetFDMap::const_iterator it = target_fd_map.begin(), end = target_fd_map.end(); it != end; ++it) {
    //FIXME Skipping stdin, stdout, stderr for now.
    //if(it->first == STDIN_FILENO || it->first == STDOUT_FILENO || it->first == STDERR_FILENO)
    //  continue;
    //Possible FIXME you can use AddAllocated() instead to avoid the object copy.
    //I chose not to because it also implicitly passes ownership of the pointed-to object to the RepeatedPtrField,
    //so if the TargetFileDescriptor ever needed to outlast the protobuf object, we'd be toast.
    it->second->save(); //Snag updated info from fstat()/lseek()
    //FIXME The fd should really be passed in and stored internal to TargetFileDescriptor or similar.
    //I think we'll have to make an explicit class that (thinly) wraps TargetFDMap.
    rigelsim::TargetFileDescriptorState *tfds = it->second->get_state();
    tfds->set_fd(it->first);
    *(rtfds->Add()) = *(it->second->get_state());
  }
}

void Syscall::restore_from_protobuf(RepeatedTFDS *rtfds) {
  for(RepeatedTFDS::iterator it = rtfds->begin(), end = rtfds->end(); it != end; ++it) {
    TargetFileDescriptor *tfd = new TargetFileDescriptor(&(*it));
    if(tfd->has_path()) tfd->open_to_previous_offset();
    target_fd_map[it->fd()] = tfd;
  }
}

