////////////////////////////////////////////////////////////////////////////////
// event_track.cpp
////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////


#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for fprintf, sprintf, FILE, etc
#include <fstream>                      // for ifstream, ofstream, etc
#include <list>                         // for list, _List_iterator, etc
#include "core.h"           // for CoreInOrderLegacy
#include "define.h"         // for DEBUG_HEADER
#include "util/event_track.h"    // for EventTrack, etc
#include "core/regfile_legacy.h"        // for RegisterFile
#include "rigellib.h"       // for ::EVT_EVENT_COUNT, etc
#include "sim.h"            // for THREADS_TOTAL, etc
#include "memory/backing_store.h"

using namespace rigel::event;

EventTrack rigel::event::global_tracker;
int rigel::event::gTaskCount;
int* EventTrack::CurrentTask;

//Needs to be here because EventTrack is forward-declared.
void EventCounterWaitForAllThreads::end(int tid) {
  // Only count event when pending core ends (total length of
  // barrier).
  if (pending_thread == tid) {
    parent->RTM_BarrierCount++; //FIXME Should call incRTMBarCount()?
    EventCounter::end(0);
    pending_thread = -1;
    //DEBUG_HEADER();
    //fprintf(stderr, "BARRIER FOUND: %lld\n", parent->GetRTMBarrierCount());                
  }
}

////////////////////////////////////////////////////////////////////////////////
/// EventTrack
///  Constructor for the Event Tracking subsystem.
////////////////////////////////////////////////////////////////////////////////
EventTrack::EventTrack() :
  RTM_TaskLength("RTM_TASK_LENGTH"),
  RTM_Enqueue("RTM_ENQUEUE"),
  RTM_EnqueueGroup("RTM_ENQUEUE_GROUP"),
  RTM_Dequeue("RTM_DEQUEUE"),
  RTM_Barrier("RTM_BARRIER", this),
  RTM_Idle("RTM_IDLE"),
  RCUDA_KernelLength("RCUDA_KERNEL_LENGTH"),
  RCUDA_KernelLaunch("RCUDA_KERNEL_LAUNCH"),
  RCUDA_BlockFetch("RCUDA_BLOCK_FETCH"),
  RCUDA_Barrier("RCUDA_BARRIER"),
  RCUDA_ThreadId("RCUDA_THREAD_ID"),
  RCUDA_SyncThreads("RCUDA_SYNC_THREADS"),
  RCUDA_Init("RCUDA_INIT")
{
}

