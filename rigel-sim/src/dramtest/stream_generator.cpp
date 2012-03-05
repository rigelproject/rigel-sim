#include <stddef.h>                     // for NULL
#include <list>                         // for list, _List_iterator, etc
#include "memory/dram_driver.h"    // for DRAMDriver
#include "read_mem_trace_c.h"  // for get_next_thread_access, etc
#include "stream_generator.h"

extern "C" {
}

void ConcurrentStreamGenerator::PerCycle()
{
  while(outstandingRequests.size() < concurrency)
  {
    MemoryRequest *mr;
    bool uniqueAddress;
    do
    {
      uniqueAddress = true;
      mr = rgen.getNextRequest();
      //for(std::list<MemoryRequest *>::iterator it = outstandingRequests.begin(); it != outstandingRequests.end(); ++it)
      //  if((*it)->addr == mr->addr)
      //  {
      //    uniqueAddress = false;
      //    break;
      //  }
    } while(!uniqueAddress);

    mr->identifier = nextIdentifier;
    nextIdentifier++;
    outstandingRequests.push_back(mr);
  }
  for(std::list<MemoryRequest *>::iterator it = outstandingRequests.begin(); it != outstandingRequests.end(); ++it)
  {
    MemoryRequest &mr = *(*it);
    if(!mr.scheduled)
    {
      if(driver->SendRequest(mr.addr, mr.readNotWrite, index, mr.identifier))
        mr.scheduled = true;
    }
  }
}

MemoryRequest * GzipRequestGenerator::getNextRequest()
{
  struct extended_info ei;
  if(get_next_thread_access(mtr, thread, &ei) == 0)
  {
    if(buffer_one_sw_thread(mtr, thread) == 0)
    {
      return NULL;
    }
    else
    {
      if(get_next_thread_access(mtr, thread, &ei) == 0)
        return NULL;
    }
  }
  //ei has a valid access
  MemoryRequest *mr = new MemoryRequest(ei.ai.addr, (ei.read == 1));
  //printf("R");
  return mr;
}
