#ifndef __DYNAMIC_BITSET_H__
#define __DYNAMIC_BITSET_H__

#include <cstdio>
#include <inttypes.h>
#include <stdint.h>
#include <cstring>
#include <set>
#include "define.h" //for rigel_bsf()
#include <cassert>
#include "util/debug.h"

//Forward-declarations
class DynamicBitset32;
class DynamicBitset64;

//Can switch between one implementation and the other, depending on host machine
#if 0
  typedef DynamicBitset64 DynamicBitset;
#else
  typedef DynamicBitset32 DynamicBitset;
#endif

//32-bit version
class DynamicBitset32
{

  public:
    DynamicBitset32(size_t numBits) : size(numBits), numSetBits(0)
    {
      size_t numWords = (size+31)/32;
      data = new uint32_t[numWords];
      //Zero out array
      memset((void *)data, 0, numWords*sizeof(uint32_t));
      end = data + numWords; //Pointer to one past end of data array
    };

    //Need to define copy constructor for deep copies of objects which contain one of us.
    DynamicBitset32(const DynamicBitset32 &other) : size(other.size), numSetBits(other.numSetBits)
    {
      size_t numWords = (size+31)/32;
      data = new uint32_t[numWords];
      memcpy(data, other.data, numWords*sizeof(uint32_t));
      end = data + numWords;
    }

    //Need to define assignment for deep copies of objects which contain one of us.
    DynamicBitset32& operator=(const DynamicBitset32 &other)
    {
      if (this == &other) return *this;
      size = other.size;
      numSetBits = other.numSetBits;
      delete[] data;
      size_t numWords = (size+31)/32;
      data = new uint32_t[numWords];
      memcpy(data, other.data, numWords*sizeof(uint32_t));
      end = data + numWords;
      return *this;
    }
    
    ~DynamicBitset32() { delete[] data; } 

    //Small bitsets can print in binary, otherwise in hex
    void print(FILE *stream, bool alwaysInHex = false) const
    {
      if(size <= 64 && !alwaysInHex)
      {
        size_t i = size-1;
        while(1)
        {
          fprintf(stream, "%s", (test(i) ? "1" : "0"));
          if(i == 0U) break;
          else i--;
        }
      }
      else
      {
        fprintf(stream, "0x");
        uint32_t *walker = end-1;
        //Print last word (may be unaligned)
        if(size % 32 != 0)
        {
          uint32_t mask = ((uint32_t)1 << (size % 32))-1;
          switch(((size % 32) + 3) / 4)
          {
            case 1: fprintf(stream, "%01x", *walker & mask); break;
            case 2: fprintf(stream, "%02x", *walker & mask); break;
            case 3: fprintf(stream, "%03x", *walker & mask); break;
            case 4: fprintf(stream, "%04x", *walker & mask); break;
            case 5: fprintf(stream, "%05x", *walker & mask); break;
            case 6: fprintf(stream, "%06x", *walker & mask); break;
            case 7: fprintf(stream, "%07x", *walker & mask); break;
            case 8: fprintf(stream, "%08x", *walker & mask); break;
          }
          walker--;
        }
        while(walker >= data)
        {
          fprintf(stream, "%08x", *walker);
          walker--;
        }
      }
    }

    void clearAll() { setUintValue(0x00000000U); numSetBits = 0; }
    void setAll() { setUintValue(0xFFFFFFFFU); numSetBits = size; }
    void toggleAll()
    {
      uint32_t *walker = data;
      while(walker != end)
      {
        *walker ^= 0xFFFFFFFFU;
        walker++;
      }
      numSetBits = size-numSetBits;
    }
    void clear(const size_t pos)
    {
      if(pos >= size) //out of range
      {
        rigel_dump_backtrace();
        fprintf(stderr, "Error: Trying to clear bit %zu in a %zu-bit set\n", pos, size);
        assert(0);
      }
      const size_t dataIndex = pos / 32;
      const uint32_t mask = ~(1U << (pos % 32));
      const uint32_t *dataPtr = &(data[dataIndex]);
      if((*dataPtr | mask) == 0xFFFFFFFFU) //If bit is set
        numSetBits--; //One fewer bit set
      data[dataIndex] &= mask; //Clear the bit
    }
    void set(const size_t pos)
    {
      if(pos >= size) //out of range
      {
        fprintf(stderr, "Error: Trying to set bit %zu in a %zu-bit set\n", pos, size);
        assert(0);
      }
      const size_t dataIndex = pos / 32;
      const uint32_t mask = (1U << (pos % 32));
      const uint32_t *dataPtr = &(data[dataIndex]);
      if((*dataPtr & mask) == 0x00000000U) //If bit is clear
        numSetBits++; //One more bit set
      data[dataIndex] |= mask; //Set the bit
    }
    void toggle(const size_t pos)
    {
      if(pos >= size) //out of range
      {
        fprintf(stderr, "Error: Trying to set bit %zu in a %zu-bit set\n", pos, size);
        assert(0);
      }
      const size_t dataIndex = pos / 32;
      const uint32_t mask = (1U << (pos % 32));
      const uint32_t *dataPtr = &(data[dataIndex]);
      if((*dataPtr & mask) == 0x00000000U) //If bit is clear
        numSetBits++; //One more bit set
      else
        numSetBits--; //One fewer bit set
      data[dataIndex] ^= mask; //Toggle the bit
    }
    
