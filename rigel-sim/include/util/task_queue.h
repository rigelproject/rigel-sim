////////////////////////////////////////////////////////////////////////////////
// task_queue.h
////////////////////////////////////////////////////////////////////////////////
//  
//  This file contains the definition of the hardware task queue system for RTM.
//  It should only be used for fast prototyping and debugging of code since it
//  is highly idealized.
//  
////////////////////////////////////////////////////////////////////////////////
// More notes on Rigel Task Queing Mechanism:
//
// TaskSystemBase contains the interface to a Carbon-like [Kumar et al. ISCA'07]
// task queuing system.  Each implementation should implement this interface.
// The pipeline and the software compilation toolchain will use the same API to
// access the task queues.
//
// TaskSystemBaseline is a simple implementation with a single global queue
// with no memory backing.  Later we will want to add support to allow a TQ_Init
// to handoff some memory to the TQ System for its own use.  For now, that is a
// NOP and the task queue is a magical structure that is separate from any of
// the caches or memory.
//
// Instructions:
//
// All instructions use r44, r45, r46, r47 as sources.  The task descriptor
// lives in these four registers.  Error/status returned in r43.  Dequeues return
// task in [r44..r47]
//
// tq.init    
// tq.end 
// tq.enqueue <ip>, <data>, <begin>, <end>
// tq.loop    <ip>, <data>, <N>, <S>
// tq.dequeue   Returns: <ip> <data> <begin> <end>
//
#ifndef __TASK_QUEUE_H__
#define __TASK_QUEUE_H__


#include <deque>
#include <string>
#include <set>
#include <list>
#include <utility> //For std::pair
#include <stdint.h>
#include <vector>
#include "sim.h"
#include "profile/profile.h"

class CoreInOrderLegacy;

/////////////////////////////////////////////////////////////////////////////
// struct TaskQueueDesc
/////////////////////////////////////////////////////////////////////////////
// Information about a particular task queue.  We may need to track multiples
/////////////////////////////////////////////////////////////////////////////
typedef struct TaskQueueDesc {
  TaskQueueDesc(int id = -1, int size = rigel::task_queue::QUEUE_MAX_SIZE) {
    MaxSize = size;
    TaskQueueID = id;
  }

  int MaxSize;
  int TaskQueueID;
} TaskQueueDesc;

/////////////////////////////////////////////////////////////////////////////
// typedef struct TaskDescriptor
/////////////////////////////////////////////////////////////////////////////
// Task descriptor held in in-memory queues
/////////////////////////////////////////////////////////////////////////////
typedef struct TaskDescriptor {
  ////////////////////////////////////////////////////////////////////
  // XXX: Updates here must also go into the includes/ for LLVM XXX // 
  ////////////////////////////////////////////////////////////////////
  TaskDescriptor() {
    clear();
  }

  void clear() {
    ip = 0xdeadbeef;
    data = 0;
    begin = 0;
    end = 0xFFFFFFFF;
  }

  // Start of task location
  union { RIGEL_ADDR_T ip;  uint32_t v1; };
  union { uint32_t data;    uint32_t v2; };
  union { uint32_t begin;   uint32_t v3; };
  union { uint32_t end;     uint32_t v4; };

  // Set up comparison operators so we can track descriptors in sets and other
  // sorted list containers
  inline bool operator==(const TaskDescriptor &td) const {
    return ((v1 == td.v1) && (v2 == td.v2) && (v3 == td.v3) && (v4 == td.v4));
  }
  // The less than operator is a bit contrived here...there is no real meaning
  // to it, it just provides an ordering that is required by STL -
  inline bool operator<(const TaskDescriptor &td) const {
    return ((v1 < td.v1) || (v2 < td.v2) || (v3 < td.v3) || (v4 < td.v4));
  }
} TaskDescriptor;

 ///////////////////////////////////////////////////////////////////
 // Typedef event_track_t
 ///////////////////////////////////////////////////////////////////
 // Enumerated type used for keeping track of the core state 
 // in the simulator
 ///////////////////////////////////////////////////////////////////
