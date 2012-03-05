#ifndef __VALUE_TRACKER_H__
#define __VALUE_TRACKER_H__

#include <stdint.h>
#include <inttypes.h>
#include <iostream>
#include "define.h" //For rigel_log2()
#include <cstring> //For memset

class ValueTracker {
  public:
    virtual void addRead(uint32_t value) = 0;
    virtual void addWrite(uint32_t readVal, uint32_t writeVal) = 0;
    virtual void addRead(uint32_t *values, size_t numValues) = 0;
    virtual void addWrite(uint32_t *readvals, uint32_t *writevals, size_t numValues) = 0;
    virtual void report(std::ostream &str) = 0;
    virtual void reset() = 0;
};

class ZeroTracker : public ValueTracker {
  public:
    const static size_t WORDSIZE = 32; //32-bit words
    ZeroTracker() : numReads(0), numWrites(0) {
      size_t log2_wordsize = rigel_log2(WORDSIZE);
      size_t arraysize = log2_wordsize + 1;
//       numZeroValues = new size_t *[arraysize];
//       numReadsSaved = new size_t *[arraysize];
//       numWritesSaved = new size_t *[arraysize];
//       for(size_t i = 0; i < arraysize; i++) {
//         size_t subarraysize = 1 << (arraysize - i - 1);
//         numZeroValues[i] = new size_t [subarraysize];
//         memset(numZeroValues[i], 0, subarraysize*sizeof(size_t));
//         numReadsSaved[i] = new size_t [subarraysize];
//         memset(numReadsSaved[i], 0, subarraysize*sizeof(size_t));
//         numWritesSaved[i] = new size_t [subarraysize];
//         memset(numWritesSaved[i], 0, subarraysize*sizeof(size_t));
//       }
      numReadsSaved = new size_t [arraysize];
      numWritesSaved = new size_t [arraysize];
      reset();
    }
      void addRead(uint32_t value);
      void addWrite(uint32_t readVal, uint32_t writeVal);
      void addRead(uint32_t *values, size_t numValues);
      void addWrite(uint32_t *readvals, uint32_t *writevals, size_t numValues);
      void report(std::ostream &str);
      void reset();
  private:
    //FIXME: Only supports single values
    size_t countZeros(uint32_t &value, size_t bitGranularity) {
      return countZeros(&value, 1, bitGranularity);
    }
    size_t countZeros(uint32_t *values, size_t numValues, size_t bitGranularity);
    size_t *numReadsSaved;
    size_t *numWritesSaved;
    size_t numReads;
    size_t numWrites;
};
    

#endif //#ifndef __VALUE_TRACKER_H__