    bool allSet() const { return (numSetBits == size); }
    bool allClear() const { return (numSetBits == 0); }

    bool test(const size_t pos) const
    {
      if(pos >= size) //out of range
      {
        fprintf(stderr, "Error: Trying to test bit %zu in a %zu-bit set\n", pos, size);
        assert(0);
      }
      const size_t dataIndex = pos / 32;
      const uint32_t mask = (1U << (pos % 32));
      return ((data[dataIndex] & mask) != 0x00000000U);
    }

    size_t getNumSetBits() const { return numSetBits; }
    size_t getNumClearBits() const { return (size - numSetBits); }
    size_t getSize() const { return size; }

    int findFirstSet() const
    {
      return findFirstSet(data); //Start from beginning
    }

    int findFirstClear() const
    {
      return findFirstClear(data); //Start from beginning
    }

    int findNextSetInclusive(size_t lastSet) const
    {
      if(lastSet == 0)
        return findFirstSet();
      else
        return findNextSet(lastSet-1);
    }

    int findNextClearInclusive(size_t lastClear) const
    {
      if(lastClear == 0)
        return findFirstClear();
      else
        return findNextClear(lastClear-1);
    }

    int findNextSet(size_t lastSet) const
    {
      //If the previous one was on a word boundary, search subsequent full words
      if((lastSet+1U) % 32U == 0)
        return findFirstSet(&(data[(lastSet+1U)/32U]));
      //Else, need to search partial word first.
      const size_t wordIndex = lastSet/32U;
      uint32_t *wordptr = &(data[wordIndex]);
      uint32_t word = *wordptr;
      const size_t bitInWord = lastSet % 32U;
      //Mask it (clear all bits at lastSet and below)
      word &= ~((1 << (bitInWord+1))-1);
      if(word == 0x00000000U) //Rest of bits are clear, search subsequent full words
        return findFirstSet(wordptr+1);
      else //Next set bit is in this word, return it.
      {
        const int bitInWord = rigel_bsf(word);
        const int ret = bitInWord + 32U*wordIndex;
        if(ret >= (int)size)
          return -1;
        else
          return ret;
      }
    }

    int findNextClear(size_t lastSet) const
    {
      //If the previous one was on a word boundary, search subsequent full words
      if((lastSet+1U) % 32U == 0)
        return findFirstClear(&(data[(lastSet+1U)/32U]));
      //Else, need to search partial word first.
      const size_t wordIndex = lastSet/32U;
      uint32_t *wordptr = &(data[wordIndex]);
      uint32_t word = *wordptr;
      const size_t bitInWord = lastSet % 32U;
      //Mask it (set all bits at lastSet and below)
      word |= ((1 << (bitInWord+1))-1);
      if(word == 0xFFFFFFFFU) //Rest of bits are set, search subsequent full words
        return findFirstClear(wordptr+1);
      else //Next set bit is in this word, return it. (complement, then do bsf)
      {
        const int bitInWord = rigel_bsf(~word);
        const int ret = bitInWord + 32U*wordIndex;
        if(ret >= (int)size)
          return -1;
        else
          return ret;
      }
    }

    size_t findAllSet(std::set<size_t> &setToFill) const
    {
      if(allClear()) return 0U;
      int setBitPos = findFirstSet();
      setToFill.insert(setBitPos);
      size_t numFound = 1;
      while((numFound < numSetBits) && (setBitPos = findNextSet(setBitPos)) != -1)
      {
        setToFill.insert(setBitPos);
        numFound++;
      }
      return numSetBits;
    }