typedef enum TQ_CoreStateType {
  ////////////////////////////////////////////////////////////////////
  // XXX: Updates here must also go into the includes/ for LLVM XXX // 
  ////////////////////////////////////////////////////////////////////
  TQ_STATE_ACTIVE,        // Processor is active, queue is not doing anything.

  TQ_STATE_BLOCKING,      // Waiting on an event...

  TQ_STATE_UNBLOCK,       // The task is now unblocked so send a SYNC message

  TQ_STATE_PENDING_END,   // A TQ_End was called while this was blocking

  TQ_STATE_END_UNBLOCK,   // All blocked tasks become unblocked

  TQ_STATE_INVALID,       // Used for debugging.  Should never occur in practice

  // XXX: The following states are used to create variable latencies for task
  // queue operations.
  TQ_STATE_PENDING_DEQUEUE,       // A task has been dequeued, but not delivered
  TQ_STATE_PENDING_ENQUEUE_ONE,   // Waiting before single enqueue can finish
  TQ_STATE_PENDING_ENQUEUE_LOOP,  // Waiting for a loop to complete enqueue
  TQ_STATE_PENDING_UNBLOCK // All threads have reached a barrier...bleed the clock a little

} TQ_CoreStateType;

/////////////////////////////////////////////////////////////////////////////
// typedef struct TQ_CoreStateDesc
/////////////////////////////////////////////////////////////////////////////
// Struct that keeps track of the TQ state local to a core
/////////////////////////////////////////////////////////////////////////////
typedef struct TQ_CoreStateDesc {
  // Constructor
  TQ_CoreStateDesc() { 
    Reset();
    PendingTaskDescriptor.clear();
  }

  // Call before Clock() and after all updates are completed and count number to
  // see if we are at global unblocking
  bool CheckIfAtSync() { 
    return (NextState == TQ_STATE_BLOCKING); 
  }

  // Returns true when the last command is allowed to unblock.  It is one not
  // zero since it gets delivered next cycle
  bool CheckReady() { return (CyclesReady <= 1); }

  // Latch the next state.  Call every cycle?
  void Clock() {
    CurrState = NextState;
    // Default value for NextState.  May be updating in the next cycle
    NextState = TQ_STATE_ACTIVE;

    switch (CurrState) {
      case TQ_STATE_PENDING_DEQUEUE:
      case TQ_STATE_PENDING_ENQUEUE_ONE:
      case TQ_STATE_PENDING_ENQUEUE_LOOP:
      case TQ_STATE_PENDING_UNBLOCK:
      {
        // Advances cycle...when CyclesReady is zero, we can return the data
        CyclesReady--;
        NextState = CurrState;
        break;
      }
        // Once an End is signaled, all cores keep that state until otherwise
        // changed
      case TQ_STATE_END_UNBLOCK:
      case TQ_STATE_UNBLOCK:
      case TQ_STATE_BLOCKING:
      {
        NextState = CurrState;
        break;
      } 
      default:
        NextState = TQ_STATE_ACTIVE;
    }
  }
  // Clear the state of the latch
  void Reset() {
    CurrState = TQ_STATE_ACTIVE;
    NextState = CurrState;
    CyclesReady = 0;
  }
  // Set the state to be updated at the next Clock() edge
  void Update(TQ_CoreStateType state) {
    using namespace rigel::task_queue;
    switch (state) {
      case TQ_STATE_PENDING_DEQUEUE:  
        CyclesReady = LATENCY_DEQUEUE; 
        break;
      case TQ_STATE_PENDING_ENQUEUE_ONE: 
        CyclesReady = LATENCY_ENQUEUE_ONE; 
        break;
      case TQ_STATE_PENDING_ENQUEUE_LOOP: 
        CyclesReady = LATENCY_ENQUEUE_LOOP; 
        break;
      case TQ_STATE_PENDING_UNBLOCK: 
        CyclesReady = LATENCY_BLOCKED_SYNC; 
        break;
      default: /* NADA */
        break;
    }
    NextState = state;
  }
  // Set the CurrState immediately.  Useful for doing global updates after
  // Clock() has been called for a cycle.  Otherwise, Update is probably safer.
  void Set(TQ_CoreStateType state) {
    CurrState = state;
  }

  TQ_CoreStateType CurrState;
  TQ_CoreStateType NextState;
  // Number of cycles until the value/response can be delivered to the core
  int CyclesReady;
  // For a TQ_Dequeue, holds the task descriptor to return to the core after the
  // dqueue latency has elapsed.  For a TQ_Enqueue, holds the task descriptor
  // until the enque latency has elapsed at which point the task(s) are inserted
  // into the queue.
  TaskDescriptor PendingTaskDescriptor;
} TQ_CoreStateDesc;

