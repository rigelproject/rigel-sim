#ifndef __DRAM_CONTROLLER_H__
#define __DRAM_CONTROLLER_H__

#include <set>
#include <limits>
#include <deque>
#include <map>
#include <stdint.h>

#include "sim.h"
#include "memory/dram_channel_model.h"
#include "mshr.h"
#include "memory/address_mapping.h"

// Forward declarations
class GlobalCache;
class CallbackInterface;

template<int T> class MissHandlingEntry;

struct PendingRequestEntry {

  PendingRequestEntry() {
    Reset();
  }

  bool Valid; // Is a request actually pending at the DRAM Controller?
  std::set<uint32_t> addrs;
  MissHandlingEntry<rigel::cache::LINESIZE> MSHR; // The associated MSHR for the request
  uint32_t Data[rigel::cache::LINESIZE/sizeof(uint32_t)]; // The data that we will want for the fill
  bool ReadNOTWrite; // Read (false Write)
  // Fields for in-flight instructions:
  int BurstCount;
  // Which bank is this request working on?
  int Rank;
  int Bank;
  // What index is this request within its pending request buffer?
  int Slot;
  // Is the request currently being serviced?
  bool InProgress;
  
  std::set<ReadyLinesEntry> ReadyLines;
  
  // Keep track of how long this request has been pending.  If it exceeds the
  // threshold, we reset it and let it try and reschedule
  int WDTCycles;
  //These 2 are so we can do a callback once the request is complete.
  //We will call Requester->callback(RequestIdentifier, 0) for historical
  //reasons (0 means "0 cycles until data is ready" = "request is done")
  CallbackInterface *Requester;
  int RequestIdentifier;
  // Cycle where this request was first attempted.
  uint64_t FirstAttemptCycle;
  // Cycle when this request was inserted into the request buffer
  uint64_t InsertionCycle;
  uint64_t RequestNumber; //Incrementing counter, per-channel.
  uint64_t BatchNumber; //Used when rigel::MC_BATCHING is enabled to enforce
  //partial orders on batches of requests (for fairness)

  bool AllLinesPending() const
  {
    return(ReadyLines.size() == addrs.size());
  }

  void SetLineReady(uint32_t addr, uint64_t cyclesFromNow)
  {
    if(addrs.count(addr) == 0)
    {
      fprintf(stderr, "Error: Request is not waiting on address 0x%08x\n", addr);
      dump();
      assert(0);
    }
    if(ReadyLines.size() >= addrs.size())
    {
      fprintf(stderr, "Error: Request already has %zu ready lines and %zu pending, why push "
      "another one (addr %08x)?\n", ReadyLines.size(), addrs.size(), addr);
      dump();
      assert(0);
    }

    std::set<ReadyLinesEntry>::iterator it = ReadyLines.find(ReadyLinesEntry(addr, 0));
    if(it != ReadyLines.end())
    {
      fprintf(stderr, "Error!  Setting line %08x ready at cycle %08"PRIx64", but it's "
                      "already ready (%08x) at cycle %08"PRIX64"\n",
              addr, DRAMChannelModel::GetDRAMCycle()+cyclesFromNow, it->getAddr(), it->getReadyCycle());
      dump();
      assert(0);
    }

    ReadyLinesEntry rle(addr, DRAMChannelModel::GetDRAMCycle()+cyclesFromNow);
    ReadyLines.insert(rle);
  }

  bool HasReadyLine() const
  {
    if (ReadyLines.empty()) { return false; }
    else { return (ReadyLines.begin()->getReadyCycle() <= DRAMChannelModel::GetDRAMCycle()); }
  }

  uint32_t GetReadyAddress() const
  {
    if (ReadyLines.empty()) {
      rigel::GLOBAL_debug_sim.dump_all();
      rigel_dump_backtrace();
      dump();
      assert(0);
      return 0;
    } else {
      return ReadyLines.begin()->getAddr();
    }
  }

