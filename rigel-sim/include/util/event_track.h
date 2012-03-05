#ifndef __EVENT_TRACK_H__
#define __EVENT_TRACK_H__
// This file contains the declaration of the EventTrack class for handling
// statistics tracking based on events occuring within simulation.  The
// events are triggered explicitly by target code with the 'event' instruction.

// Shared between RigelSim and libraries
#include "sim.h"
#include "profile/profile.h"
#include <string>
#include <stdint.h>

class CoreInOrderLegacy;

namespace rigel {
  namespace event {

    extern int gTaskCount;
    //Defined in src/event_track.cpp
    class EventTrack;
    extern class EventTrack global_tracker;

    // Track summation of event data for a counter.
    struct EventCounter 
    {
      // Per core running count of start/stop times.
      struct EventCounterPerThread {
        EventCounterPerThread() : cycle(0), instr(0) {}

        void reset() { 
          cycle = UINT64_MAX;
          instr = UINT64_MAX;
        }
        uint64_t cycle;
        uint64_t instr;
      };

      public:
        EventCounter(const char *name) : 
          total_count(0), total_time(0), total_instrs(0), event_name(name),
          min_time(UINT64_MAX), max_time(0)
        {
        }

        ~EventCounter() {
          delete [] last_start;
        }
        // Accessors
        uint64_t get_count() { return total_count; }
        EventCounterPerThread get_last_start(int tid) { return last_start[tid]; }
        uint64_t get_time() { return total_time; }

        // Initialize the per-core timers.
        void init() {
          last_start = new EventCounterPerThread[rigel::THREADS_TOTAL];
          for (int i = 0; i < rigel::THREADS_TOTAL; i++) { reset(i); }
        }
        // Start a counter.  It can be called multiple times to restart the
        // counter at a later time. 
        void start(int tid) {
          last_start[tid].cycle = rigel::CURR_CYCLE;
          last_start[tid].instr = ProfileStat::get_retired_instrs(tid);
        }
        // Add event from thread to running summation of event data.  Reset
        // the counter.
        void end(int tid) {
          // Check if there is a running counter.
          if ((uint64_t)(-1) == last_start[tid].cycle) return;

          uint64_t time = rigel::CURR_CYCLE - last_start[tid].cycle;
          uint64_t instrs = ProfileStat::get_retired_instrs(tid)
            - last_start[tid].instr;
          count_event(time, instrs);
          reset(tid);
        }
        // Clear the counter.
        void reset(int tid) {
          last_start[tid].reset();
        }
        // Print NAME, MEAN for counter
        void print_event(FILE * fp) {
          float mean_time = (1.0f * total_time)/(1.0f * total_count);
          float mean_instr = (1.0f * total_instrs)/(1.0f * total_count);
					std::string name("EVENT_");
          // Capitalize name.
          for (size_t i = 0; i < event_name.length(); i++) {
            name.push_back(toupper(event_name[i]));
          }
          fprintf(fp, "%s_COUNT, %llu\n", event_name.c_str(), (long long unsigned)total_count); 
          fprintf(fp, "%s_TIME, %llu\n", event_name.c_str(), (long long unsigned)total_time); 
          fprintf(fp, "%s_MIN_TIME, %llu\n", event_name.c_str(), (long long unsigned)min_time); 
          fprintf(fp, "%s_MAX_TIME, %llu\n", event_name.c_str(), (long long unsigned)max_time); 
          fprintf(fp, "%s_MEAN_TIME, %f\n", event_name.c_str(), mean_time); 
          fprintf(fp, "%s_MEAN_INSTR, %f\n", event_name.c_str(), mean_instr); 
          fprintf(fp, "%s_PERCENT_RUNTIME, %f\n", event_name.c_str(), (100.0f * total_time) /
            (1.0f * ProfileStat::get_active_cycles() * rigel::THREADS_TOTAL));
        }
        void count_event(unsigned int time, int instrs) { 
          total_count++;
          total_time += time;
          total_instrs += instrs;
          // min & max (store time)
          if(time < min_time && time > 0) {
            min_time = time;
          }
          if(time > max_time) {
            max_time = time; 
          }
        }

      private:
        // Update running total for the event.
        // Running total across all threads.
        uint64_t total_count;
        uint64_t total_time;
        uint64_t total_instrs;
				std::string event_name;
        // min & max field
        uint64_t min_time;
        uint64_t max_time;
      protected:
        // Array of last start time for each thread.
        EventCounterPerThread *last_start;
    };
    // EventCounterWaitForAllThreads
    //
    // Extend EventCounter for counters that should only trigger once when
    // all threads have called end().  The last (temporally) start is used as
    // the initial point of the counter.
    struct EventCounterWaitForAllThreads : public EventCounter
    {
      public:
        EventCounterWaitForAllThreads(const char *name, EventTrack *_parent) :
          EventCounter(name), pending_thread(-1), parent(_parent) { }


        // Add event from core to running summation of event data.
        void end(int tid); //Uses parent pointer, so needs to be defined in .cpp file.
        // At any start, all threads are reset.
        void start(int tid) {
          pending_thread = tid;
          EventCounter::start(0);
        }
        void reset(int tid) {
          if (pending_thread == tid) EventCounter::reset(0);
        }

      private:
        int pending_thread;
        EventTrack *parent;
    };

    class EventTrack
    {
      public:
        // Methods for accessing statistics from within the simulator
        uint64_t GetRTMBarrierCount() { return RTM_BarrierCount; }
        uint64_t GetRTMTaskCount() { return RTM_Dequeue.get_count(); }
        int      GetCurrentTaskNum( int tid ) {
                    return CurrentTask[tid];
        }
        uint64_t RTM_BarrierCount;

      public:
        // Default constructor
        EventTrack();
        // Perform appropriate action for event on a thread.
        // tid is a global thread id.
        void HandleEvent(event_track_t event, int tid,CoreInOrderLegacy* cptr);
        // Display event results for end-of-simulation Profiler dump.
        void PrintResultsRTM(FILE * output_fp);
        // Initialize data structures for class.
        void init();

				std::vector<EventCounter *>RTMEventCounters;
				std::vector<EventCounter *>RCUDAEventCounters;

        // Task start/end times after dequeue
        EventCounter RTM_TaskLength;
        // Enqueue time
        EventCounter RTM_Enqueue;
        // Enqueue time
        EventCounter RTM_EnqueueGroup;
        // Dequeue time
        EventCounter RTM_Dequeue;
        // Barrier time
        EventCounterWaitForAllThreads RTM_Barrier;
        // Idle time
        EventCounter RTM_Idle;

        // per-thread task number tracking
        static int* CurrentTask;

        // RCUDA Event Tracking
        EventCounter RCUDA_KernelLength;
        EventCounter RCUDA_KernelLaunch;
        EventCounter RCUDA_BlockFetch;
        EventCounter RCUDA_Barrier;
        EventCounter RCUDA_ThreadId;
        EventCounter RCUDA_SyncThreads;
        EventCounter RCUDA_Init;
    };
  }
}

#endif