////////////////////////////////////////////////////////////////////////////////
/// HandleEvent
/// 
/// An event instruction is found in execute (it is non-speculative).  This
/// method is used to call the handler for the particular counter.
////////////////////////////////////////////////////////////////////////////////
void 
EventTrack::HandleEvent(event_track_t event, int tid, CoreInOrderLegacy* cptr)
{
  int core = tid / rigel::THREADS_PER_CORE;
  int thread = tid % rigel::THREADS_PER_CORE;

  if (event < 0 || event >= EVT_EVENT_COUNT) {
    goto error_case;
  }
  switch (event) 
  {

    //---------------------------------------------------//
    // dump the entire current memory image to a file
    case EVT_MEM_DUMP: {
      //backing_store->dump_to_file(rigel::MEM_DUMP_FILE);
      (rigel::GLOBAL_BACKING_STORE_PTR)->dump_to_file("dumpster.mem");
    }
    //---------------------------------------------------//
    // dump the current RF contents to a file
    // (hard-coded to rf.out)
    case EVT_RF_DUMP: {
          char out_file[200];
          sprintf(out_file,"rf.%d.%d.dump", core, thread);
					std::ofstream rfdump;
          rfdump.open(out_file);
          cptr->get_regfile(thread)->dump_reg_file(rfdump);
          rfdump.close();
          break;
    }
    // load the current RF contents from a file
    // (hard-coded to either rf.in or rf.core.thread.in)
    case EVT_RF_LOAD: {
      //TODO: Support multiple regfiles per core and cluster and tile and pr0n
          //assert(0 && "RF Load not implemented!\n");
          char in_file[200];
          uint32_t temp_val;
          //sprintf(in_file,"rf.%d.%d.in",core,thread);
          sprintf(in_file,"rf.in");
					std::ifstream rfload;
          rfload.open(in_file);
          for(int rr=0; rr<32; rr++) {  // FIXME: hardcoded to 32 regs!
            // FIXME: we need to change the write methods so we don't have to
            // access the data array directly, see bug #146
            rfload >> std::hex >> temp_val;
            cptr->get_regfile(thread)->set(temp_val,rr);
          }
          rfload.close();

          break;
    }
    //---------------------------------------------------//
    case EVT_RTM_TASK_START: { RTM_TaskLength.start(tid);
          gTaskCount++; // get next task count
          assert( NULL != CurrentTask && "CurrentTask[] uninitialized!");
          CurrentTask[tid] = gTaskCount;
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"TASK_START : core %d thread %d TASK#: %d\n",
              core,thread,CurrentTask[tid]);
          }
          #endif
          break; }
    case EVT_RTM_TASK_END: { RTM_TaskLength.end(tid); 
          // make sure we've started a task before we try to end one...
          if( gTaskCount >= 0 && CurrentTask[tid] >= 0 ) {
            #ifdef DEBUG_SHARING
            if(rigel::CMDLINE_DUMP_SHARING_STATS) {
              fprintf(stdout,"TASK_END   : core %d thread %d TASK#: %d\n",
                core, thread, CurrentTask[tid]);
            }
            #endif
          }
          break; } 
    case EVT_RTM_TASK_RESET: { RTM_TaskLength.reset(tid);
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"TASK_RESET : core %d thread %d TASK# %d\n",
              core, thread, CurrentTask[tid]);
          }
          #endif
          break; }
    //---------------------------------------------------//
    case EVT_RTM_DEQUEUE_START: { RTM_Dequeue.start(tid); 
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"DEQ_START  : core %d thread %d TASK#: %d\n",
              core, thread, CurrentTask[tid]);
          }
          #endif
          break; }
    case EVT_RTM_DEQUEUE_END: { RTM_Dequeue.end(tid);
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"DEQ_END    : core %d thread %d TASK#: %d\n",
              core, thread, CurrentTask[tid]);
          }
          #endif
          break; }
    case EVT_RTM_DEQUEUE_RESET: { RTM_Dequeue.reset(tid);
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"DEQ_RESET  : core %d thread %d TASK# %d\n",
              core, thread, CurrentTask[tid]);
          }
          #endif
          break; }
    //---------------------------------------------------//
    case EVT_RTM_ENQUEUE_START: { RTM_Enqueue.start(tid);
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"ENQ_START  : core %d thread %d TASK#: %d\n",
              core,CurrentTask[tid]);
          }
          #endif
          break; }
    case EVT_RTM_ENQUEUE_END: { RTM_Enqueue.end(tid);
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"ENQ_END    : core %d thread %d TASK#: %d\n",
              core, thread, CurrentTask[tid]);
          }
          #endif
          break; }
    case EVT_RTM_ENQUEUE_RESET: { RTM_Enqueue.reset(tid);
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"ENQ_RESET  : core %d thread %d TASK# %d\n",
              core, thread, CurrentTask[tid]);
          }
          #endif
          break; }
    //---------------------------------------------------//
    case EVT_RTM_ENQUEUE_GROUP_START: { RTM_EnqueueGroup.start(tid);
          break; }
    case EVT_RTM_ENQUEUE_GROUP_END: { RTM_EnqueueGroup.end(tid);
          break; }
    case EVT_RTM_ENQUEUE_GROUP_RESET: { RTM_EnqueueGroup.reset(tid);
          break; }
    //---------------------------------------------------//
    case EVT_RTM_BARRIER_START: { RTM_Barrier.start(tid);
          break; }
    case EVT_RTM_BARRIER_END: { RTM_Barrier.end(tid);
          break; }
    case EVT_RTM_BARRIER_RESET: { RTM_Barrier.reset(tid);
          break; }
    //---------------------------------------------------//
    case EVT_RTM_IDLE_START: { RTM_Idle.start(tid);
          break; }
    case EVT_RTM_IDLE_END: { RTM_Idle.end(tid);
          break; }
    case EVT_RTM_IDLE_RESET: { RTM_Idle.reset(tid);
          break; }
    //---------------------------------------------------//


    case EVT_RCUDA_KERNEL_START:
        RCUDA_KernelLength.start(tid);
        break;

    case EVT_RCUDA_KERNEL_END:
        RCUDA_KernelLength.end(tid);
        break;

    case EVT_RCUDA_KERNEL_LAUNCH_START:
        RCUDA_KernelLaunch.start(tid);
        break;
    
    case EVT_RCUDA_KERNEL_LAUNCH_END:
        RCUDA_KernelLaunch.end(tid);
        break;

    case EVT_RCUDA_BLOCK_FETCH_START:
        RCUDA_BlockFetch.start(tid);
        break;

    case EVT_RCUDA_BLOCK_FETCH_END:
        RCUDA_BlockFetch.end(tid);
        break;

    case EVT_RCUDA_BARRIER_START:
        RCUDA_Barrier.start(tid);
        break;
    case EVT_RCUDA_BARRIER_END:
        RCUDA_Barrier.end(tid);
        break;
    case EVT_RCUDA_THREAD_ID_START:
        RCUDA_ThreadId.start(tid);
        break;
    case EVT_RCUDA_THREAD_ID_END:
        RCUDA_ThreadId.end(tid);
        break;
    case EVT_RCUDA_SYNC_THREADS_START:
        RCUDA_SyncThreads.start(tid);
        break;
    case EVT_RCUDA_SYNC_THREADS_END:
        RCUDA_SyncThreads.end(tid);
        break;
    
    case EVT_RCUDA_INIT_START:
        RCUDA_Init.start(tid);
        break;

    case EVT_RCUDA_INIT_END:
        RCUDA_Init.end(tid);
        break;

