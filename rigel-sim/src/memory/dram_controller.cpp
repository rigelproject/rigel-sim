
////////////////////////////////////////////////////////////////////////////////
// dram_controller.cpp
////////////////////////////////////////////////////////////////////////////////


#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu64
#include <stdint.h>                     // for uint32_t, uint64_t, etc
#include <stdio.h>                      // for fprintf, stderr, NULL, etc
#include <stdlib.h>                     // for exit
#include <sys/types.h>                  // for int32_t
#include <deque>                        // for deque, etc
#include <iterator>                     // for reverse_iterator
#include <map>                          // for _Rb_tree_const_iterator, etc
#include <set>                          // for set, set<>::const_iterator, etc
#include <deque>                        // for deque
#include "memory/address_mapping.h"  // for AddressMapping
#include "define.h"         // for DEBUG_HEADER, etc
#include "memory/dram.h"           // for BANKS, RANKS, WL, CL, etc
#include "memory/dram_channel_model.h"  // for DRAMChannelModel
#include "memory/dram_controller.h"  // for DRAMController, etc
#include "mshr.h"           // for MissHandlingEntry, etc
#include "profile/profile.h"        // for ProfileStat, MemStat, etc
#include "profile/profile_names.h"
#include "sim.h"            // for PENDING_PER_BANK, stats, etc
#include "util/util.h"           // for CallbackInterface
#if 0
#define DEBUG_PRINT_MC_TRACE(c, x) do { \
  DEBUG_HEADER(); \
  fprintf(stderr, "MC_TRACE: ctrl: %d %s\n", c, x); \
} while (0);
#else
#define DEBUG_PRINT_MC_TRACE(c, x)
#endif

//#define DEBUG_CYCLE 0x1
#undef DEBUG_CYCLE

//#define DRAM_CONTROLLER_DEBUG(x) do { if(DRAMChannelModel::GetDRAMCycle() > 4440000 && DRAMChannelModel::GetDRAMCycle() < 4540000) { x; } } while(0)

#define DRAM_CONTROLLER_DEBUG(x)

