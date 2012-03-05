#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t
#include <string.h>                     // for memset
#include <cassert>                      // for assert
#include <ostream>                      // for operator<<, basic_ostream, etc
#include <string>                       // for char_traits
#include "define.h"         // for rigel_log2
#include "util/value_tracker.h"  // for ZeroTracker, etc

ZeroTracker *GLOBAL_ZERO_TRACKER_PTR;

void ZeroTracker::addRead(uint32_t value)
{
  numReads++;
  int arrayIndex = 0;
  for(size_t i = 1; i <= WORDSIZE; i <<= 1) {
    numReadsSaved[arrayIndex] += countZeros(value, i);
    arrayIndex++;
  }
}
  
void ZeroTracker::addWrite(uint32_t readVal, uint32_t writeVal) {
  numWrites++;
  int arrayIndex = 0;
  for(size_t i = 1; i <= WORDSIZE; i <<= 1) {
    numWritesSaved[arrayIndex] += countZeros(writeVal, i);
    arrayIndex++;
  }
}


void ZeroTracker::addRead(uint32_t *values, size_t numValues) {

}
void ZeroTracker::addWrite(uint32_t *readvals, uint32_t *writevals, size_t numValues) {

}

void ZeroTracker::report(std::ostream &str) {
  str << numReads << " total reads, " << numWrites << " total writes" << std::endl;
  int arrayIndex = 0;
  for(size_t i = 1; i <= WORDSIZE; i <<= 1) {
    str << "Granularity " << i << ": ";
    str << numReadsSaved[arrayIndex] << " reads saved (" << ((double)(numReadsSaved[arrayIndex])*100.0f*i/((double)numReads*WORDSIZE));
    str << "%), ";
    str << numWritesSaved[arrayIndex] << " writes saved (" << ((double)(numWritesSaved[arrayIndex])*100.0f*i/((double)numWrites*WORDSIZE));
    str << "%)" << std::endl;
    arrayIndex++;
  }
}
 
void ZeroTracker::reset() {
  numReads = 0;
  numWrites = 0;
  //size_t arraySize = WORDSIZE;
  memset(numReadsSaved, 0, (rigel_log2(WORDSIZE)+1)*sizeof(size_t));
  memset(numWritesSaved, 0, (rigel_log2(WORDSIZE)+1)*sizeof(size_t));
//   for(size_t i = 0; i < (rigel_log2(WORDSIZE)+1); i++) {
//     memset(numReadsSaved[i], 0, arraySize*sizeof(size_t));
//     memset(numWritesSaved[i], 0, arraySize*sizeof(size_t));
//   }
}

size_t ZeroTracker::countZeros(uint32_t *values, size_t numValues, size_t bitGranularity) {
  assert(numValues == 1 && "Only support single values for now");
  uint32_t v = *values; //Make a copy
  size_t zeros = 0;
  size_t numShifts = (sizeof(uint32_t) * 8) / bitGranularity;
  for(size_t i = 0; i < numShifts; i++) {
    bool foundOne = false;
    for(size_t j = 0; j < bitGranularity; j++) {
      if((v & 0x1) != 0) { //One bit
        foundOne = true;
      }
      v >>= 1;
    }
    if(!foundOne) {
      zeros++;
    }
  }
  return zeros;
}
 