  void ClearReadyLine(uint32_t addr)
  {
    assert(!ReadyLines.empty() && "No lines ready to fill!");
    assert(addr == ReadyLines.begin()->getAddr() && "Address mismatch!");
    ReadyLines.erase(ReadyLines.begin());
    if (addrs.erase(addr & (~0x1F)) == 0)
    {
      fprintf(stderr, "ERROR: DRAM Request Did not contain address %08x\n", addr & (~0x1F));
      rigel::GLOBAL_debug_sim.dump_all();
      rigel_dump_backtrace();
      dump();
      assert(0);
    }
  }

  bool IsPending(uint32_t addr)
  {
    return (ReadyLines.count(ReadyLinesEntry(addr, 0)) != 0);
  }

  void Reset() {
    Valid = false;
    InProgress = false;
    //FIXME make this more generic
    BurstCount = (rigel::cache::LINESIZE / 4);
    assert(ReadyLines.empty() && "DRAM Request ReadyLines is not empty!"); 
    WDTCycles = rigel::mem_sched::MC_PENDING_WDT;
    Requester = NULL;
    RequestIdentifier = -1;
    FirstAttemptCycle = U64_MAX_VAL;
    RequestNumber = U64_MAX_VAL;
    BatchNumber = U64_MAX_VAL;
    addrs.clear();
  }

  void dump() const;
};

using namespace rigel::DRAM;
class MemoryTimingDRAM;

typedef struct _RowCacheEntry
{
  void reset() { row = -1; dirty = false; numReads = 0; numWrites = 0; }
  uint64_t cycleAllocated;
  uint64_t lastRead;
  uint64_t lastWritten;
  int numReads;
  int numWrites;
  int row;
  bool dirty;
} RowCacheEntry;