    size_t findAllClear(std::set<size_t> &setToFill) const
    {
      if(allSet()) return 0U;
      int clearBitPos = findFirstClear();
      setToFill.insert(clearBitPos);
      size_t numFound = 1;
      const size_t numClearBits = size-numSetBits;
      while((numFound < numClearBits) && (clearBitPos = findNextClear(clearBitPos)) != -1)
      {
        setToFill.insert(clearBitPos);
        numFound++;
      }
      return numClearBits;
    }    
    
 
  private:

    void setUintValue(const uint32_t val)
    {
      uint32_t *walker = data;
      while(walker != end)
      {
        *walker = val;
        walker++;
      }
    }

    int findFirstSet(uint32_t const *wordPos) const
    {
      uint32_t const *walker = wordPos;
      while(walker != end)
      {
        if(*walker == 0x00000000U) //All clear, move on
        {
          walker++;
          continue;
        }
        //else, first set bit can be found with a bsf.
        size_t firstbit = rigel_bsf(*walker);
        if(((size % 32) != 0) &&
           (walker == (end-1)) &&
           (firstbit >= (size % 32))) //found one, but it's past the end
        {
          return -1;
        }
        else
        {
          return (int)(firstbit + ((ptrdiff_t)(walker - data))*32);
        }
      }
      return -1;
    }

    int findFirstClear(uint32_t const *wordPos) const
    {
      uint32_t const *walker = wordPos;
      while(walker != end)
      {
        if(*walker == 0xFFFFFFFFU) //All set, move on
        {
          walker++;
          continue;
        }
        //else, first clear bit can be found with a bsf on the complemented value.
        size_t firstbit = rigel_bsf(~(*walker));
        if(((size % 32) != 0) &&
          (walker == (end-1)) &&
          (firstbit >= (size % 32))) //found one, but it's past the end
        {
          return -1;
        }
        else
        {
          return (int)(firstbit + ((ptrdiff_t)(walker - data))*32);
        }
      }
      return -1;
    }
    
    uint32_t *data, *end;
    size_t size;
    size_t numSetBits;
};

///////////////////////////////////////////////////////////////////////////////////
//64-bit version

class DynamicBitset64
{
  public:
    DynamicBitset64(size_t numBits) : size(numBits), numSetBits(0)
    {
      size_t numWords = (size+63)/64;
      data = new uint64_t[numWords];
      //Zero out array
      memset((void *)data, 0, numWords*sizeof(uint64_t));
      end = data + numWords; //Pointer to one past end of data array
    };

    DynamicBitset64(const DynamicBitset64 &other) : size(other.size), numSetBits(other.numSetBits)
    {
      size_t numWords = (size+63)/64;
      data = new uint64_t[numWords];
      memcpy(data, other.data, numWords*sizeof(uint64_t));
      end = data + numWords;
    }

    //Need to define assignment for deep copies of objects which contain one of us.
    DynamicBitset64& operator=(const DynamicBitset64 &other)
    {
      if (this == &other) return *this;
      delete[] data;
      size = other.size;
      numSetBits = other.numSetBits;
      size_t numWords = (size+63)/64;
      data = new uint64_t[numWords];
      memcpy(data, other.data, numWords*sizeof(uint64_t));
      end = data + numWords;
      return *this;
    }
    
    ~DynamicBitset64() { delete[] data; }
    
    //Small bitsets can print in binary, otherwise in hex
    void print(FILE *stream, bool alwaysInHex = false) const
    {
      if(size <= 64 && !alwaysInHex)
      {
        size_t i = size-1;
        while(1)
        {
          fprintf(stream, "%s", (test(i) ? "1" : "0"));          
          if(i == 0U) break;
          else i--;
        }
      }
      else
      {
        fprintf(stream, "0x");
        uint64_t *walker = end-1;
        //Print last word (may be unaligned)
        if(size % 64 != 0)
        {
          uint64_t mask = ((uint64_t)1 << (size % 64))-1;
          switch(((size % 64) + 3) / 4)
          {
            case 1: fprintf(stream, "%01"PRIx64"", *walker & mask); break;
            case 2: fprintf(stream, "%02"PRIx64"", *walker & mask); break;
            case 3: fprintf(stream, "%03"PRIx64"", *walker & mask); break;
            case 4: fprintf(stream, "%04"PRIx64"", *walker & mask); break;
            case 5: fprintf(stream, "%05"PRIx64"", *walker & mask); break;
            case 6: fprintf(stream, "%06"PRIx64"", *walker & mask); break;
            case 7: fprintf(stream, "%07"PRIx64"", *walker & mask); break;
            case 8: fprintf(stream, "%08"PRIx64"", *walker & mask); break;
            case 9: fprintf(stream, "%09"PRIx64"", *walker & mask); break;
            case 10: fprintf(stream, "%010"PRIx64"", *walker & mask); break;
            case 11: fprintf(stream, "%011"PRIx64"", *walker & mask); break;
            case 12: fprintf(stream, "%012"PRIx64"", *walker & mask); break;
            case 13: fprintf(stream, "%013"PRIx64"", *walker & mask); break;
            case 14: fprintf(stream, "%014"PRIx64"", *walker & mask); break;
            case 15: fprintf(stream, "%015"PRIx64"", *walker & mask); break;
            case 16: fprintf(stream, "%016"PRIx64"", *walker & mask); break;
          }
          walker--;
        }
        while(walker >= data)
        {
          fprintf(stream, "%016"PRIx64"", *walker);
          walker--;
        }
      }
    }
    
