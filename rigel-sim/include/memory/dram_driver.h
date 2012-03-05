#ifndef __DRAM_DRIVER_H__
#define __DRAM_DRIVER_H__

#include <cmath>
#include <vector>
#include <stdint.h>
#include <set>
#include "sim.h"
#include "util/util.h"
#include "mshr.h"
#include "stream_generator.h"
#include "memory_timing.h"
using namespace rigel;

//Used to aggregate several StreamGenerators' requests and present them to the memory model
//which in term hands them to the appropriate DRAMControllers.
//The DRAMDriver will get a request from a StreamGenerator with a request identifier,
//append some bits to the identifier corresponding to the index of the StreamGenerator
//from which the request came, then send it on to the MemoryModel.
//When the DRAMController calls callback(), the information that the request is done will be forwarded
//To the appropriate StreamGenerator by looking at the StreamGenerator identifier bits.
//NOTE that this means the request identifier must fit in 32-log2(# of StreamGenerators) bits.
class DRAMDriver : public CallbackInterface
{
  public:
    DRAMDriver(MemoryTimingDRAM &mm) : memory_model(mm) { }
    virtual void callback(int a, int b, uint32_t c)
    {
      if(streams.size() == 1)
      {
        streams[0]->requestDone(a);
        return;
      }
      uint32_t streamIndex = ((uint32_t)a) >> (32-numIndexBits);
      if(streamIndex >= streams.size())
      {
				std::cerr << "Error: Callback called with stream identifier " << streamIndex
             << " not in [0 " << streams.size() << ")\n";
        exit(1);
      }
      uint32_t mask = ((1 << (32-numIndexBits)) - 1);
      //cout << "a: " << std::hex << a << ", streamIndex: " << streamIndex << ", mask: " << mask << endl;
      streams[streamIndex]->requestDone(a & mask);
    }

    void addStream(StreamGenerator *stream)
    {
      streams.push_back(stream);
      numIndexBits = floor(log(streams.size()-1)/log(2)) + 1;
    }

    int numStreams() const
    {
      return streams.size();
    }

    bool SendRequest(uint32_t addr, bool readNotWrite, uint32_t streamIndex, uint32_t identifier)
    {
      if(streams.size() > 1 && identifier > (uint32_t)(1 << (32-rigel_log2(streams.size()))) - 1)
      {
				std::cerr << "Error: Identifier 0x" << std::hex << identifier << " is too large with " << streams.size() << " streams.\n";
        exit(1);
      }
      MissHandlingEntry<rigel::cache::LINESIZE> mshr;
      mshr.set(addr, 1, true, (readNotWrite) ? IC_MSG_READ_REQ : IC_MSG_WRITE_REQ, -10,  1, rigel::POISON_TID);
      uint32_t finalIdentifier = (streams.size() == 1) ? identifier
                                                       : ((streamIndex << (32-numIndexBits)) | identifier);
      //std::cerr << std::hex << streamIndex << " " << identifier << " " << finalIdentifier << "\n";
      std::set<uint32_t> s;
      s.insert(addr);
      return memory_model.Schedule(s, rigel::cache::LINESIZE, mshr, this, finalIdentifier);
    }

    void PerCycle()
    {
      for(std::vector<StreamGenerator *>::iterator it = streams.begin(); it != streams.end(); ++it)
        (*it)->PerCycle();
    }

    MemoryTimingDRAM &memory_model;
		std::vector<StreamGenerator *> streams;
    uint32_t numIndexBits;
};

#endif //#ifndef __DRAM_DRIVER_H__