typedef struct _TimingInfo {

  enum DRAMBankState
  {
    DRAMBANKSTATE_IDLE,
    DRAMBANKSTATE_ACTIVATING,
    DRAMBANKSTATE_ACTIVE,
    DRAMBANKSTATE_PRECHARGING
  };

  enum RowCacheState
  {
    ROWCACHESTATE_IDLE,
    ROWCACHESTATE_EVICT_PRECHARGING,
    ROWCACHESTATE_EVICT_ACTIVATING,
    ROWCACHESTATE_EVICT_WRITING,
    ROWCACHESTATE_PRECHARGING,
    ROWCACHESTATE_ACTIVATING,
    ROWCACHESTATE_READING
  };

  typedef struct _ActivateTracker
  {
    _ActivateTracker(uint64_t c, int b) : cycle(c), bank(b) {};
    uint64_t cycle;
    int bank;
  } ActivateTracker;

  std::set<uint64_t> AddressBusBusy;
  //Note that, for now, CommandBusBusy is not used.
  //This is because it would not be meaningful, since we schedule one access at a time,
  //and do not schedule commands which cannot be issued until a future cycle.
  //On the other hand, a single command may have multiple cycles of address, so we need to handle that.
  std::set<uint64_t> CommandBusBusy;
  std::set<uint64_t> DataBusBusy;

  DRAMChannelModel *model;

  DRAMBankState **State;
  RowCacheState **CacheState;
  uint64_t **PRE;
  uint64_t **ACT;
  uint64_t **READ;
  uint64_t **WRITE;
  uint64_t **ReadData;
  uint64_t **WriteData;
  int **ActiveRow;
  RowCacheEntry ***CacheRows;
  int **victim;
	std::deque<ActivateTracker> *LastNActivates;

  _TimingInfo()
  {
    State = new DRAMBankState * [RANKS];
    CacheState = new RowCacheState * [RANKS];
    PRE = new uint64_t * [RANKS];
    ACT = new uint64_t * [RANKS];
    READ = new uint64_t * [RANKS];
    WRITE = new uint64_t * [RANKS];
    ReadData = new uint64_t * [RANKS];
    WriteData = new uint64_t * [RANKS];
    ActiveRow = new int * [RANKS];
    CacheRows = new RowCacheEntry ** [RANKS];
    victim = new int * [RANKS];
    LastNActivates = new std::deque<ActivateTracker> [RANKS];
    // Make sure that initially all timers are set to -infinity
    for (unsigned int i = 0; i < RANKS; i++)
    {
      State[i] = new DRAMBankState [BANKS];
      CacheState[i] = new RowCacheState [BANKS];
      PRE[i] = new uint64_t [BANKS];
      ACT[i] = new uint64_t [BANKS];
      READ[i] = new uint64_t [BANKS];
      WRITE[i] = new uint64_t [BANKS];
      ReadData[i] = new uint64_t [BANKS];
      WriteData[i] = new uint64_t [BANKS];
      ActiveRow[i] = new int [BANKS];
      CacheRows[i] = new RowCacheEntry * [BANKS];
      victim[i] = new int [BANKS];
      for (unsigned int j = 0; j < BANKS; j++)
      {
        State[i][j] = DRAMBANKSTATE_IDLE;
        CacheState[i][j] = ROWCACHESTATE_IDLE;
        PRE[i][j] = UINT64_MAX;
        ACT[i][j] = UINT64_MAX;
        READ[i][j] = UINT64_MAX;
        WRITE[i][j] = UINT64_MAX;
        ReadData[i][j] = 0;
        WriteData[i][j] = 0;
        ActiveRow[i][j] = -1;
        CacheRows[i][j] = new RowCacheEntry [OPEN_ROWS];
        victim[i][j] = -1;
        for (unsigned int k = 0; k < OPEN_ROWS; k++)
          CacheRows[i][j][k].reset();
      }
    }
  }

  inline int findInCache(unsigned int rank, unsigned int bank, unsigned int row)
  {
    for(unsigned int i = 0; i < rigel::DRAM::OPEN_ROWS; i++)
      if(CacheRows[rank][bank][i].row == (int)row)
        return i;
    return -1;
  }

  //Here we use an LRU eviction policy that does not take into account clean vs. dirty
  //First invalid, then least recently accessed
  //TODO: Implement, LRA, LRR, LRW, clean-first, etc.
  inline int findVictim(unsigned int rank, unsigned int bank)
  {
    using namespace rigel::mem_sched;
    //printf("Finding victim\n");
    //for(unsigned int i = 0; i < rigel::DRAM::OPEN_ROWS; i++)
    //{
    //  RowCacheEntry & rce = CacheRows[rank][bank][i];
    //  printf("Entry %u: Row %d Dirty %01d LastRead: %"PRIu64" LastWritten: %"PRIu64"\n", i, rce.row, rce.dirty, rce.lastRead, rce.lastWritten);
    //}
    switch(ROW_CACHE_REPLACEMENT_POLICY)
    {
      case rc_replace_lru:
      {
        uint64_t leastRecentUse = UINT64_MAX;
        int leastRecentUseIndex = -1;
        for(unsigned int i = 0; i < rigel::DRAM::OPEN_ROWS; i++)
        {
          RowCacheEntry & rce = CacheRows[rank][bank][i];
          if(rce.row == -1)
          {
            //printf("Choosing entry %u\n", i);
            return i;
          }
          uint64_t lastUse = (rce.lastRead > rce.lastWritten) ? rce.lastRead : rce.lastWritten;
          if(lastUse < leastRecentUse)
          {
            leastRecentUse = lastUse;
            leastRecentUseIndex = i;
          }
        }
        assert((leastRecentUseIndex != -1) && "No row cache entry has been used!");
        //printf("Choosing entry %u\n", leastRecentUseIndex);
        return leastRecentUseIndex;
      }
      case rc_replace_tristage:
      {
        //Keep track of LRU info for clean and dirty lines separately
        uint64_t leastRecentUseClean = UINT64_MAX, leastRecentUseDirty = UINT64_MAX;
        int leastRecentUseIndexClean = -1, leastRecentUseIndexDirty = -1;
        int activeRowIndex = -1;
        for(unsigned int i = 0; i < rigel::DRAM::OPEN_ROWS; i++)
        {
          RowCacheEntry & rce = CacheRows[rank][bank][i];
          //Always choose invalid entries first
          if(rce.row == -1)
          {
            //printf("Choosing entry %u\n", i);
            return i;
          }
          if(rce.row == ActiveRow[rank][bank])
            activeRowIndex = i;
          uint64_t lastUse = (rce.lastRead > rce.lastWritten) ? rce.lastRead : rce.lastWritten;
          if(rce.dirty) //update dirty lru info
          {
            if(lastUse < leastRecentUseDirty)
            {
              leastRecentUseDirty = lastUse;
              leastRecentUseIndexDirty = i;
            }
          }
          else
          {
            if(lastUse < leastRecentUseClean)
            {
              leastRecentUseClean = lastUse;
              leastRecentUseIndexClean = i;
            }
          }
        }
        assert(((leastRecentUseIndexClean != -1) || (leastRecentUseIndexDirty != -1)) && "No row cache entry has been used!");
        if(leastRecentUseIndexClean != -1)
        {
          //printf("CLEAN\n");
          return leastRecentUseIndexClean;
        }
        else
        {
          if(activeRowIndex != -1) //Currently active row in DRAM array is in cache and dirty, write that back
          {
            //printf("ACTIVE\n");
            return activeRowIndex;
          }
          else
          {
            //printf("DIRTY\n");
            return leastRecentUseIndexDirty;
          }
        }
      }
      case rc_replace_mru:
      {
        uint64_t mostRecentUse = 0UL;
        int mostRecentUseIndex = -1;
        for(unsigned int i = 0; i < rigel::DRAM::OPEN_ROWS; i++)
        {
          RowCacheEntry & rce = CacheRows[rank][bank][i];
          if(rce.row == -1)
          {
            //printf("Choosing entry %u\n", i);
            return i;
          }
          uint64_t lastUse = (rce.lastRead > rce.lastWritten) ? rce.lastRead : rce.lastWritten;
          if(lastUse > mostRecentUse)
          {
            mostRecentUse = lastUse;
            mostRecentUseIndex = i;
          }
        }
        assert((mostRecentUseIndex != -1) && "No row cache entry has been used!");
        //printf("Choosing entry %u\n", leastRecentUseIndex);
        return mostRecentUseIndex;
      }
      case rc_replace_lfu:
      {
        double leastFrequentUse = std::numeric_limits<double>::max();
        int leastFrequentUseIndex = -1;
        for(unsigned int i = 0; i < rigel::DRAM::OPEN_ROWS; i++)
        {
          RowCacheEntry & rce = CacheRows[rank][bank][i];
          if(rce.row == -1)
          {
            //printf("Choosing entry %u\n", i);
            return i;
          }
          double touchesPerCycle = (double)(rce.numReads+rce.numWrites) / (double)(DRAMChannelModel::GetDRAMCycle() - rce.cycleAllocated + 1);
          if(touchesPerCycle < leastFrequentUse)
          {
            leastFrequentUse = touchesPerCycle;
            leastFrequentUseIndex = i;
          }
        }
        assert((leastFrequentUseIndex != -1) && "No row cache entry has been used!");
        //printf("Choosing entry %u\n", leastFrequentUseIndex);
        return leastFrequentUseIndex;
      }
      default:
        assert(0 && "Invalid row cache replacement policy!");
    }
  }

  inline void setAddressBusBusy(uint64_t cycle) { AddressBusBusy.insert(cycle); }
  inline void setCommandBusBusy(uint64_t cycle) { CommandBusBusy.insert(cycle); }
  inline void setDataBusBusy(uint64_t cycle)    { DataBusBusy.insert(cycle);    }
  inline bool getAddressBusBusy(uint64_t cycle) { return (AddressBusBusy.count(cycle) != 0); }
  inline bool getCommandBusBusy(uint64_t cycle) { return (CommandBusBusy.count(cycle) != 0); }
  inline bool getDataBusBusy(uint64_t cycle)    { return (DataBusBusy.count(cycle) != 0);    }

  inline void purgeOldAddressBusBusyEntries(uint64_t cycle)   { AddressBusBusy.erase(AddressBusBusy.begin(),
                                                                                     AddressBusBusy.lower_bound(cycle)); }
  inline void purgeOldCommandBusBusyEntries(uint64_t cycle)   { CommandBusBusy.erase(CommandBusBusy.begin(),
                                                                                     CommandBusBusy.lower_bound(cycle)); }
  inline void purgeOldDataBusBusyEntries(uint64_t cycle)      { DataBusBusy.erase(DataBusBusy.begin(),
                                                                                  DataBusBusy.lower_bound(cycle)); }

  inline void setACT(int rank, int bank, int row, bool backdoor)
  {
    uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();
    //Set last-ACT time for this bank
    ACT[rank][bank] = CurrCycle;
    //This row is now the active row in the bank
     if(rigel::DRAM::OPEN_ROWS == 1 || backdoor) //FIXME we should have an ENABLE_ROW_CACHE or something
      ActiveRow[rank][bank] = row;
    //Keep track of ACTs in the LastNActivates window to check for tRRD, tFAW, etc.
    ActivateTracker track(CurrCycle, bank);
    LastNActivates[rank].push_back(track);
    //Make sure we only keep track of the last MAX_ACTIVATES_IN_WINDOW ACTs (otherwise the queue could get pretty long)
    while((int)LastNActivates[rank].size() > rigel::DRAM::MAX_ACTIVATES_IN_WINDOW) //hopefully just once
    {
      LastNActivates[rank].pop_front();
    }
    assert(State[rank][bank] == DRAMBANKSTATE_IDLE && "Error: Trying to activate when the bank isn't IDLE");
    State[rank][bank] = DRAMBANKSTATE_ACTIVATING;

    if(!backdoor)
    {
      setCommandBusBusy(CurrCycle);
      setAddressBusBusy(CurrCycle);
    }
  }

  inline void setPRE(int rank, int bank, bool backdoor)
  {
    uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();
    PRE[rank][bank] = CurrCycle;
    assert(State[rank][bank] == DRAMBANKSTATE_ACTIVE && "Error: Trying to precharge when the row isn't ACTIVE");
    State[rank][bank] = DRAMBANKSTATE_PRECHARGING;
    ActiveRow[rank][bank] = -1;
    if(!backdoor)
    {
      setCommandBusBusy(CurrCycle);
      setAddressBusBusy(CurrCycle);
    }
  }

  //TODO: Handle variable-length bursts, DDR addressing, etc.
  //For now, just assume everything will read out after exactly CL.  This may actually be the way we want to do it.
  inline void setREAD(int rank, int bank, int row, bool backdoor)
  {
    assert((State[rank][bank] == DRAMBANKSTATE_ACTIVE || State[rank][bank] == DRAMBANKSTATE_ACTIVATING) && "Error: Trying to read when the row isn't ACTIVE or ACTIVATING");
    State[rank][bank] = DRAMBANKSTATE_ACTIVE;
    uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();
    READ[rank][bank] = CurrCycle;
    ReadData[rank][bank] = CurrCycle + rigel::DRAM::LATENCY::CL + (rigel::DRAM::BURST_SIZE/2) - 1;

    if(!backdoor)
    {
      setCommandBusBusy(CurrCycle);
      setAddressBusBusy(CurrCycle);
      //TODO: Handle non-cache-line bursts, make sure this works in the rest of the system if BURST_SIZE != 8, handle non-DDR (2 should not be hardcoded)
      for(int i = 0; i < (rigel::DRAM::BURST_SIZE/2); i++)
      {
        setDataBusBusy(CurrCycle+rigel::DRAM::LATENCY::CL+i);
      }
    }
  }

  //TODO: Maybe different read and write burst sizes?
  //TODO: Handle variable-length bursts, DDR addressing, etc.
  //For now, just assume everything will read out after exactly WL.  This may actually be the way we want to do it.
  inline void setWRITE(int rank, int bank, int row, bool backdoor)
  {
    assert((State[rank][bank] == DRAMBANKSTATE_ACTIVE || State[rank][bank] == DRAMBANKSTATE_ACTIVATING) && "Error: Trying to write when the row isn't ACTIVE or ACTIVATING");
    State[rank][bank] = DRAMBANKSTATE_ACTIVE;
    uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();
    WRITE[rank][bank] = CurrCycle;
    if(!backdoor)
      WriteData[rank][bank] = CurrCycle + rigel::DRAM::LATENCY::WL + (rigel::DRAM::BURST_SIZE/2) - 1;
    else
      WriteData[rank][bank] = CurrCycle;
    if(!backdoor)
    {
      setCommandBusBusy(CurrCycle);
      setAddressBusBusy(CurrCycle);
      //TODO: Handle non-cache-line bursts, make sure this works in the rest of the system if BURST_SIZE != 8, handle non-DDR (2 should not be hardcoded)
      for(int i = 0; i < (rigel::DRAM::BURST_SIZE/2); i++)
      {
        setDataBusBusy(CurrCycle+rigel::DRAM::LATENCY::WL+i);
      }
    }
  }

  inline bool WRITEHasHappenedSinceLastACT(int rank, int bank)
  {
    return ((WRITE[rank][bank] > ACT[rank][bank]) && (WRITE[rank][bank] != UINT64_MAX));
  }

  inline bool READHasHappenedSinceLastACT(int rank, int bank)
  {
    return ((READ[rank][bank] > ACT[rank][bank]) && (READ[rank][bank] != UINT64_MAX));;
  }

  //Get cycle number of last CAS (READ or WRITE command) across all ranks
  inline uint64_t getLastCAS()
  {
    uint64_t lastCAS = READ[0][0];
    for(unsigned int rank = 0; rank < RANKS; rank++)
    {
      for(unsigned int bank = 0; bank < BANKS; bank++)
      {
        if((READ[rank][bank] > lastCAS) && (READ[rank][bank] != UINT64_MAX)) lastCAS = READ[rank][bank];
        if((WRITE[rank][bank] > lastCAS) && (WRITE[rank][bank] != UINT64_MAX)) lastCAS = WRITE[rank][bank];
      }
    }
    if(lastCAS == UINT64_MAX) lastCAS = 0;
    return lastCAS;
  }
  //Get cycle number of last CAS (READ or WRITE command) for the specified rank.
  //Returns 0 if the specified rank has never been CASed.
  inline uint64_t getLastCAS(unsigned int rank)
  {
    uint64_t lastCAS = READ[0][0];
    for(unsigned int bank = 0; bank < BANKS; bank++)
    {
      if((READ[rank][bank] > lastCAS) && (READ[rank][bank] != UINT64_MAX)) lastCAS = READ[rank][bank];
      if((WRITE[rank][bank] > lastCAS) && (WRITE[rank][bank] != UINT64_MAX)) lastCAS = WRITE[rank][bank];
    }
    if(lastCAS == UINT64_MAX) lastCAS = 0;
    return lastCAS;
  }
 
  bool canACT(int rank, int bank, bool backdoor);

  bool canREAD(int rank, int bank, int row, bool backdoor);

  bool canWRITE(int rank, int bank, int row, bool backdoor);

  bool canPRE(int rank, int bank, bool backdoor);

} TimingInfo;