    void clearAll() { setUintValue(UINT64_C(0x0000000000000000)); numSetBits = 0; }
    void setAll() { setUintValue(UINT64_C(0xFFFFFFFFFFFFFFFF)); numSetBits = size; }
    void toggleAll()
    {
      uint64_t *walker = data;
      while(walker != end)
      {
        *walker ^= UINT64_C(0xFFFFFFFFFFFFFFFF);
        walker++;
      }
      numSetBits = size-numSetBits;
    }
    void clear(const size_t pos)
    {
      if(pos >= size) //out of range
      {
        fprintf(stderr, "Error: Trying to clear bit %zu in a %zu-bit set\n", pos, size);
        assert(0);
      }
      const size_t dataIndex = pos / 64;
      const uint64_t notmask = UINT64_C(1) << (pos % 64);
      const uint64_t mask = ~notmask;
      const uint64_t *dataPtr = &(data[dataIndex]);
      if((*dataPtr | mask) == UINT64_C(0xFFFFFFFFFFFFFFFF)) //If bit is set
        numSetBits--; //One fewer bit set
        data[dataIndex] &= mask; //Clear the bit
    }
    void set(const size_t pos)
    {
      if(pos >= size) //out of range
      {
        fprintf(stderr, "Error: Trying to set bit %zu in a %zu-bit set\n", pos, size);
        assert(0);
      }
      const size_t dataIndex = pos / 64;
      const uint64_t mask = UINT64_C(1) << (pos % 64);
      const uint64_t *dataPtr = &(data[dataIndex]);
      if((*dataPtr & mask) == UINT64_C(0x0000000000000000)) //If bit is clear
        numSetBits++; //One more bit set
        data[dataIndex] |= mask; //Set the bit
    }
    bool toggle(const size_t pos)
    {
      if(pos >= size) //out of range
      {
        fprintf(stderr, "Error: Trying to set bit %zu in a %zu-bit set\n", pos, size);
        assert(0);
      }
      const size_t dataIndex = pos / 64;
      const uint64_t mask = (UINT64_C(1) << (pos % 64));
      const uint64_t *dataPtr = &(data[dataIndex]);
      bool ret;
      if((*dataPtr & mask) == UINT64_C(0x0000000000000000)) //If bit is clear
      {
        ret = true; //Now set
        numSetBits++; //One more bit set
      }
      else
      {
        ret = false; //Now clear
        numSetBits--; //One fewer bit set
      }
      data[dataIndex] ^= mask; //Toggle the bit
      return ret;
    }
    
    bool allSet() const { return (numSetBits == size); }
    bool allClear() const { return (numSetBits == 0); }
    
    bool test(const size_t pos) const
    {
      if(pos >= size) //out of range
      {
        fprintf(stderr, "Error: Trying to test bit %zu in a %zu-bit set\n", pos, size);
        assert(0);
      }
      const size_t dataIndex = pos / 64;
      const uint64_t mask = ((uint64_t)1UL << (pos % 64));
      return ((data[dataIndex] & mask) != UINT64_C(0x0000000000000000));
    }
    
    size_t getNumSetBits() const { return numSetBits; }
    size_t getNumClearBits() const { return (size-numSetBits); }
    size_t getSize() const { return size; }
    
    int findFirstSet() const
    {
      return findFirstSet(data); //Start from beginning
    }
    
    int findFirstClear() const
    {
      return findFirstClear(data); //Start from beginning
    }
    