////////////////////////////////////////////////////////////////////////////////
// constructor
////////////////////////////////////////////////////////////////////////////////
DRAMController::DRAMController(
  int controllerID, 
  DRAMChannelModel *channelModel, 
  bool _collisionChecking
) :
  id(controllerID), 
  model(channelModel), 
  inputRequestCounter(0UL), 
  outputRequestCounter(0UL),
  issuingBatchCounter(0UL), 
  servicingBatchCounter(0UL), 
  requestsInCurrentBatch(0),
  collisionChecking(_collisionChecking)
{
  using namespace rigel::DRAM;

  //Initialize first batching map entry (the rest will be taken care of on batch boundaries in Schedule())
  outstandingBatchRequests[0] = 0;

  //Initialize buffer structures
  if(rigel::mem_sched::DRAM_SCHEDULING_POLICY == rigel::mem_sched::single)
  {
    myScheduler = &DRAMController::Schedule_SinglePendingRequest;
    pendingRequest = new PendingRequestEntry *[1];
    pendingRequest[0] = NULL;
  }
  else if(rigel::mem_sched::DRAM_SCHEDULING_POLICY == rigel::mem_sched::perbank)
  {
    myScheduler = &DRAMController::Schedule_PerBankPendingRequests;
    pendingRequest = new PendingRequestEntry *[RANKS*BANKS];
    for(unsigned int i = 0; i < RANKS*BANKS; i++)
      pendingRequest[i] = NULL;
  }

  if(rigel::DRAM::OPEN_ROWS == 1) //no row cache
    myRequestHandler = &DRAMController::helper_HandleRequest;
  else
    myRequestHandler = &DRAMController::helper_HandleRequestRowCache;

  totalRequestBufferOccupancy = 0;
  requestBuffer = new PendingRequestEntry ** [RANKS];
  requestBufferOccupancy = new int * [RANKS];
  linesServiced = new uint64_t * [RANKS];
  requestsServiced = new uint64_t * [RANKS];
  // Build the buffers to hold pending memory requests.
  // Clear the scheduling priority rank/bank.
  priorityRank = 0;
  priorityBank = 0;
  // 4 is an arbitrary number, but >= 4 requests/bank is a lower bound on what
  // you would reasonably want.
  //assert(rigel::mem_sched::PENDING_PER_BANK >= 4 && "Disallow fewer than 4 entries in GDDR4 (arbitrary)");

  for (unsigned int i = 0; i < RANKS; i++) {
    requestBuffer[i] = new PendingRequestEntry * [BANKS];
    requestBufferOccupancy[i] = new int [BANKS];
    requestsServiced[i] = new uint64_t [BANKS];
    linesServiced[i] = new uint64_t [BANKS];
    for (unsigned int j = 0; j < BANKS; j++)
    {
      using namespace rigel::mem_sched;
      requestBuffer[i][j] = new PendingRequestEntry[PENDING_PER_BANK];
      for(int k = 0; k < PENDING_PER_BANK; k++)
      {
        requestBuffer[i][j][k].Reset();
        requestBuffer[i][j][k].Rank = i;
        requestBuffer[i][j][k].Bank = j;
        requestBuffer[i][j][k].Slot = k;
      }
      // Initially, nothing is in the request buffer.
      requestBufferOccupancy[i][j] = 0;
      requestsServiced[i][j] = 0;
      linesServiced[i][j] = 0;
    }
  }
  timingInfo.model = model;
}
// end constructor
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PerCycle()
////////////////////////////////////////////////////////////////////////////////
void DRAMController::PerCycle() {
  // Gate the DRAM model if the idealized model is in use.
  if (rigel::CMDLINE_ENABLE_IDEALIZED_DRAM) { return; }

  helper_GatherStatistics();
  helper_HandleReadyLines();
  if (!SchedWindowEmpty())
    (*this.*myScheduler)(); //Call function pointed to by myScheduler
  priorityBank = ((priorityBank + 1) % BANKS);
  if(priorityBank == 0) //Wrapped banks, switch to next rank
  {
    priorityRank = ((priorityRank + 1) % RANKS);
  }

  uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();
  //FIXME don't hardcode this?  is there a better place to do this periodic cleanup?
  if(CurrCycle % 2000 == 0 && CurrCycle > 1)
  {
    timingInfo.purgeOldAddressBusBusyEntries(CurrCycle-1);
    timingInfo.purgeOldCommandBusBusyEntries(CurrCycle-1);
    timingInfo.purgeOldDataBusBusyEntries(CurrCycle-1);
  }

  helper_DecrementWDTs();
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// helper_DecrementWDTs()
////////////////////////////////////////////////////////////////////////////////
// decrements watchdog timers?
////////////////////////////////////////////////////////////////////////////////
void DRAMController::helper_DecrementWDTs() {
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;
	if(totalRequestBufferOccupancy == 0)
		return;
  for(unsigned int i = 0; i < RANKS; i++) {
    for(unsigned int j = 0; j < BANKS; j++) {
			if(requestBufferOccupancy[i][j] == 0)
				continue;
      for(int k = 0; k < PENDING_PER_BANK; k++) {
        PendingRequestEntry &req = requestBuffer[i][j][k];
        if(req.Valid)
        {
          req.WDTCycles--;
          if (req.WDTCycles <= 0) {
            DEBUG_HEADER();
            fprintf(stderr, "Error: Request timed out in memory controller %d\n", id);
            req.dump();
            req.MSHR.dump();
            assert(0);
          }
        }
      }
    }
  }
}   
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// helperHandleReadyLines
////////////////////////////////////////////////////////////////////////////////
// TODO FIXME: what does this dO?
////////////////////////////////////////////////////////////////////////////////
void DRAMController::helper_HandleReadyLines() {
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;
	if(totalRequestBufferOccupancy == 0)
		return;

  uint64_t currCycle = DRAMChannelModel::GetDRAMCycle();
  for(unsigned int rank = 0; rank < RANKS; rank++) {
    for(unsigned int bank = 0; bank < BANKS; bank++) {
			if(requestBufferOccupancy[rank][bank] == 0)
				continue;
      for(int slot = 0; slot < PENDING_PER_BANK; slot++) {
        PendingRequestEntry &req = requestBuffer[rank][bank][slot];
        if(req.Valid)
        {
          if(!req.HasReadyLine()) continue;
          while(req.HasReadyLine())
          {
            uint32_t addr = req.GetReadyAddress();
            DRAM_CONTROLLER_DEBUG(printf("Data burst finished\n"));
            //Tell G$ the line is back
            req.Requester->callback(req.RequestIdentifier, 0, addr);
            req.ClearReadyLine(addr);
            linesServiced[rank][bank]++;
          }
          if(req.addrs.empty()) //All done!
          {
            DRAM_CONTROLLER_DEBUG(printf("Request finished\n"));
            //Tell the profiler about 1 request that took N DRAM cycles to complete
            rigel::profiler::stats[STATNAME_MC_LATENCY].inc_mem_histogram(currCycle - req.InsertionCycle, 1);
            //Handle reordering
            helper_HandleReordering(req.RequestNumber);
            //Decrement number of outstanding requests from batch.
            //If batch is empty, erase the entry to keep the map small.

            //printf("O in %"PRIu64" out %"PRIu64" numi %u numo %u\n",
            //  issuingBatchCounter, servicingBatchCounter,
            //  outstandingBatchRequests[issuingBatchCounter],
            //       outstandingBatchRequests[servicingBatchCounter]);

            //If we are batching, double-check that the request that is finishing is in the correct batch.
            if(DRAM_BATCHING_POLICY != batch_none && req.BatchNumber != servicingBatchCounter)
            {
              printf("Error: Request w/ batch %"PRIu64" got through, servicing %"PRIu64"\n",
              req.BatchNumber, servicingBatchCounter);
              assert(0);
            }
            outstandingBatchRequests[req.BatchNumber] = outstandingBatchRequests[req.BatchNumber] - 1;
            //If we are done servicing this batch and the scheduler has moved on from this batch
            //(meaning that we will never see another request from this batch), then we can move on to servicing
            //the next batch.  Don't erase it though.
            //TODO: Say why we should erase the entry in Schedule() and not here.
            if(outstandingBatchRequests[req.BatchNumber] == 0 && issuingBatchCounter > servicingBatchCounter)
            {
              servicingBatchCounter++;
            }
            //Reset the entry for somebody else to occupy later
            req.Reset();
            //One less valid entry in this bank
            requestBufferOccupancy[rank][bank]--;
            //One less valid entry overall
            totalRequestBufferOccupancy--;
            //One more entry serviced
            requestsServiced[rank][bank]++;
            
          }
          else
          {
            DRAM_CONTROLLER_DEBUG(printf("More data left in request\n"));
          }
        }
      }
    }
  }
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// helper_GatherStatistics()
/// * Helper method to gather any per-controller per-cycle statistics.
/// * Keeps PerCycle() short
/// * RETURNS: ---
//////////////////////////////////////////////////////////////////////////////////
void DRAMController::helper_GatherStatistics()
{
  //FIXME: This is icky, these should be normal profiler statistics
  if(totalRequestBufferOccupancy != 0 && ProfileStat::is_active())
    Profile::mem_stats.waiting_cycles[getID()]++;
}

//////////////////////////////////////////////////////////////////////////////////
// Schedule_SinglePendingRequest()
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
void DRAMController::Schedule_SinglePendingRequest()
{
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;

  // Try to find a row hit and use that to schedule first.
  PendingRequestEntry *rowHitRequest = NULL, *forcedRequest = NULL, *oldestForcedRequest = NULL;
  bool RowHit = false, ForceSchedule = false;
  bool foundForcedRequest = false;
  int32_t minWDTCycles = INT32_MAX;
  if(pendingRequest[0] == NULL)
  {
    for(unsigned int i = 0; i < RANKS; i++)
    {
      for(unsigned int j = 0; j < BANKS; j++)
      {
        ForceSchedule = helper_FindTimeout(i, j, forcedRequest);
        if(ForceSchedule)
        {
          foundForcedRequest = true;
          if(forcedRequest->WDTCycles < minWDTCycles)
          {
            oldestForcedRequest = forcedRequest;
            minWDTCycles = forcedRequest->WDTCycles;
          }
        }
        RowHit = helper_FindRowHit(i, j, rowHitRequest);
      }
    }
  }

  if(pendingRequest[0] == NULL)
  {
    if (foundForcedRequest) {
      // Scheduling was forced.  Do not update priority.
      DEBUG_PRINT_MC_TRACE(id, "ScheduleController() - Forced schedule.");
      pendingRequest[0] = oldestForcedRequest;
    }
    else if (RowHit) {
      pendingRequest[0] = rowHitRequest;
      rigel::profiler::stats[STATNAME_MC_SCHED_ROW_HIT].inc();
    }
    else {
      pendingRequest[0] = &(requestBuffer[priorityRank][priorityBank][DRAMChannelModel::GetDRAMCycle() % PENDING_PER_BANK]);
    }
  }

  assert(pendingRequest[0] != NULL && "Error: pendingRequest[0] is NULL after Schedule Force, Row Hit, and Priority");

  for (unsigned int rank_count = 0; rank_count < RANKS; rank_count++, pendingRequest[0] = &(requestBuffer[(pendingRequest[0]->Rank + 1) % RANKS][pendingRequest[0]->Bank][pendingRequest[0]->Slot]))
  {
    for (unsigned int bank_count = 0; bank_count < BANKS; bank_count++, pendingRequest[0] = &(requestBuffer[pendingRequest[0]->Rank][(pendingRequest[0]->Bank + 1) % BANKS][pendingRequest[0]->Slot]))
    {
      if(requestBufferOccupancy[pendingRequest[0]->Rank][pendingRequest[0]->Bank] == 0)
      {
        if (foundForcedRequest)
        {
          DEBUG_HEADER();
          fprintf(stderr, "Error: Forced schedule but bank's request buffer is empty\n");
          pendingRequest[0]->dump();
          assert(0);
        }
        continue;
      }

      // We know there is *a* pending request in this buffer.  Let's cycle through them in round-robin
      // priority order.
      for (int slot_count = 0; slot_count < PENDING_PER_BANK; slot_count++, pendingRequest[0] = &(requestBuffer[pendingRequest[0]->Rank][pendingRequest[0]->Bank][(pendingRequest[0]->Slot + 1) % PENDING_PER_BANK]))
      {
        // Skip empty entries
        if (!pendingRequest[0]->Valid) {
          if (foundForcedRequest) {
            DEBUG_HEADER();
            fprintf(stderr, "Error: Forced schedule but slot is invalid\n");
            pendingRequest[0]->dump();
            assert(0);
          }
          continue;
        }
        else {
          pendingRequest[0]->FirstAttemptCycle = rigel::CURR_CYCLE;
          pendingRequest[0]->InProgress = true;
          if ((*this.*myRequestHandler)(pendingRequest[0]))
            return;
          else
          {
            //If we are scheduling this access because it timed out, we shouldn't keep cycling through if we can't schedule something this cycle.  We need to stick with it until it finishes.
            if(foundForcedRequest)
              return;
            else
              continue;
          }
        }
      }
      if(foundForcedRequest)
        assert(0 && "Scheduler was forced, should have found a pending access!");
    }
  }
  pendingRequest[0] = NULL;
}
//////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////
// Schedule_PerBankPendingRequests()
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
void DRAMController::Schedule_PerBankPendingRequests()
{
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;

  PendingRequestEntry* temp;
  // Try to find a row hit and use that to schedule first.
  PendingRequestEntry *rowHitRequest = NULL, *forcedRequest = NULL;
  bool RowHit = false, ForceSchedule = false, TryingNewRequest = false;
 
  unsigned int startRank = priorityRank, startBank = priorityBank;

  int32_t minWDTCycles = INT32_MAX;
  bool foundForcedRequest = false;
  bool foundHardForcedRequest = false;
  //Need to prioritize bank with forced request every time, regardless of round-robin
  //priority scheme
  for(unsigned int _rank = 0, i = priorityRank; _rank < RANKS; _rank++, i = (i + 1) % RANKS)
  {
    for(unsigned int _bank = 0, j = priorityBank; _bank < BANKS; _bank++, j = (j + 1) % BANKS)
    {
      if(helper_FindTimeout(i, j, forcedRequest))
      {
        if(!foundForcedRequest || forcedRequest->WDTCycles < minWDTCycles)
        {
          foundForcedRequest = true;
          minWDTCycles = forcedRequest->WDTCycles;
          startRank = i; startBank = j;
        }
      }
    }
  }

  if((MC_PENDING_WDT - minWDTCycles) >= MC_FORCE_SCHEDULE_HARD)
    foundHardForcedRequest = true;
    
  //if(foundForcedRequest)
  //  fprintf(stderr, "%"PRIu64": Forced request found, starting with rank %u bank %u\n",
  //          DRAMChannelModel::GetDRAMCycle(), startRank, startBank);

  for(unsigned int _rank = 0, i = startRank; _rank < RANKS; _rank++, i = (i + 1) % RANKS)
  {
    for(unsigned int _bank = 0, j = startBank; _bank < BANKS; _bank++, j = (j + 1) % BANKS)
    {
      //Can skip all this if this bank is empty.
      if(requestBufferOccupancy[i][j] == 0)
        continue;
      temp = NULL;
      ForceSchedule = false;
      RowHit = false; //Need to reset to false for each bank.
      PendingRequestEntry* &currentPendingRequest = pendingRequest[i*BANKS + j];
      TryingNewRequest = (currentPendingRequest == NULL);
      if(TryingNewRequest)
      {
        RowHit = helper_FindRowHit(i, j, rowHitRequest);
        ForceSchedule = helper_FindTimeout(i, j, forcedRequest);
      }
      else
      {
        assert(currentPendingRequest->Valid && "Error: currentPendingRequest is not valid\n");
      }

      // If a bank was found with a row hit, try that one first.  It is assumed
      // that it will schedule this cycle if it has a row hit so the for loop
      // becomes a single iteration if it is found.

      if(TryingNewRequest)
      {
        if (ForceSchedule) {
          // Scheduling was forced.  Do not update priority.
          DEBUG_PRINT_MC_TRACE(id, "ScheduleController() - Forced schedule.");
          temp = forcedRequest;
        }
        else if (RowHit) {
          temp = rowHitRequest;
          rigel::profiler::stats[STATNAME_MC_SCHED_ROW_HIT].inc();
        }
        else {
          //FIXME The k index here is implemented the same as priorityBank/Rank.  Implement it that way.
          temp = &(requestBuffer[i][j][DRAMChannelModel::GetDRAMCycle() % PENDING_PER_BANK]);
        }
      }
      else
      {
        temp = currentPendingRequest;
      }

      assert(temp != NULL && "Error: temp is NULL after Schedule Force, Row Hit, and Priority");

      // We know there is *a* pending request in this buffer.  Let's cycle through them in round-robin
      // priority order.

      //Use this to signal that we have completed an access.  In particular, keep the assertion after this loop from firing in the case
      //that temp and currentPendingRequest both get NULLed out by the scheduler due to completion.
      bool completedAccess = false;
      for (int slot_count = 0; slot_count < PENDING_PER_BANK; slot_count++, temp = &(requestBuffer[temp->Rank][temp->Bank][(temp->Slot + 1) % PENDING_PER_BANK]))
      {
        // Skip empty entries
        if (!temp->Valid) {
          if (ForceSchedule && (temp == forcedRequest)) {
            DEBUG_HEADER();
            fprintf(stderr, "Error: Forced schedule on invalid slot\n");
            temp->dump();
            assert(0);
          }
          continue;
        }

        if(RowHit && !(helper_IsRowHit(temp))) {
          continue;
        }
        
        if(temp->AllLinesPending())
        {
          if(ForceSchedule && (temp == forcedRequest))
            completedAccess = true;
          continue;
        }
        else {
          //If we're doing batching and this request is not from the correct batch,
          //we can't use it.
          if(DRAM_BATCHING_POLICY != batch_none &&
            temp->BatchNumber != servicingBatchCounter)
          {
            if(ForceSchedule)
              assert(0 && "ForceSchedule is too strict, request from new batch was forced");
            continue;
          }
          temp->FirstAttemptCycle = rigel::CURR_CYCLE;
          temp->InProgress = true;

          //If myRequestHandler() returns true, we were able to make progress on the request.
          //Once we make progress on a request, we should stick with it until it is completed,
          //so as to avoid thrashing/livelock.
          //You want to avoid the case where you've found a row hit, don't happen to make progress on it,
          //and switch to something else (non row-hit).  I'm having trouble coming up with a scenario
          //where you can't make progress on a row hit but can make progress on a non row-hit to the same
          //bank, but I observed this behavior in DRAMSim @ NVR and fixing it improved performance
          //significantly.
          if ((*this.*myRequestHandler)(temp))
          {
            if(temp == NULL) //request finished
              completedAccess = true;
            currentPendingRequest = temp;
            if(foundHardForcedRequest)
              return;
            break;
          }
          else
          {
            if(foundForcedRequest)
            {
              currentPendingRequest = temp;
              if(foundHardForcedRequest)
                return;
              else
                break;
            }
            else
              continue;
          }
        }
      }
      //Something went wrong here
      if(DRAM_BATCHING_POLICY == batch_none && ForceSchedule && currentPendingRequest == NULL && !completedAccess)
      {
        assert(0 && "Scheduler was forced, should have found a pending access!");
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTION: helper_ScheduleControllerRowHit()
//////////////////////////////////////////////////////////////////////////////////
// Note that pendingRequest must be NULL on exit.
//////////////////////////////////////////////////////////////////////////////////
bool DRAMController::helper_FindRowHit(unsigned int rank, unsigned int bank, PendingRequestEntry* &rowHit)
{
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;

  // Nothing to schedule for this bank.
  if (requestBufferOccupancy[rank][bank] == 0) return false;

  unsigned int ActiveRowIndex = timingInfo.ActiveRow[rank][bank];
  // If we find a request that is a row hit, choose it first
  for (int slot = 0; slot < PENDING_PER_BANK; slot++) {
    if (!requestBuffer[rank][bank][slot].Valid)
      continue;
    else if (requestBuffer[rank][bank][slot].AllLinesPending())
      continue; //This request is already taken care of, doesn't count as a new row hit
    else
    {
      uint32_t addr = *(requestBuffer[rank][bank][slot].addrs.begin());
      unsigned int row = AddressMapping::GetRow(addr);
      bool hit = (rigel::DRAM::OPEN_ROWS == 1) ? ((unsigned int)row == ActiveRowIndex)
                                                 : (timingInfo.findInCache(rank, bank, row) != -1);
      if (hit)
      {
        // Found a row hit in a bank
        //If we're doing batching and it's not from the correct batch, we can't use it.
        if(DRAM_BATCHING_POLICY != batch_none &&
           requestBuffer[rank][bank][slot].BatchNumber != servicingBatchCounter)
        {
          continue;
        }
        rowHit = &(requestBuffer[rank][bank][slot]);
        return true;
      }
    }
  }

  // No row hits found
  rowHit = NULL;
  return false;
}


//////////////////////////////////////////////////////////////////////////////////
// helper_FindTimeout()
//////////////////////////////////////////////////////////////////////////////////
bool DRAMController::helper_FindTimeout(unsigned int rank, unsigned int bank, PendingRequestEntry* &oldRequest)
{
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;

  bool force_sched = false;
  uint64_t force_max_wait = 0;

  oldRequest = NULL;

  if(requestBufferOccupancy[rank][bank] == 0)
    return false;
  for (int slot = 0; slot < PENDING_PER_BANK; slot++) {
    if (!requestBuffer[rank][bank][slot].Valid)
      continue;
    //If we reach here, the request is valid.
    uint64_t wait_time = MC_PENDING_WDT - requestBuffer[rank][bank][slot].WDTCycles;
    if (wait_time >= MC_FORCE_SCHEDULE_SOFT) {
      // Hit a point where a scheduling action is forced
      profiler::stats[STATNAME_MC_FORCE_SCHEDULE].inc();
      if (wait_time >= MC_FORCE_SCHEDULE_WARN) {
        // Throttle prints
        if (wait_time % 2500 == 0) {
          DEBUG_HEADER();
          fprintf(stderr, "[[MEM WDT]] Forced memory schedule (waiting %"PRIu64" cycles)\n", wait_time);
          requestBuffer[rank][bank][slot].dump();
        }
      }

      // Force scheduling of the oldest request first
      if (wait_time > force_max_wait) {
        //If we're doing batching and this request is not from the correct batch,
        //we can't use it.  We still want to display the warnings above though.
        if(DRAM_BATCHING_POLICY != batch_none &&
          requestBuffer[rank][bank][slot].BatchNumber != servicingBatchCounter)
          continue;
        //Same deal if this request is already taken care of and just waiting for data.
        if(requestBuffer[rank][bank][slot].AllLinesPending())
          continue;
        force_sched = true;
        oldRequest = &(requestBuffer[rank][bank][slot]);
        force_max_wait = wait_time;
      }
    }
  }

  // If we are forcing a scheduling action, return true.
  return force_sched;
}

//////////////////////////////////////////////////////////////////////////////////
// DRAMController::helper_HandlePendingRequest()
//////////////////////////////////////////////////////////////////////////////////
// There is a request that is valid in the PendingRequest buffer for the
// controller.  The controller will issue the proper command to make forward
// progress on this command.
////////////////////////////////////////////////////////////////////////////////////
bool
DRAMController::helper_HandleRequestRowCache(PendingRequestEntry* &request)
{
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;

  DRAM_CONTROLLER_DEBUG(printf("----------\n"));
  // Schedule the memory request to be delivered to the cache controller
  uint32_t addr;
  bool foundUnpendedLine = false;
  for(std::set<uint32_t>::const_iterator it = request->addrs.begin(), end = request->addrs.end();
      it != end; ++it)
  {
    if(!(request->IsPending(*it)))
    {
      foundUnpendedLine = true;
      addr = *it;
      break;
    }
  }
  assert(foundUnpendedLine && "Error: request should not make it into helper_HandleRequestRowCache() if all lines are pending");

  int row = AddressMapping::GetRow(addr);
  int bank = AddressMapping::GetBank(addr);
  int rank = AddressMapping::GetRank(addr);
  uint64_t currCycle = DRAMChannelModel::GetDRAMCycle();
  DRAM_CONTROLLER_DEBUG(printf("%"PRIu64": Ctrl %d Rank %d Bank %d %s from 0x%"PRIx32"\n",
                                DRAMChannelModel::GetDRAMCycle(), AddressMapping::GetController(addr), rank, bank,
                                (request->ReadNOTWrite ? "Read" : "Write"),
                                addr));
  if(request->Rank != rank || request->Bank != bank)
  {
    fprintf(stderr, "Error: request is in buffer for rank/bank %d/%d, address maps to rank/bank %d/%d\n",
            request->Rank, request->Bank, rank, bank);
    exit(1);
  }

  int cacheEntry = timingInfo.findInCache(rank, bank, row);
  if(cacheEntry == -1) //Row cache miss
  {
    DRAM_CONTROLLER_DEBUG(printf("Row not in cache\n"));

    if(request->ReadNOTWrite == true) //read
    {
      int victim = timingInfo.victim[rank][bank];
      if(victim == -1) //No victim selected yet.
      {
        //We will use the replacement policy every cycle to choose a new victim until
        //we can make forward progress (issue a command) with one.  Then we lock it in
        //so we don't livelock.
        victim = timingInfo.findVictim(rank, bank);
        DRAM_CONTROLLER_DEBUG(printf("Cache entry %d (row %d) selected as victim\n", victim, timingInfo.CacheRows[rank][bank][victim].row));
      }
      //Now we have a victim.  If it is invalid, we can try to fill now.
      //If it is valid and clean, we should invalidate it.
      //If it is valid and dirty, we need to write it back.
      RowCacheEntry & vic = timingInfo.CacheRows[rank][bank][victim];
      if(vic.row != -1) //Valid
      {
        if(!vic.dirty) //Clean
        {
          DRAM_CONTROLLER_DEBUG(printf("Have clean victim\n"));
          //vic.row = -1;
        }
        else //Dirty
        {
          DRAM_CONTROLLER_DEBUG(printf("Victim is dirty\n"));
          if(timingInfo.ActiveRow[rank][bank] != vic.row) //Need to switch rows to do the writeback
          {
            DRAM_CONTROLLER_DEBUG(printf("Need to switch rows to writeback dirty victim\n"));
            if(timingInfo.ActiveRow[rank][bank] == -1) //IDLE (need to ACT)
            {
              if(timingInfo.canACT(rank, bank, true)) //See if we can do a backdoor ACT
              {
                DRAM_CONTROLLER_DEBUG(printf("Activating\n"));
                profiler::stats[STATNAME_DRAM_CMD_ACTIVATE].inc();
                timingInfo.setACT(rank, bank, vic.row, true); //Backdoor ACT
                //Can't do this because the channel model doesn't know about row caches.
                //model->ScheduleCommand(addr, DRAM_CMD_ACTIVATE, 0);
                timingInfo.victim[rank][bank] = victim; //We made forward progress, lock it in.
                return true;
              }
              DRAM_CONTROLLER_DEBUG(printf("IDLE, but can't activate\n"));
              return false;
            }
            else //Another row is active, we need to precharge before we can activate the victim's row
            {
              if(timingInfo.canPRE(rank, bank, true))
              {
                DRAM_CONTROLLER_DEBUG(printf("Precharging\n"));
                profiler::stats[STATNAME_DRAM_CMD_PRECHARGE].inc();
                timingInfo.setPRE(rank, bank, true);
                //Can't do this because the channel model doesn't know about row caches.
                //model->ScheduleCommand(addr, DRAM_CMD_PRECHARGE, 0);
                timingInfo.victim[rank][bank] = victim;
                return true;
              }
              DRAM_CONTROLLER_DEBUG(printf("Need to precharge but can't\n"));
              return false;
            }
            assert(0);
            return false; //Should never reach
          }
          //The correct row is active, now we just need to do a writeback
          DRAM_CONTROLLER_DEBUG(printf("Victim writeback row is active, need to do the WRITE\n"));
          if(timingInfo.canWRITE(rank, bank, vic.row, true)) //We do the write immediately.
          {
            timingInfo.setWRITE(rank, bank, vic.row, true);
            //We are done with the writeback.  We can clean the victim entry now, and it will be invalidated next cycle.
            vic.dirty = false;
            timingInfo.victim[rank][bank] = victim;
            return true;
          }
        }
      }
      //The victim is all taken care of now, and we can safely move on to reading in the row of interest.
      DRAM_CONTROLLER_DEBUG(printf("Have a row cache slot, need to read in new row\n"));
      if(timingInfo.ActiveRow[rank][bank] != row) //Row is not already active (bank could be idle or active on a different row)
      {
        DRAM_CONTROLLER_DEBUG(printf("Need to switch rows to read in new row (currently %d is active)\n", timingInfo.ActiveRow[rank][bank]));
        if(timingInfo.ActiveRow[rank][bank] == -1) //IDLE (need to ACT)
        {
          if(timingInfo.canACT(rank, bank, true)) //See if we can do a backdoor ACT
          {
            DRAM_CONTROLLER_DEBUG(printf("Activating\n"));
            profiler::stats[STATNAME_DRAM_CMD_ACTIVATE].inc();
            timingInfo.setACT(rank, bank, row, true); //Backdoor ACT
            //timingInfo.CacheState[rank][bank] = ROWCACHESTATE_EVICT_ACTIVATING;
            //model->ScheduleCommand(addr, DRAM_CMD_ACTIVATE, 0);
            timingInfo.victim[rank][bank] = victim;
            return true;
          }
          DRAM_CONTROLLER_DEBUG(printf("IDLE, but can't activate\n"));
          return false;
        }
        else //Another row is active, we need to precharge before we can activate the new row
        {
          if(timingInfo.canPRE(rank, bank, true))
          {
            DRAM_CONTROLLER_DEBUG(printf("Precharging\n"));
            profiler::stats[STATNAME_DRAM_CMD_PRECHARGE].inc();
            timingInfo.setPRE(rank, bank, true);
            //model->ScheduleCommand(addr, DRAM_CMD_PRECHARGE, 0);
            timingInfo.victim[rank][bank] = victim;
            return true;
          }
          DRAM_CONTROLLER_DEBUG(printf("Need to precharge but can't\n"));
          return false;
        }
        assert(0);
        return false; //Should never reach
      }
      //The correct row is active, just need to do the READ now.
      if(timingInfo.canREAD(rank, bank, row, true)) //We do the read immediately.
      {
        timingInfo.setREAD(rank, bank, row, true);
        //We are done with the read.  We can fill in the RowCacheEntry now.
        DRAM_CONTROLLER_DEBUG(printf("Done filling row cache\n"));
        vic.row = row;
        vic.dirty = 0;
        vic.cycleAllocated = currCycle;
        vic.lastRead = 0;
        vic.lastWritten = 0;
        timingInfo.victim[rank][bank] = -1;
        return true;
      }
      else
      {
        DRAM_CONTROLLER_DEBUG(printf("Need to backdoor READ into row cache but can't\n"));
        return false;
      }
    }
    else //write
    {
      DRAM_CONTROLLER_DEBUG(printf("Doing a write to a line not in the row cache, do it in the row buffer\n"));
      if(timingInfo.ActiveRow[rank][bank] != row) //Need to switch rows to do the writeback
      {
        DRAM_CONTROLLER_DEBUG(printf("Need to switch rows to do row buffer write\n"));
        if(timingInfo.ActiveRow[rank][bank] == -1) //IDLE (need to ACT)
        {
          if(timingInfo.canACT(rank, bank, true)) //See if we can do a backdoor ACT
          {
            DRAM_CONTROLLER_DEBUG(printf("Activating\n"));
            profiler::stats[STATNAME_DRAM_CMD_ACTIVATE].inc();
            timingInfo.setACT(rank, bank, row, true); //Backdoor ACT
            //Can't do this because the channel model doesn't know about row caches.
            //model->ScheduleCommand(addr, DRAM_CMD_ACTIVATE, 0);
            return true;
          }
          DRAM_CONTROLLER_DEBUG(printf("IDLE, but can't activate\n"));
          return false;
        }
        else //Another row is active, we need to precharge before we can activate the victim's row
        {
          if(timingInfo.canPRE(rank, bank, true))
          {
            DRAM_CONTROLLER_DEBUG(printf("Precharging\n"));
            profiler::stats[STATNAME_DRAM_CMD_PRECHARGE].inc();
            timingInfo.setPRE(rank, bank, true);
            //Can't do this because the channel model doesn't know about row caches.
            //model->ScheduleCommand(addr, DRAM_CMD_PRECHARGE, 0);
            return true;
          }
          DRAM_CONTROLLER_DEBUG(printf("Need to precharge but can't\n"));
          return false;
        }
        assert(0);
        return false; //Should never reach
      }
    }
  }
  assert((cacheEntry >= 0 || (!request->ReadNOTWrite)) && "Shouldn't get here if the row was not in the cache!\n");

  //Row is in the cache, now we can do the normal (non-backdoor) READ/WRITE.
  if(request->ReadNOTWrite) //READ
  {
    if(timingInfo.canREAD(rank, bank, row, false))
    {
      profiler::stats[STATNAME_DRAM_CMD_READ].inc();
      timingInfo.setREAD(rank, bank, row, false);
      //FIXME We really shouldn't have to send a command every cycle...
      //FIXME BURST_SIZE/2 is no good (see other FIXMEs)
      //model->ScheduleCommand(addr, DRAM_CMD_READ, 0);
      //for(int i = 0; i < (request->BurstCount/2); i++)
      //{
      //  model->ScheduleData(addr, true, rigel::DRAM::LATENCY::CL + i);
      //}
      uint64_t cyclesFromNow = rigel::DRAM::LATENCY::CL + (request->BurstCount/2) - 1;
      uint64_t doneCycle = currCycle + cyclesFromNow;
      timingInfo.CacheRows[rank][bank][cacheEntry].lastRead = doneCycle;
      timingInfo.CacheRows[rank][bank][cacheEntry].numReads++;
      DRAM_CONTROLLER_DEBUG(printf("Issuing READ, should be done @ cycle %"PRIu64"\n", doneCycle));
      request->SetLineReady(addr, cyclesFromNow);
      //If this is the last line we need to pend, NULL out the pointer to tell the caller that this
      //request should not be passed to the HandleRequest() function anymore.
      if(request->AllLinesPending())
        request = NULL;
      return true;
    }
    DRAM_CONTROLLER_DEBUG(printf("Can't issue READ\n"));
    return false;
  }
  else //WRITE
  {
    if(cacheEntry == -1) //Not in row cache
    {
      if(timingInfo.canWRITE(rank, bank, row, false))
      {
        profiler::stats[STATNAME_DRAM_CMD_WRITE].inc();
        timingInfo.setWRITE(rank, bank, row, true); //backdoor
        uint64_t cyclesFromNow = rigel::DRAM::LATENCY::WL + (request->BurstCount/2) - 1;
        DRAM_CONTROLLER_DEBUG(printf("Issuing WRITE, should be done @ cycle %"PRIu64"\n", (currCycle + cyclesFromNow)));
        request->SetLineReady(addr, cyclesFromNow);
        //If this is the last line we need to pend, NULL out the pointer to tell the caller that this
        //request should not be passed to the HandleRequest() function anymore.
        if(request->AllLinesPending())
          request = NULL;
        return true;
      }
      DRAM_CONTROLLER_DEBUG(printf("Can't issue WRITE\n"));
      return false;
    }
    else
    {
      if(timingInfo.canWRITE(rank, bank, row, false))
      {
        profiler::stats[STATNAME_DRAM_CMD_WRITE].inc();
        timingInfo.setWRITE(rank, bank, row, false);
        //FIXME We really shouldn't have to send a command every cycle...
        //FIXME BURST_SIZE/2 is no good (see other FIXMEs)
        //model->ScheduleCommand(addr, DRAM_CMD_WRITE, 0);
        //for(int i = 0; i < (request->BurstCount/2); i++)
        //{
        //  model->ScheduleData(addr, false, rigel::DRAM::LATENCY::WL + i);
        //}
        //FIXME I think we can actually just call PM_set_sched now and get rid of it.
        //In hardware we could probably have a cache-line buffer for the read/write data for the current request
        //pretty easily.  We know that the request will complete at this point (before any other accesses).
        uint64_t cyclesFromNow = rigel::DRAM::LATENCY::WL + (request->BurstCount/2) - 1;
        uint64_t doneCycle = currCycle + cyclesFromNow;
        timingInfo.CacheRows[rank][bank][cacheEntry].lastWritten = doneCycle;
        timingInfo.CacheRows[rank][bank][cacheEntry].numWrites++;
        timingInfo.CacheRows[rank][bank][cacheEntry].dirty = true;
        DRAM_CONTROLLER_DEBUG(printf("Issuing WRITE, should be done @ cycle %"PRIu64"\n", doneCycle));
        request->SetLineReady(addr, cyclesFromNow);
        //If this is the last line we need to pend, NULL out the pointer to tell the caller that this
        //request should not be passed to the HandleRequest() function anymore.
        if(request->AllLinesPending())
          request = NULL;
        return true;
      }
      DRAM_CONTROLLER_DEBUG(printf("Can't issue WRITE\n"));
      return false;
    }
  }
  assert(0 && "Should never reach!");
  return false; //Should never reach
}

//////////////////////////////////////////////////////////////////////////////////
// DRAMController::helper_HandlePendingRequest()
//////////////////////////////////////////////////////////////////////////////////
// There is a request that is valid in the PendingRequest buffer for the
// controller.  The controller will issue the proper command to make forward
// progress on this command.
//////////////////////////////////////////////////////////////////////////////////
bool
DRAMController::helper_HandleRequest(PendingRequestEntry* &request)
{
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;

  DRAM_CONTROLLER_DEBUG(printf("----------\n"));
  // Schedule the memory request to be delivered to the cache controller
  uint32_t addr;
  bool foundUnpendedLine = false;
  for(std::set<uint32_t>::const_iterator it = request->addrs.begin(), end = request->addrs.end();
  it != end; ++it)
  {
    if(!(request->IsPending(*it)))
    {
      foundUnpendedLine = true;
      addr = *it;
      break;
    }
  }
  assert(foundUnpendedLine && "Error: request should not make it into helper_HandleRequest() if all lines are pending");
  
  int row = AddressMapping::GetRow(addr);
  int bank = AddressMapping::GetBank(addr);
  int rank = AddressMapping::GetRank(addr);
  //uint64_t currCycle = DRAMChannelModel::GetDRAMCycle();

  DRAM_CONTROLLER_DEBUG(printf("%"PRIu64": Ctrl %d Rank %d Bank %d Row %d %s from 0x%08"PRIx32"\n",
                               DRAMChannelModel::GetDRAMCycle(), AddressMapping::GetController(addr), rank, bank,
                               row, (request->ReadNOTWrite ? "Read" : "Write"), addr));

  if(request->Rank != rank || request->Bank != bank)
  {
    fprintf(stderr, "Error: request is in buffer for rank/bank %d/%d, address maps to rank/bank %d/%d\n",
            request->Rank, request->Bank, rank, bank);
    exit(1);
  }

  if(timingInfo.ActiveRow[rank][bank] != row)
  {
    DRAM_CONTROLLER_DEBUG(printf("Row not active\n"));
    if(timingInfo.ActiveRow[rank][bank] == -1) //IDLE (need to ACT)
    {
      if(timingInfo.canACT(rank, bank, false))
      {
        DRAM_CONTROLLER_DEBUG(printf("Activating\n"));
        timingInfo.setACT(rank, bank, row, false);
        model->ScheduleCommand(addr, DRAM_CMD_ACTIVATE, 0);
        return true;
      }
      DRAM_CONTROLLER_DEBUG(printf("IDLE, but can't activate\n"));
      return false;
    }
    else //Another row is active -> we need to PRE
    {
      if(timingInfo.canPRE(rank, bank, false))
      {
        DRAM_CONTROLLER_DEBUG(printf("Precharging\n"));
        timingInfo.setPRE(rank, bank, false);
        model->ScheduleCommand(addr, DRAM_CMD_PRECHARGE, 0);
        return true;
      }
      DRAM_CONTROLLER_DEBUG(printf("Need to precharge but can't\n"));
      return false;
    }
    return false; //Should never reach
  }

  if(request->ReadNOTWrite) //READ
  {
    if(timingInfo.canREAD(rank, bank, row, false))
    {
      timingInfo.setREAD(rank, bank, row, false);
      //FIXME We really shouldn't have to send a command every cycle...
      //FIXME BURST_SIZE/2 is no good (see other FIXMEs)
      model->ScheduleCommand(addr, DRAM_CMD_READ, 0);
      for(int i = 0; i < (request->BurstCount/2); i++)
      {
        model->ScheduleData(addr, true, rigel::DRAM::LATENCY::CL + i);
      }
      uint64_t cyclesFromNow = rigel::DRAM::LATENCY::CL + (request->BurstCount/2) - 1;
      //uint64_t doneCycle = currCycle + cyclesFromNow;
      DRAM_CONTROLLER_DEBUG(printf("Issuing READ, should be done @ cycle %"PRIu64"\n", doneCycle));
      request->SetLineReady(addr, cyclesFromNow);
      //If this is the last line we need to pend, NULL out the pointer to tell the caller that this
      //request should not be passed to the HandleRequest() function anymore.
      if(request->AllLinesPending())
        request = NULL;
      return true;
    }
    DRAM_CONTROLLER_DEBUG(printf("Can't issue READ\n"));
    return false;
  }
  else //WRITE
  {
    if(timingInfo.canWRITE(rank, bank, row, false))
    {
      timingInfo.setWRITE(rank, bank, row, false);
      //FIXME We really shouldn't have to send a command every cycle...
      //FIXME BURST_SIZE/2 is no good (see other FIXMEs)
      model->ScheduleCommand(addr, DRAM_CMD_WRITE, 0);
      for(int i = 0; i < (request->BurstCount/2); i++)
      {
        model->ScheduleData(addr, false, rigel::DRAM::LATENCY::WL + i);
      }
      //FIXME I think we can actually just call PM_set_sched now and get rid of it.
      //In hardware we could probably have a cache-line buffer for the read/write data for the current request
      //pretty easily.  We know that the request will complete at this point (before any other accesses).
      uint64_t cyclesFromNow = rigel::DRAM::LATENCY::WL + (request->BurstCount/2) - 1;
      //uint64_t doneCycle = currCycle + cyclesFromNow;
      DRAM_CONTROLLER_DEBUG(printf("Issuing WRITE, should be done @ cycle %"PRIu64"\n", doneCycle));
      request->SetLineReady(addr, cyclesFromNow);
      //If this is the last line we need to pend, NULL out the pointer to tell the caller that this
      //request should not be passed to the HandleRequest() function anymore.
      if(request->AllLinesPending())
        request = NULL;
      return true;
    }
    DRAM_CONTROLLER_DEBUG(printf("Can't issue WRITE\n"));
    return false;
  }
  assert(0 && "Should never reach!");
  return false; //Should never reach
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void DRAMController::helper_HandleReordering(uint64_t RequestNumber)
{
  //printf("Handling request %"PRIu64", old counter is %"PRIu64", ", RequestNumber, outputRequestCounter);
  if(RequestNumber == outputRequestCounter)
  {
    //One request was reordered by 0 (no reordering)
    rigel::profiler::stats[STATNAME_MC_REORDERING].inc_mem_histogram(0, 1);
    //Other requests may have been serviced out of order before this one.
    //Search outOfOrderRequests for RequestNumber+1, RequestNumber+2, ...
    //Until one of them is not found in the set.  The first one that is not
    //found is the new outputRequestCounter.
    outputRequestCounter++;
    while(outOfOrderRequests.erase(outputRequestCounter) != 0)
      outputRequestCounter++;
  }
  else
  {
    //Find delta between expected and actual
    uint64_t delta = RequestNumber - outputRequestCounter;
    rigel::profiler::stats[STATNAME_MC_REORDERING].inc_mem_histogram(delta, 1);
    outOfOrderRequests.insert(RequestNumber);
  }
  //printf("new counter is %"PRIu64"\n", outputRequestCounter);
}

////////////////////////////////////////////////////////////////////////////////
// Schedule(ARGS...)
// 
// The GCache calls this method to insert a new request for this address into
// the scheduling window for the memory controller.  The GCache will keep
// retrying a request until GCRequestAcked is set by Schedule().
// RETURNS: TODO: We need to fix this so that the return value makes sense...
//////////////////////////////////////////////////////////////////////////////////
bool DRAMController::Schedule(const std::set<uint32_t> addrs, int size,
    const MissHandlingEntry<rigel::cache::LINESIZE> & MSHR,
    class CallbackInterface *requestingEntity, int requestIdentifier)
{
  using namespace rigel::mem_sched;

  //We emulate a monolithic scheduler (as opposed to per-bank request
  //buffers) very simply.  We just size each per-bank buffer as large
  //as the virtual monolithic queue (so no bank buffer overflows) and
  //we NAK requests that would push total occupancy above the given size
  if(MONOLITHIC_SCHEDULER && totalRequestBufferOccupancy >= PENDING_PER_BANK)
    return false;
  //We could also return false in the baseline case if the whole memory controller
  //is full, but that seems unlikely.

  unsigned int ctrl = AddressMapping::GetController(*(addrs.begin()));
  unsigned int rank = AddressMapping::GetRank(*(addrs.begin()));
  unsigned int bank = AddressMapping::GetBank(*(addrs.begin()));
  unsigned int row = AddressMapping::GetRow(*(addrs.begin()));
  for(std::set<uint32_t>::iterator it = addrs.begin(); it != addrs.end(); ++it)
  {
    if(AddressMapping::GetController(*it) != ctrl ||
       AddressMapping::GetRank(*it) != rank ||
       AddressMapping::GetBank(*it) != bank ||
       AddressMapping::GetRow(*it) != row)
    {
      fprintf(stderr,"Error: Address 0x%08x maps to CTRL/RANK/BANK/ROW (%u,%u,%u,%u)\n",
              *(addrs.begin()), AddressMapping::GetController(*it),AddressMapping::GetRank(*it),
              AddressMapping::GetBank(*it),AddressMapping::GetRow(*it));
      fprintf(stderr,"First request in batch maps to (%u,%u,%u,%u)\n",
              ctrl, rank, bank, row);
      assert(0 && "Requests in batch stride across DRAM rows!");
    }
  }

  // Update notes for when we get rid of MSHR reference in the
  // GDDR:
  //
  // We still need the MSHR values since we use the following fields inside
  // of the memory controller now:
  //  1. watchdog_last_set_time
  //  2. dump()
  //  3. addr()

  // Find an empty entry and insert the new request from the GCache.
  for (int slot = 0; slot < rigel::mem_sched::PENDING_PER_BANK; slot++) {
    PendingRequestEntry * preq = &requestBuffer[rank][bank][slot];
    if(preq->Valid)
    {
      if(collisionChecking)
      {
        for(std::set<uint32_t>::iterator it = addrs.begin(); it != addrs.end(); ++it) {
          for(std::set<uint32_t>::iterator it2 = preq->addrs.begin(); it2 != preq->addrs.end(); ++it2) {
            if(*it == *it2)
            {
              // Found a match!
              DEBUG_HEADER();
              fprintf(stderr, "[[MEM CTRL]] COLLISION FOUND!\n");
              fprintf(stderr, "Old addrs: ");
              for(std::set<uint32_t>::iterator it3 = preq->addrs.begin(); it3 != preq->addrs.end(); ++it3)
                fprintf(stderr, "0x%x ", *it3);
              fprintf(stderr, "\n");
              fprintf(stderr, "New addrs: ");
              for(std::set<uint32_t>::iterator it3 = addrs.begin(); it3 != addrs.end(); ++it3)
                fprintf(stderr, "0x%x ", *it3);
              fprintf(stderr, "\n");
              fprintf(stderr, "REQ MSHR: \n");
              MSHR.dump();
              fprintf(stderr, "PEND MSHR: \n");
              preq->MSHR.dump();
              assert(0 && "COLLISION!");
            }
          }
        }
      }
    }
    else //Found a free entry
    {
      // The entry is now "busy"
      preq->Valid = true;
      // TODO: Add coalescing/ordering?
      preq->MSHR = MSHR;
      preq->addrs = addrs;
      //Keep a callback to the caller so we can let them know when the request is done.
      preq->Requester = requestingEntity;
      preq->RequestIdentifier = requestIdentifier;
      preq->ReadNOTWrite = MSHR.IsRead();
      preq->InsertionCycle = DRAMChannelModel::GetDRAMCycle();
      preq->RequestNumber = inputRequestCounter;
      inputRequestCounter++;

      //Assign batch number (only if we are using the batch_perchannel
      //or batch_perbank policies)
      if(DRAM_BATCHING_POLICY == batch_perchannel)
      {
        if(requestsInCurrentBatch == DRAM_BATCHING_CAP)
        {
          //printf("%u Requests in Batch\n", requestsInCurrentBatch);
          //Need to deal with a weird race condition.
          //We need to notify the servicer if it is idle when we start a new batch.
          if(outstandingBatchRequests[issuingBatchCounter] == 0)
          {
            outstandingBatchRequests.erase(issuingBatchCounter);
            servicingBatchCounter++;
          }
          requestsInCurrentBatch = 0;
          issuingBatchCounter++;
          outstandingBatchRequests[issuingBatchCounter] = 0;
        }
        outstandingBatchRequests[issuingBatchCounter]++;
        requestsInCurrentBatch++;
        preq->BatchNumber = issuingBatchCounter;
      }
      else if(DRAM_BATCHING_POLICY == batch_perbank)
      {
        unsigned int requestsInBank = 0;
        bool endBatch = false;
        for (int slot = 0; slot < PENDING_PER_BANK; slot++) {
          PendingRequestEntry * preq2 = &requestBuffer[rank][bank][slot];
          if(preq2->BatchNumber == issuingBatchCounter)
          {
            requestsInBank++;
            if(requestsInBank == DRAM_BATCHING_CAP)
            {
              endBatch = true;
              break;
            }
          }
        }
        if(endBatch)
        {
          //printf("%u Requests in Batch\n", requestsInCurrentBatch);
          //Need to deal with a weird race condition.
          //We need to notify the servicer if it is idle when we start a new batch.
          if(outstandingBatchRequests[issuingBatchCounter] == 0)
          {
            outstandingBatchRequests.erase(issuingBatchCounter);
            servicingBatchCounter++;
          }
          requestsInCurrentBatch = 0;
          issuingBatchCounter++;
          outstandingBatchRequests[issuingBatchCounter] = 0;
        }
        requestsInCurrentBatch++;
        outstandingBatchRequests[issuingBatchCounter]++;
        preq->BatchNumber = issuingBatchCounter;
      }

     // printf("I in %"PRIu64" out %"PRIu64" numi %u numo %u\n",
     //   issuingBatchCounter, servicingBatchCounter,
     //   outstandingBatchRequests[issuingBatchCounter],
     //        outstandingBatchRequests[servicingBatchCounter]);

      // DONE: Request inserted.
      requestBufferOccupancy[rank][bank]++;
      totalRequestBufferOccupancy++;
      // Schedule() succeeded.  Requester is now waiting for callback.

      if (rigel::GENERATE_DRAMTRACE) {
        for(std::set<uint32_t>::iterator it = addrs.begin(); it != addrs.end(); ++it)
        {
          uint32_t addr = *it;
        fprintf(rigel::memory::dramtrace_out, "%12llu, %8x, %d, %d, %d, %d, %d, %d, %d\n",
          (unsigned long long)DRAMChannelModel::GetDRAMCycle(), addr, MSHR.IsRead(),
              MSHR.GetCoreID(), AddressMapping::GetController(addr),
              AddressMapping::GetRank(addr),
              AddressMapping::GetBank(addr),
              AddressMapping::GetRow(addr),
              AddressMapping::GetCol(addr));
        }
      }
#ifdef DEBUG_PRINT_DRAM_TRACE
  DEBUG_HEADER();
  fprintf(stderr, "[[MEM CTRL]] GCache Schedule() callback.");
  fprintf(stderr, "ctrl: %4d rank: %d bank: %4d addr: 0x%08x read: %d", id, rank, bank, preq->MSHR.get_addr(), preq->ReadNOTWrite);
  fprintf(stderr, "\n");
#endif
      return true;
    }
  }
  // No valid request buffers found.  GCache will have to call back later
  return false;
}

bool DRAMController::helper_IsRowHit(PendingRequestEntry* const &request) const {
  if((int)AddressMapping::GetRow(*(request->addrs.begin())) == timingInfo.ActiveRow[request->Rank][request->Bank]) {
    return true;
  }
  else {
    return false;
  } 
}

bool TimingInfo::canACT(int rank, int bank, bool backdoor)
{
  uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();

  if(!backdoor)
  {
    if(getAddressBusBusy(CurrCycle))
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't activate because address bus is busy\n"));
      return false;
    }
    if(getCommandBusBusy(CurrCycle))
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't activate because command bus is busy\n"));
      return false;
    }
  }

  //A setPRE() call puts the bank in PRECHARGING.
  //As a performance optimization we do not check every cycle and set it to IDLE after tRP.
  //Thus, when we need to know the state, we check how long ago the PRE occurred, and if it
  //was long enough ago we make it IDLE.
  if(State[rank][bank] != DRAMBANKSTATE_IDLE)
  {
    if(State[rank][bank] != DRAMBANKSTATE_PRECHARGING)
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't activate because bank is not in precharging state\n"));
      return false; //Bank is ACTIVATING or ACTIVE.
    }
    else
    {
      if(CurrCycle - PRE[rank][bank] >= rigel::DRAM::LATENCY::RP) //tRP is satisfied
        State[rank][bank] = DRAMBANKSTATE_IDLE;
      else //tRP not satisfied
      {
        DRAM_CONTROLLER_DEBUG(printf("Can't activate because tRP is not satisfied\n"));
        return false;
      }
    }
  }

  //At this point, either we return'ed or the bank is IDLE.
  //If this is the first ACT for this rank, it is always OK
  if(LastNActivates[rank].empty())
    return true;

  //Have we satisfied tRC?
  if(CurrCycle - ACT[rank][bank] < rigel::DRAM::LATENCY::RC) //tRC not satisfied
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't activate because tRC is not satisfied\n"));
    return false;
  }

  //Have we satisfied tRRD?
  if(CurrCycle - LastNActivates[rank].back().cycle < rigel::DRAM::LATENCY::RRD) //tRRD not satisfied
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't activate because tRRD is not satisfied\n"));
    return false;
  }

  //Have we satisfied tFAW?
  //Find fourth-most-recent activate cycle
  if(LastNActivates[rank].size() < 4)
    return true;
	std::deque<ActivateTracker>::reverse_iterator it(LastNActivates[rank].rbegin()); //most recent
  ++it; //second
  ++it; //third
  ++it; //fourth
  uint64_t fourthMostRecentACTCycle = it->cycle;
  if(CurrCycle - fourthMostRecentACTCycle < rigel::DRAM::LATENCY::FAW) //tFAW not satisfied
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't activate because tFAW is not satisfied\n"));
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool TimingInfo::canREAD(int rank, int bank, int row, bool backdoor)
{
  if(rigel::DRAM::OPEN_ROWS == 1 || backdoor)
  {
    if(ActiveRow[rank][bank] != row)
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't read because a different row is active\n"));
      return false;
    }
  }
  uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();
  if(!backdoor)
  {
    if(getAddressBusBusy(CurrCycle))
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't read because address bus is busy\n"));
      return false;
    }
    if(getCommandBusBusy(CurrCycle))
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't read because command bus is busy\n"));
      return false;
    }
  }
  if(State[rank][bank] != DRAMBANKSTATE_ACTIVE && State[rank][bank] != DRAMBANKSTATE_ACTIVATING)
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't activate because bank is not in ACTIVATING or ACTIVE state\n"));
    return false;
  }
  if(CurrCycle - ACT[rank][bank] < rigel::DRAM::LATENCY::RCDR) //tRCDR not fulfilled
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't read because tRCDR is not satisfied\n"));
    return false;
  }
  if(WRITEHasHappenedSinceLastACT(rank, bank) && (CurrCycle - WriteData[rank][bank] < rigel::DRAM::LATENCY::WTR)) //tWTR not fulfilled
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't read because tWTR is not satisfied\n"));
    return false;
  }
  if(CurrCycle - getLastCAS(rank) < rigel::DRAM::LATENCY::CCD) //tCCD not fulfilled
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't read because tCCD is not satisfied\n"));
    return false;
  }
  if(!backdoor)
  {
    //Will the data bus be available for the full burst length?  If not, don't schedule the READ.
    //TODO BURST_SIZE/2 is not general enough.
    for(int i = 0; i < (rigel::DRAM::BURST_SIZE/2); i++)
    {
      if(getDataBusBusy(CurrCycle + rigel::DRAM::LATENCY::CL + i))
      {
        DRAM_CONTROLLER_DEBUG(printf("Can't read because data bus is busy\n"));
        return false;
      }
    }
  }
  return true;
}