class DRAMController
{
  private:
    int id;
    unsigned int priorityRank;
    unsigned int priorityBank;
    DRAMChannelModel *model;
    //These are used for quantifying the extent of request reordering in the memory controller.
    //A histogram of the reorder density, as specified in RFC5236, is stored in STATNAME_MC_REORDERING
    uint64_t inputRequestCounter;
    uint64_t outputRequestCounter;
    std::set<uint64_t> outOfOrderRequests;
    //Used for batching
    //Maps from batch number to number of outstanding requests for that batch.  Entries should be removed when
    //all requests from a batch are serviced and the count reaches 0.
    //Entries should be decremented (to denote having been serviced) either when the request is selected as a pendingRequest
    //or when it is finished (when data gets back).  I think the latter approach ensures fairness better.
    std::map<uint64_t, unsigned int> outstandingBatchRequests;
    //The batch number we are currently assigning to incoming requests
    uint64_t issuingBatchCounter;
    //The batch we are prioritizing for outgoing requests
    uint64_t servicingBatchCounter;
    
    //Number of requests ever assigned to current batch.  Used in Schedule() to figure out when to cut off a batch
    //When we are using the perchannel policy.
    unsigned int requestsInCurrentBatch;
  public:
    PendingRequestEntry *** requestBuffer;
    int ** requestBufferOccupancy;
    int totalRequestBufferOccupancy;
    PendingRequestEntry ** pendingRequest;
    TimingInfo timingInfo;
    bool collisionChecking;
    uint64_t ** linesServiced;
    uint64_t ** requestsServiced;

    
    DRAMController(int controllerID, DRAMChannelModel *channelModel, bool _collisionChecking);