/////////////////////////////////////////////////////////////////////////////
// Class TaskSystemBase
/////////////////////////////////////////////////////////////////////////////
// Baseline is a direct implementaiton of Carbon from Kumar et al. ISCA'07
/////////////////////////////////////////////////////////////////////////////
class TaskSystemBase {
  //public member functions
  public:
    // Default constructor
    TaskSystemBase() {
      TQ_CoreState = new TQ_CoreStateDesc[rigel::CORES_TOTAL];
    }
    virtual ~TaskSystemBase() {
      delete [] TQ_CoreState;
    }

    // GetQueueType:  Returns a string with the name of the queue type.  Used in
    //                profiler and must be implemented by all derived classes.
		std::string GetQueueType();


    // TQ_Init: Initialize the TaskSystem.  
    //
    // Returns: TQ_RET_SUCCESS - a valid taskqueue descriptor assigned to
    //            tqdesc.
    //          TQ_RET_INIT_FAIL  - An error has occured.  Implementation
    //            dependant information is stashed in TaskQueueDesc.
    TQ_RetType TQ_Init(int CoreNum, int NumThreads, TaskQueueDesc &tqdesc);
    
    // TQ_End: No new task are to be enqueued.  All blocked threads are unblocked
    //
    // Returns: TQ_RET_SUCCESS when completed
    TQ_RetType TQ_End(int CoreNum);
    
    // TQ_Enqueue: Insert a task into the task queue
    //
    // Returns: TQ_RET_SUCCESS - Task is inserted, no errors occurred
    //          TQ_RET_BLOCK - Unable to enqueue this cycle, try again next
    //            cycle.  Used to avoid race conditions when other task is doing
    //            a globally-visible update such as dealing with enlarging the
    //            task queue.
    //          TQ_RET_OVERFLOW - There is no more room in the queue.  XXX: Software
    //            needs to deal with it somehow. 
    TQ_RetType TQ_Enqueue(int CoreNum, TaskDescriptor &tdesc);
    
    // TQ_EnqueueLoop: Insert a range of tasks.  v1/v2 are not assigned meaning
    // by the hardware.  N is the number of iterations total.  S is the number
    // of iterations to do per task.   tdesc is replicated for each task with
    // the data field taking the iteration number as input
    //
    // XXX: This is all or nothing: Either all tasks are enqueued or none are
    // and a condition is flagged.  Core should call at EX stage.  Treat like
    // memory operation?
    //
    // Returns: Same as TQ_Enqueue()
    TQ_RetType TQ_EnqueueLoop(int CoreNum, TaskDescriptor &tdesc, int N, int S);
    
    // TQ_Dequeue: Removes the 'next' task descriptor from the queue.  Blocks if
    // no tasks are available.  May return with no task when a TQ_End is
    // signaled by another thread.  The task is removed from the queue and no
    // free()'ing must occur.
    //
    // Returns: TQ_RET_SUCCESS - tdesc contains a valid descriptor
    //          TQ_RET_BLOCK - No tasks are available this cycle so the core
    //            must stall.  Retry next cycle.
    //          TQ_RET_END - End was signalled while we were blocking.  The core
    //            may take a different control path in this case.
    //          TQ_RET_UNDERFLOW - An underflow condition has occured and the
    //            core needs to refill the task queues from memory somehow.
    //            May not be applicable to Rigel.
    //          TQ_RET_SYNC - Everyone has started BLOCK'ing and there are no
    //            new tasks.  This forms a barrier.
    TQ_RetType TQ_Dequeue(int CoreNum, TaskDescriptor &tdesc);

    // TQ found a Sync, we need to update the interval for all of the tasks
    virtual void SyncCallback() = 0;
    
    // TQ_CoreStateDesc Holds the state of each core in the system w.r.t. the
    // task queing system.  Set in the constructor.  Called at 
  //  TQ_CoreStateDesc *TQ_CoreState;
    TQ_CoreStateDesc *TQ_CoreState;
    
    // PerCycle(): Clocks the task queue structure by latching new values.
    // Must be called at the **START** of every cycle.
    void PerCycle(); 
};