bool TimingInfo::canWRITE(int rank, int bank, int row, bool backdoor)
{
  if(rigel::DRAM::OPEN_ROWS == 1 || backdoor)
  {
    if(ActiveRow[rank][bank] != row)
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't write because different row is active\n"));
      return false;
    }
  }
  uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();
  if(!backdoor)
  {
    if(getAddressBusBusy(CurrCycle))
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't write because address bus is busy\n"));
      return false;
    }
    if(getCommandBusBusy(CurrCycle))
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't write because command bus is busy\n"));
      return false; 
    }
  }
  if(State[rank][bank] != DRAMBANKSTATE_ACTIVE && State[rank][bank] != DRAMBANKSTATE_ACTIVATING)
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't write because bank is not in ACTIVATING or ACTIVE state\n"));
    return false;
  }
  if(CurrCycle - ACT[rank][bank] < rigel::DRAM::LATENCY::RCDW) //tRCDW not fulfilled
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't write because tRCDW is not satisfied\n"));
    return false;
  }
  if(CurrCycle - getLastCAS(rank) < rigel::DRAM::LATENCY::CCD) //tCCD not fulfilled
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't write because tCCD is not satisfied\n"));
    return false;
  }
  if(!backdoor)
  {
    //Will the data bus be available for the full burst length?  If not, don't schedule the READ.
    //TODO BURST_SIZE/2 is not general enough.
    //TODO different r/w burst lengths?
    for(int i = 0; i < (rigel::DRAM::BURST_SIZE/2); i++)
      if(getDataBusBusy(CurrCycle + rigel::DRAM::LATENCY::WL + i))
      {
        DRAM_CONTROLLER_DEBUG(printf("Can't write because data bus is busy\n"));
        return false;
      }
  }
  return true;
}