    int getID() const
    {
      return id;
    }

    // Returns true if a row hit is found.
    // If it returns true, rowHit will point to the row hit entry.
    bool helper_FindRowHit(unsigned int rank, unsigned int bank, PendingRequestEntry* &rowHit);
    // Helper function for testing if any of the WDTs have expired
    bool helper_FindTimeout(unsigned int rank, unsigned int bank, PendingRequestEntry* &oldRequest);
    //Helper function to gather per-cycle, per-controller statistics
    void helper_GatherStatistics();
    bool helper_IsRowHit(PendingRequestEntry* const &request) const;
    void PerCycle();

    // Try to schedule a request
    void Schedule_SinglePendingRequest();
    void Schedule_PerBankPendingRequests();
    void (DRAMController::*myScheduler)();

    // Try to schedule request set in PendingRequest for this controller.
    // Return true if we were able to make forward progress on this request
    // The result can be used for potential new requests to see if they are ready
    bool helper_HandleRequest(PendingRequestEntry* &request);
    bool helper_HandleRequestRowCache(PendingRequestEntry* &request); //Special version with row cache
    bool (DRAMController::*myRequestHandler)(PendingRequestEntry* &request); //Function pointer to decide
    //which one to use (set in constructor based on rigel::DRAM::OPEN_ROWS)

    // Compute the extent of request reordering and increment the histogram.
    void helper_HandleReordering(uint64_t RequestNumber);
    bool Schedule(const std::set<uint32_t> addrs, int size,
                  const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
                  class CallbackInterface *requestingEntity, int requestIdentifier);

    // Check to see if there is nothing available for the controller to even
    // attempt to schedule.  Simulator performance optimization.
    bool SchedWindowEmpty() const {
      return (totalRequestBufferOccupancy == 0);
    }

    // Dump current state of the memory controller for debugging purposes.
    void dump(bool pendingOnly) const;

    // Decrement all WDTs for valid requests.
    void helper_DecrementWDTs();

    // Look at which requests have lines ready (data transfer has completed) and handle
    // notifying the G$ and possibly clearing the request if all lines in the request have
    // completed
    void helper_HandleReadyLines();
    
};

#endif //#ifndef __DRAM_CONTROLLER_H__
