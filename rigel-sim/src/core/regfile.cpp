
#include "core/regfile.h"

namespace rigel {
  extern const char * sprf_strings[];
}

/// constructor
RegisterFile::RegisterFile(unsigned int numregs) 
  : numregs(numregs),
    tracefile(NULL)
{
  rf = new regval32_t[numregs];
  rf[0] = (uint32_t)0;
  for (unsigned int i=1; i<numregs; i++) {
    rf[i] = (int)0;
  }
}

/// destructor
RegisterFile::~RegisterFile() {
  if (rf) { delete rf; }
  if (tracefile) {
    fclose(tracefile);
  }
}

/// accessor
const regval32_t 
RegisterFile::operator[](uint32_t i) const {
  assert(i < numregs);
  return rf[i];  
}

/// write a register
void 
RegisterFile::write(unsigned int i, regval32_t rv, uint32_t pc) {
  assert(i < numregs && "attempting to write invalid register");
  if (i != 0) { // ignore writes to R0
    rf[i] = rv;
    if (tracefile) {
      //fprintf(tracefile,"%012"PRIu64" %08x %d %08x\n", rigel::CURR_CYCLE,pc, i, rv.u32());
      fprintf(tracefile,"%08x %d %08x\n", pc, i, rv.u32());
    }
  }
}

/// Dump, print RF contents
void 
RegisterFile::Dump(unsigned int cols) {
  for (unsigned int i=0; i<numregs; i++) {
    printf("%2u: 0x%08x ", i, rf[i].i32());
    if ((i % cols) == (cols-1)) {
      printf("\n");
    }
  } 
  printf("\n");
}

/// set trace file
void 
RegisterFile::setTraceFile( const char* filename ) {
  DPRINT(DB_RF,"%s: %s\n",__func__,filename); 
  tracefile = fopen(filename,"w");
  assert(tracefile != NULL && "invalid tracefile or unable to open");
  setbuf(tracefile,NULL); /// FIXME DELETE THIS!
}


/// assign: for assigning at init time and internal use by the simulator ONLY
/// NOTE: allows arbitrary clobbering of SPRF values
/// NOTE: NOT for use during runtime simulation. use write()
void
SPRegisterFile::assign(sprf_names_t i, uint32_t val) {
  assert((unsigned)i < numregs && "attempting to assign invalid out-of-range register");
  // warn if simcycle > 0?
  printf("sprf::assign[%20s:%d]: %8d (%08x)\n", rigel::sprf_strings[i], i, val, val);
  rf[i] = regval32_t(val);
}

/// for writing to the SPRF during simulation
void 
SPRegisterFile::write(unsigned int i, regval32_t rv, uint32_t pc) {
  assert(i < numregs && "attempting to write invalid out-of-range register");
  if (writable.test(i)) {
    rf[i] = rv;
    if (tracefile) {
      //fprintf(tracefile,"%012"PRIu64" %08x %d %08x\n", rigel::CURR_CYCLE,pc, i, rv.u32());
      fprintf(tracefile,"%08x s%d %08x\n", pc, i, rv.u32());
    }
  } else {
    printf("attempted to write READONLY SPRF register [%d] (%s)\n", 
      i, rigel::sprf_strings[i]);
    assert(0&&"attempting to write readonly register!");
  }
}