/////////////////////////////////////////////////////////////////////////////
// Class TaskSystemBaseline
/////////////////////////////////////////////////////////////////////////////
// Builds upon Carbon Model. Adds specific function pointers which are
// helper functions for storing and removing tasks.
/////////////////////////////////////////////////////////////////////////////
class TaskSystemBaseline : public TaskSystemBase{
  //public member functions
  public:
    TaskSystemBaseline();
    virtual ~TaskSystemBaseline() {};
    virtual std::string GetQueueType() { return std::string("TQ_SYSTEM_BASELINE"); }

    // TQ_Init: 
    TQ_RetType TQ_Init(int CoreNum, int NumThreads, TaskQueueDesc &tqdesc);
    
    // TQ_End: 
    TQ_RetType TQ_End(int CoreNum);
    
    // TQ_Enqueue:
    virtual TQ_RetType TQ_Enqueue(int CoreNum, TaskDescriptor &tdesc);
    
    // TQ_EnqueueLoop:    
    TQ_RetType TQ_EnqueueLoop(int CoreNum, TaskDescriptor &tdesc, int N, int S);
    
    // TQ_Dequeue:
    virtual TQ_RetType TQ_Dequeue(int CoreNum, TaskDescriptor &tdesc);

    // TQ_PerCycle: Use TaskSystemBase::PerCycle()
    // void TQ_PerCycle() { TaskSystemBase::PerCycle(); }
    //TQ_CoreStateDesc *TQ_CoreState;
    virtual void SyncCallback() { /* NADA */ }

    // Handle returning a task to the dequeuer.  It is called from TQ_Dequeue,
    // but the child version is used if it is the actual dequeuer.
    virtual void SelectNextTask(int CoreNum, TaskDescriptor &tdesc) {
      tdesc = TaskQueue.front();
      TaskQueue.pop_front();
      Profile::global_task_stats.sched_without_cache_affinity++;
    }

  //public data members
  public:
  
    // Double-ended queue for holding tasks.  
		std::deque<TaskDescriptor> TaskQueue;

  //public data members
  private:
    // Lock the queue for an entire cycle.  XXX: If we parallelize this, we may
    // need to rethink how we handle the lock.
    bool GlobalQueueLock;

    // Maximum number of tasks that can be inserted.  Inserting QueueMaxSize+1
    // tasks will result in an overflow.
    int QueueMaxSize;

    // Overflow was found and a TQ_RET_OVERFLOW  was sent to a core to deal with it
    bool OverflowPending;
    // Ditto, but for Underflow
    bool UnderflowPending;

};

#include <set>
#include <list>

/////////////////////////////////////////////////////////////////////////////
// Helper functions for TaskQueue Simulation
/////////////////////////////////////////////////////////////////////////////
int Stack_RangeMatch(std::list<TaskDescriptor> &task_stack, const TaskDescriptor &tdesc);
int Stack_SingleMatch(std::list<TaskDescriptor> &task_stack, const TaskDescriptor &tdesc, int field);
void Stack_InsertTask(std::list<TaskDescriptor> &task_stack, const TaskDescriptor &tdesc, int TaskStackMaxSize, int InsertedTaskCount);
int Stack_FullMatch(std::list<TaskDescriptor> &task_stack, const TaskDescriptor &tdesc);
int Stack_NearSingleMatch(std::list<TaskDescriptor> &task_stack, 
  const TaskDescriptor &tdesc, int field, int range); 
int Stack_NearRangeMatch(std::list<TaskDescriptor> &task_stack, 
  const TaskDescriptor &tdesc, int range);

/////////////////////////////////////////////////////////////////////////////
// typedef AffinityDataInterface
/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////
typedef struct AffinityDataInterface {
  public: 
    // Clean everything and reset counters
    void Clear();
    // Returns the number of inserted tasks for the interval
    int InsertedTaskCount();
    // Insert the task into the various filters
    void InsertTask(const TaskDescriptor &tdesc); 
    // Return the affinity for a task using the default method
    int CheckTask(const TaskDescriptor &tdesc);
} AffinityDataInterface;