/*

    case EVT_RTM_TASK_START: { RTM_TaskLength.start(tid);
          gTaskCount++; // get next task count
          assert( NULL != CurrentTask && "CurrentTask[] uninitialized!");
          CurrentTask[tid] = gTaskCount;
          #ifdef DEBUG_SHARING
          if(rigel::CMDLINE_DUMP_SHARING_STATS) {
            fprintf(stdout,"TASK_START : core %d thread %d TASK#: %d\n",
              core,thread,CurrentTask[tid]);
          }
          #endif
          break; }
    case EVT_RTM_TASK_END: { RTM_TaskLength.end(tid); 
          // make sure we've started a task before we try to end one...
          if( gTaskCount >= 0 && CurrentTask[tid] >= 0 ) {
            #ifdef DEBUG_SHARING
            if(rigel::CMDLINE_DUMP_SHARING_STATS) {
              fprintf(stdout,"TASK_END   : core %d thread %d TASK#: %d\n",
                core, thread, CurrentTask[tid]);
            }
            #endif
          }
          break; } 

*/
    default: { goto error_case; }
  }

  return;

error_case:
  DEBUG_HEADER();
  fprintf(stderr, "Unknown event number. (event: %d core: %d thread: %d)\n",
    event, core, thread);
  assert(0);
}

////////////////////////////////////////////////////////////////////////////////
/// This method is a friend of EventCounter for using its list directly.
////////////////////////////////////////////////////////////////////////////////
void 
EventTrack::PrintResultsRTM(FILE * output_fp)
{
	if(RTM_Dequeue.get_count() > 0) {
	  for(std::vector<EventCounter *>::const_iterator it = RTMEventCounters.begin(),
			                                              end = RTMEventCounters.end();
																										it != end;
																										++it) {
		  (*it)->print_event(output_fp);
		}
  	fprintf(output_fp, "TASKS_PER_BARRIER, %f\n", 
    	(1.0f*RTM_Dequeue.get_count())/(RTM_Barrier.get_count() )); 
  	fprintf(output_fp, "TASKS_PER_BARRIER_PER_CORE, %f\n",
   	  ((1.0f*RTM_Dequeue.get_count()) /
    	 (1.0f*RTM_Barrier.get_count()) / rigel::CORES_TOTAL ));
  	fprintf(output_fp, "TASKS_PER_BARRIER_PER_THREAD, %f\n",
    	((1.0f*RTM_Dequeue.get_count()) /
    	 (1.0f*RTM_Barrier.get_count()) / rigel::THREADS_TOTAL ));
  }
	if(RCUDA_Init.get_count() > 0) {
		for(std::vector<EventCounter *>::const_iterator it = RTMEventCounters.begin(),
		                                                end = RTMEventCounters.end();
																							      it != end;
																							      ++it) {
			      (*it)->print_event(output_fp);
	  }
	}
}

static void init_and_add(EventCounter &ec, std::vector<EventCounter *> &vec)
{
	ec.init();
	vec.push_back(&ec);
}

/// init()
/// We need to wait until after the number of cores is set to initialize some of
/// the fields.
void 
EventTrack::init()
{
  init_and_add(RTM_TaskLength, RTMEventCounters);
  init_and_add(RTM_EnqueueGroup, RTMEventCounters);
	init_and_add(RTM_Enqueue, RTMEventCounters);
	init_and_add(RTM_Dequeue, RTMEventCounters);
	init_and_add(RTM_Barrier, RTMEventCounters);
	init_and_add(RTM_Idle, RTMEventCounters);

	init_and_add(RCUDA_KernelLength, RCUDAEventCounters);
	init_and_add(RCUDA_KernelLaunch, RCUDAEventCounters);
	init_and_add(RCUDA_BlockFetch, RCUDAEventCounters);
	init_and_add(RCUDA_Barrier, RCUDAEventCounters);
	init_and_add(RCUDA_ThreadId, RCUDAEventCounters);
	init_and_add(RCUDA_SyncThreads, RCUDAEventCounters);
	init_and_add(RCUDA_Init, RCUDAEventCounters);

  CurrentTask = new int[rigel::THREADS_TOTAL];
  for(int i=0; i<rigel::THREADS_TOTAL; i++) {
    CurrentTask[i] = -1;
  }
  gTaskCount = -1;

  // Global count of barriers incremented at every barrier init (only one core
  // will start the barrier from the RTM code)
  RTM_BarrierCount = 0;
}

