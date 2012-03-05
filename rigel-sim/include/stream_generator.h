#ifndef __STREAM_GENERATOR_H__
#define __STREAM_GENERATOR_H__

#include <iostream>
#include <list>
#include "sim.h"
#include "RandomLib/Random.hpp"

class DRAMDriver;
struct mem_trace_reader;

namespace rigel {
  extern RandomLib::Random RNG;
}

using namespace rigel;

typedef struct _MemoryRequest
{
  _MemoryRequest(uint32_t a, bool rNw)
  {
    addr = a;
    size = rigel::cache::LINESIZE;
    readNotWrite = rNw;
    scheduled = false;
  }
  _MemoryRequest(uint32_t a, bool rNw, uint32_t i)
  {
    addr = a;
    size = rigel::cache::LINESIZE;
    identifier = i;
    readNotWrite = rNw;
    scheduled = false;
  }
  uint32_t addr;
  uint32_t size;
  uint32_t identifier;
  bool readNotWrite;
  bool scheduled;
} MemoryRequest;

class RequestGenerator
{
  public:
    //Pure virtual
    virtual MemoryRequest * getNextRequest() = 0;
};

class GzipRequestGenerator : public RequestGenerator
{
  public:
    GzipRequestGenerator(struct mem_trace_reader *_mtr, int hwThreadNum, int numHWThreads) :
      mtr(_mtr), thread(hwThreadNum), numThreads(numHWThreads)
    {
    }

  virtual MemoryRequest * getNextRequest();

  private:
    struct mem_trace_reader *mtr;
    int thread;
    int numThreads;
};

class StrideReadGenerator : public RequestGenerator
{
  public:
    StrideReadGenerator(uint32_t _start, uint32_t _stride) :
      addr(_start), stride(_stride)
    {
    }

    virtual MemoryRequest * getNextRequest()
    {
      uint32_t oldAddr = addr;
      addr += stride;
      MemoryRequest *mr = new MemoryRequest(oldAddr, true);
      return mr;
    }

  private:
    uint32_t addr;
    uint32_t stride;
};

class StrideWriteGenerator : public RequestGenerator
{
  public:
    StrideWriteGenerator(uint32_t _start, uint32_t _stride) :
      addr(_start), stride(_stride)
    {
    }

    virtual MemoryRequest * getNextRequest()
    {
      uint32_t oldAddr = addr;
      addr += stride;
      MemoryRequest *mr = new MemoryRequest(oldAddr, false);
      return mr;
    }

  private:
    uint32_t addr;
    uint32_t stride;
};

class StrideAlternatingReadWriteGenerator : public RequestGenerator
{
  public:
    StrideAlternatingReadWriteGenerator(uint32_t _start, uint32_t _stride, uint32_t alternate, bool startReadNotWrite) :
      addr(_start), stride(_stride), alternate_reset(alternate), alternate_counter(alternate),
      currentlyReadOrWrite(startReadNotWrite)
    {
    }

    virtual MemoryRequest * getNextRequest()
    {
      uint32_t oldAddr = addr;
      addr += stride;
      MemoryRequest *mr = new MemoryRequest(oldAddr, currentlyReadOrWrite);
      alternate_counter--;
      if(alternate_counter == 0)
      {
        alternate_counter = alternate_reset;
        currentlyReadOrWrite = !currentlyReadOrWrite;
      }
      return mr;
    }

  private:
    uint32_t addr;
    uint32_t stride;
    uint32_t alternate_reset;
    uint32_t alternate_counter;
    bool currentlyReadOrWrite;
};

class StrideRandomReadWriteGenerator : public RequestGenerator
{
  public:
    StrideRandomReadWriteGenerator(uint32_t _start, uint32_t _stride, float pr) :
      addr(_start), stride(_stride), pRead(pr)
    {
    }

    virtual MemoryRequest * getNextRequest()
    {
      uint32_t oldAddr = addr;
      addr += stride;
      MemoryRequest *mr = new MemoryRequest(oldAddr, RNG.Prob(pRead));
      return mr;
    }

  private:
    uint32_t addr;
    uint32_t stride;
    float pRead;
};

class StreamGenerator
{
  public:
    StreamGenerator(DRAMDriver *d, int i, RequestGenerator &rg) : driver(d), index(i), rgen(rg){};

    virtual void PerCycle() = 0;

    //Pure virtual
    virtual void requestDone(uint32_t identifier) = 0;

    DRAMDriver *driver;
    int index;
    RequestGenerator &rgen;
};

class ConcurrentStreamGenerator : public StreamGenerator
{
  public:
    ConcurrentStreamGenerator(DRAMDriver *d, int i, RequestGenerator &rg, unsigned int _concurrency) :
      StreamGenerator::StreamGenerator(d, i, rg), concurrency(_concurrency), outstandingRequests(0),
      nextIdentifier(0)
    {
    }

    virtual void PerCycle();

    virtual void requestDone(uint32_t identifier)
    {
      //cerr << "Erasing identifier " << identifier << " from ConcurrentStreamGenerator #" << index << endl;
      for(std::list<MemoryRequest *>::iterator it = outstandingRequests.begin(); it != outstandingRequests.end(); ++it)
      {
        if((*it)->identifier == identifier)
        {
          delete (*it);
          outstandingRequests.erase(it);
          return;
        }
      }
			std::cerr << "Error: Identifier " << identifier << " not found for ConcurrentStreamGenerator #" << index << "\n";
      exit(1);
    }

  private:
    unsigned int concurrency;
    std::list<MemoryRequest *> outstandingRequests;
    unsigned int nextIdentifier;
};

#endif //#ifndef __STREAM_GENERATOR_H__