// TODO: Template this to use different affinity functions
typedef struct AffinityDataInterval : public AffinityDataInterface{
  public: 
    // Clean everything and reset counters
    virtual void Clear();
    // Returns the number of inserted tasks for the interval
    virtual int InsertedTaskCount() { return tasks_match.size(); }
    // Insert the task into the various filters
    virtual void InsertTask(const TaskDescriptor &tdesc); 
    // For now we do a full check
    virtual int CheckTask(const TaskDescriptor &tdesc) { return CheckTaskFull(tdesc); }
    // Default destructor
    virtual ~AffinityDataInterval() {}

  //private member functions
  private:
      // Return the number of v fields the task matches for.  XXX: Note that because
    // it is a multiset, you could weight each field by the number of times it
    // matches.  This is a simple hueristic we can play with later 
    int CheckTaskPartial(const TaskDescriptor &tdesc);
    // Return one if the exact same task was in the last interval, zero
    // otherwise.
    int CheckTaskFull(const TaskDescriptor &tdesc) {
      return (tasks_match.count(tdesc) > 0);
    }
    // Returns number of tasks that had a range that matches the interval
    // [begin, end) fully  overlaps, zero otherwise.
    int CheckRangeMatch(const TaskDescriptor &tdesc) {
        return range_match.count(std::pair<uint32_t, uint32_t>(tdesc.v3, tdesc.v4));
    }

  // Data for struct.  Must be copied over to last state at barrier.
  private:
    // Keep each task value in a set for fast matching
    std::multiset<uint32_t> v1_match, v2_match, v3_match, v4_match;
    // Keep list of all tasks inserted during the interval
    std::multiset<TaskDescriptor> tasks_match;
    // Keep all of the ranges around for performing fast range matches
    std::multiset< std::pair<uint32_t, uint32_t> > range_match;


} AffinityDataInterval;

typedef struct AffinityDataInterval_LIFO : public AffinityDataInterface {
  //public member functions
  public:
    AffinityDataInterval_LIFO() : TaskStackMaxSize(rigel::task_queue::INTERVAL_LIFO_STACK_SIZE) { }

    // Clear out all elements from the task stack
    void Clear() { task_stack.clear(); }
    // Returns the number of elements currently in the task stack
    int InsertedTaskCount() { return task_stack.size(); }
    // Insert a task at the rear of the stack (list)
    virtual void InsertTask(const TaskDescriptor &tdesc);
    // Do a full check against our stack
    virtual int CheckTask(const TaskDescriptor &tdesc) { return CheckTaskStack_RangeMatch(tdesc); }
    virtual ~AffinityDataInterval_LIFO() {}

  //private member functions
  private:
    // Return the depth in the stack of a matching task  Larger number means
    // better affinity (more recently used)
    int CheckTaskStack_FullMatch(const TaskDescriptor &tdesc);
    int CheckTaskStack_RangeMatch(const TaskDescriptor &tdesc);

  //private data members
  private:
    // Stack gives LIFO ordering on tasks.  Insertions happen at dequeue along
    // with removals at the end to keep the size bounded.  Insertions occur at
    // the end and values drop off at the beginning.
    std::list<TaskDescriptor> task_stack;

    // Maximum size of task stack.  Removals happen at insertion time (in
    // necessary)
    int TaskStackMaxSize;

} AffinityDataInterval_LIFO;


/////////////////////////////////////////////////////////////////////////////
// Class TQ_ClusterAffinityInfo
/////////////////////////////////////////////////////////////////////////////
// Track information about what clusters get what tasks each interval.  An
// interval is defined as the time between two syncs.  There is one of these for
// each cluster in the system. 
/////////////////////////////////////////////////////////////////////////////
template <typename T>
class TQ_ClusterAffinityInfo {
  public:
    // Called on every task given to a cluster to insert the task into the current
    // task list for this interval
    void InsertTask(const TaskDescriptor &tdesc) {
      // Insert the task for the current interval
      CurrInterval.InsertTask(tdesc);
    }
    // A barrier is reached, clock the latches
    void NextInterval() {
      // If no tasks were inserted last interval, do nothing
      if (CurrInterval.InsertedTaskCount() == 0) return;

      // Copy the last interval over and reset CurrInterval for the upcoming
      // interval.  TODO: This may be faster with double-buffering pointers 
      LastInterval = CurrInterval;
      CurrInterval.Clear();
    }
    // Returns an integer representing the affinity of a potential task (tdesc)
    // for this core.  A higher value means it is a better match.  To make it
    // run faster, once a task is found with a threshold value greater than the
    // provided threshold, the function returns the affinity found.
    int GetAffinity(const TaskDescriptor &tdesc, int threshold = 10000) {
      return LastInterval.CheckTask(tdesc);
    }