bool TimingInfo::canPRE(int rank, int bank, bool backdoor)
{
  uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();

  if(!backdoor)
  {
    if(getAddressBusBusy(CurrCycle))
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't precharge because address bus is busy\n"));
      return false;
    }
    if(getCommandBusBusy(CurrCycle))
    {
      DRAM_CONTROLLER_DEBUG(printf("Can't precharge because command bus is busy\n"));
      return false;
    }
  }

  //I think it is possible to be in ACTIVATING if you dont do any intervening READ or WRITE
  //However, I think that would be a bug, since we should only open a row to do a READ or WRITE,
  //and we shouldn't give up on it until it's done.
  if(State[rank][bank] != DRAMBANKSTATE_ACTIVE)
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't precharge because bank is not ACTIVE (state = %d)\n", State[rank][bank]));
    return false;
  }

  if(CurrCycle - ACT[rank][bank] < rigel::DRAM::LATENCY::RAS) //tRAS not fulfilled
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't precharge because tRAS has not been satisfied\n"));
    return false;
  }

  if((READHasHappenedSinceLastACT(rank, bank) && READ[rank][bank] > CurrCycle) ||
     (WRITEHasHappenedSinceLastACT(rank, bank) && WRITE[rank][bank] > CurrCycle)) //There is still a READ or WRITE pending
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't precharge because there is still a read or write pending (read cycle %"PRIu64", write cycle %"PRIu64")\n", READ[rank][bank], WRITE[rank][bank]));
    return false;
  }

  //Since CurrCycle and WriteData are unsigned, this does not handle the case when the WRITE is still in progress while you're trying to do this.
  //Therefore, the code above this is necessary.
  if(WRITEHasHappenedSinceLastACT(rank, bank) && (CurrCycle - WriteData[rank][bank] < rigel::DRAM::LATENCY::WR)) //tWR not fulfilled
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't precharge because tWR has not been satisfied\n"));
    return false;
  }

  if(READHasHappenedSinceLastACT(rank, bank) && (CurrCycle - READ[rank][bank] < rigel::DRAM::LATENCY::RTP))
  {
    DRAM_CONTROLLER_DEBUG(printf("Can't precharge because tRTP has not been satisfied\n"));
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PendingRequestEntry::dump()
////////////////////////////////////////////////////////////////////////////////
// Dumps a single pending request in the DRAM controller (and its associated MSHR) to stderr
void
PendingRequestEntry::dump() const{
  fprintf(stderr, "DRAM Cycle %"PRIu64": PendingRequestEntry: ", DRAMChannelModel::GetDRAMCycle());
  fprintf(stderr, "Valid: %1d ", Valid);
  fprintf(stderr, "Rank: %2d ", Rank);
  fprintf(stderr, "Bank: %2d ", Bank);
  fprintf(stderr, "Slot: %2d ", Slot);
  fprintf(stderr, "InProgress: %01d ", InProgress);
  fprintf(stderr, "WDTCycles: %5d ", WDTCycles);
  fprintf(stderr, "FirstAttemptCycle: %9"PRIu64" ", FirstAttemptCycle);
  fprintf(stderr, "Addrs: (");
  for(std::set<uint32_t>::const_iterator it = addrs.begin(), end = addrs.end();
      it != end; ++it)
  {
    fprintf(stderr, "0x%08x, ", *it);
  }
  fprintf(stderr, ") ReadyLines: (");
  for(std::set<ReadyLinesEntry>::const_iterator it = ReadyLines.begin(), end = ReadyLines.end();
      it != end; ++it)
  {
    fprintf(stderr, "0x%08x @ %"PRIu64", ", it->getAddr(), it->getReadyCycle());
  }
  fprintf(stderr, ") MSHR: \n");
  MSHR.dump();
  fprintf(stderr, "\n");
}

//TODO: pendingOnly is currently ignored.
void
DRAMController::dump(bool pendingOnly) const
{
  using namespace rigel::DRAM;
  using namespace rigel::mem_sched;

  fprintf(stderr, "#### CONTROLLER %3d #####\n", id);
  for(unsigned int rank = 0; rank < RANKS; rank++)
  {
    for (unsigned int bank = 0; bank < BANKS; bank++)
    {
      for (int slot = 0; slot < PENDING_PER_BANK; slot++) {
        if (requestBuffer[rank][bank][slot].Valid) {
          fprintf(stderr, "REQ: (%2d,%2d,%2d)", rank, bank, slot);
          requestBuffer[rank][bank][slot].dump();
        }
      }
    }
  }
}