    int findNextSet(size_t lastSet) const
    {
      //If the previous one was on a word boundary, search subsequent full words
      if((lastSet+1U) % 64U == 0)
        return findFirstSet(&(data[(lastSet+1UL)/64ULL]));
      //Else, need to search partial word first.
      const size_t wordIndex = lastSet/64U;
      uint64_t *wordptr = &(data[wordIndex]);
      uint64_t word = *wordptr;
      const size_t bitInWord = lastSet % 64U;
      //Mask it (clear all bits at lastSet and below)
      word &= ~(((uint64_t)1 << (bitInWord+1))-1);
      if(word == UINT64_C(0x0000000000000000)) //Rest of bits are clear, search subsequent full words
        return findFirstSet(wordptr+1);
      else //Next set bit is in this word, return it.
      {
        const int bitInWord = rigel_bsf(word);
        const int ret = bitInWord + 64U*wordIndex;
        if(ret >= (int)size)
          return -1;
        else
          return ret;
      }
    }
    
    int findNextClear(size_t lastSet) const
    {
      //If the previous one was on a word boundary, search subsequent full words
      if((lastSet+1U) % 64U == 0)
        return findFirstClear(&(data[(lastSet+1U)/64U]));
      //Else, need to search partial word first.
      const size_t wordIndex = lastSet/64U;
      uint64_t *wordptr = &(data[wordIndex]);
      uint64_t word = *wordptr;
      const size_t bitInWord = lastSet % 64U;
      //Mask it (set all bits at lastSet and below)
      word |= (((uint64_t)1 << (bitInWord+1))-1);
      if(word == UINT64_C(0xFFFFFFFFFFFFFFFF)) //Rest of bits are set, search subsequent full words
        return findFirstClear(wordptr+1);
      else //Next set bit is in this word, return it. (complement, then do bsf)
      {
        int bitInWord = rigel_bsf(~word);
        const int ret = bitInWord + 64U*wordIndex;
        if(ret >= (int)size)
          return -1;
        else
          return ret;
      }
    }

    int findNextSetInclusive(size_t lastSet) const
    {
      if(lastSet == 0)
        return findFirstSet();
      else
        return findNextSet(lastSet-1);
    }

    int findNextClearInclusive(size_t lastClear) const
    {
      if(lastClear == 0)
        return findFirstClear();
      else
        return findNextClear(lastClear-1);
    }
    
    size_t findAllSet(std::set<size_t> &setToFill) const
    {
      if(allClear()) return 0U;
      int setBitPos = findFirstSet();
      setToFill.insert(setBitPos);
      size_t numFound = 1;
      while((numFound < numSetBits) && (setBitPos = findNextSet(setBitPos)) != -1)
      {
        setToFill.insert(setBitPos);
        numFound++;
      }
      return numSetBits;
    }
    
    size_t findAllClear(std::set<size_t> &setToFill) const
    {
      if(allSet()) return 0U;
      int clearBitPos = findFirstClear();
      setToFill.insert(clearBitPos);
      size_t numFound = 1;
      const size_t numClearBits = size-numSetBits;
      while((numFound < numClearBits) && (clearBitPos = findNextClear(clearBitPos)) != -1)
      {
        setToFill.insert(clearBitPos);
        numFound++;
      }
      return numClearBits;
    }
    
    
  private:
    
    void setUintValue(const uint64_t val)
    {
      uint64_t *walker = data;
      while(walker != end)
      {
        *walker = val;
        walker++;
      }
    }
    
    int findFirstSet(uint64_t const *wordPos) const
    {
      uint64_t const *walker = wordPos;
      while(walker != end)
      {
        if(*walker == UINT64_C(0x0000000000000000)) //All clear, move on
        {
          walker++;
          continue;
        }
        //else, first set bit can be found with a bsf.
        size_t firstbit = rigel_bsf(*walker);
        if(((size % 64) != 0) &&
          (walker == (end-1)) &&
          (firstbit >= (size % 64))) //found one, but it's past the end
          {
            return -1;
          }
          else
          {
            return (int)(firstbit + ((ptrdiff_t)(walker - data))*64);
          }
      }
      return -1;
    }
    
    int findFirstClear(uint64_t const *wordPos) const
    {
      uint64_t const *walker = wordPos;
      while(walker != end)
      {
        if(*walker == UINT64_C(0xFFFFFFFFFFFFFFFF)) //All set, move on
        {
          walker++;
          continue;
        }
        //else, first clear bit can be found with a bsf on the complemented value.
        size_t firstbit = rigel_bsf(~(*walker));
        if(((size % 64) != 0) &&
          (walker == (end-1)) &&
          (firstbit >= (size % 64))) //found one, but it's past the end
          {
            return -1;
          }
          else
          {
            return (int)(firstbit + ((ptrdiff_t)(walker - data))*64);
          }
      }
      return -1;
    }
    
    uint64_t *data, *end;
    size_t size;
    size_t numSetBits;
};

#endif //#ifndef __DYNAMIC_BITSET_H__