    T CurrInterval;
    T LastInterval;
};

/////////////////////////////////////////////////////////////////////////////
// Class TQ_ClusterAffinityInfoCurrent
/////////////////////////////////////////////////////////////////////////////
// Builds upon TQ_ClusterAffinityInfo. The only thing that is different 
// is that we want to use the current interval and not the last interval
// for tracking.
/////////////////////////////////////////////////////////////////////////////
template <typename T>
class TQ_ClusterAffinityInfoCurrent : public TQ_ClusterAffinityInfo<T> {
  public:
    void NextInterval() { /* NADA */ }
    int GetAffinity(const TaskDescriptor &tdesc, int threshold = 10000) {
      return TQ_ClusterAffinityInfo<T>::CurrInterval.CheckTask(tdesc);
    }
};

/////////////////////////////////////////////////////////////////////////////
// Class TaskSystemInterval
/////////////////////////////////////////////////////////////////////////////
// Adds tracking and builds upon the TaskSystemBaseline class.
/////////////////////////////////////////////////////////////////////////////
class TaskSystemInterval : public TaskSystemBaseline {
  public: 
    TaskSystemInterval() : TaskSystemBaseline(), ClusterInfo(rigel::NUM_CLUSTERS) {
    }
    virtual ~TaskSystemInterval() { }
    virtual std::string GetQueueType() { return std::string("TQ_SYSTEM_INTERVAL"); }

    // Handle returning a task to the dequeuer.  It is called from TQ_Dequeue,
    // but the child version is used if it is the actual dequeuer.
    virtual void SelectNextTask(int CoreNum, TaskDescriptor &tdesc);

    // Hook the defaults to track affinity
    virtual TQ_RetType TQ_Enqueue(int CoreNum, TaskDescriptor &tdesc) {
      return TaskSystemBaseline::TQ_Enqueue(CoreNum, tdesc);
    }

    virtual TQ_RetType TQ_Dequeue(int CoreNum, TaskDescriptor &tdesc);

    virtual void SyncCallback();

  //public data members
  public:
		std::vector<TQ_ClusterAffinityInfo<AffinityDataInterval> > ClusterInfo;
};

/////////////////////////////////////////////////////////////////////////////
// Class TaskSystemIntervalLIFO
/////////////////////////////////////////////////////////////////////////////
// Adds tracking and builds upon the TaskSystemBaseline class. Adds 
// A LIFO (stack) interface to the TaskQueue.
/////////////////////////////////////////////////////////////////////////////
class TaskSystemIntervalLIFO : public TaskSystemBaseline {
  public: 
    TaskSystemIntervalLIFO() : TaskSystemBaseline(),
      ClusterInfo(rigel::NUM_CLUSTERS)
    {
    }
    virtual ~TaskSystemIntervalLIFO() { }
    virtual std::string GetQueueType() { return std::string("TQ_SYSTEM_INT_LIFO"); }

    // Handle returning a task to the dequeuer.  It is called from TQ_Dequeue,
    // but the child version is used if it is the actual dequeuer.
    virtual void SelectNextTask(int CoreNum, TaskDescriptor &tdesc);

    // Hook the defaults to track affinity
    virtual TQ_RetType TQ_Enqueue(int CoreNum, TaskDescriptor &tdesc) {
      return TaskSystemBaseline::TQ_Enqueue(CoreNum, tdesc);
    }

    virtual TQ_RetType TQ_Dequeue(int CoreNum, TaskDescriptor &tdesc) {
      TQ_RetType retval = TaskSystemBaseline::TQ_Dequeue(CoreNum, tdesc);

      return retval;
    }
    // TQ found a Sync, we need to update the interval for all of the tasks
    virtual void SyncCallback();

  //public data member
  public:
		std::vector<TQ_ClusterAffinityInfo<AffinityDataInterval_LIFO> > ClusterInfo;
    // If we find a task that is this close to us, we stop searching.  Should
    // always be set to a number smaller than TaskStackMaxSize
    //const int LIFOSelectionThreshold;
};


#endif
