
////////////////////////////////////////////////////////////////////////////////
// taskqueue.cpp
////////////////////////////////////////////////////////////////////////////////
// This file handles the implementation of the hardware
// task queuing system derived from Carbon
////////////////////////////////////////////////////////////////////////////////


#include <cassert>                      // for assert
#include <stdint.h>                     // for uint32_t
#include <deque>                        // for deque, _Deque_iterator, etc
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <list>                         // for list, _List_iterator, etc
#include <set>                          // for multiset
#include <string>                       // for char_traits
#include <utility>                      // for pair
#include <vector>                       // for vector
#include "core.h"           // for CoreInOrderLegacy
#include "define.h"         // for TQ_RetType, etc
#include "profile/profile.h"        // for Profile, etc
#include "sim.h"            // for CORES_TOTAL, NUM_CLUSTERS, etc
#include "util/task_queue.h"     // for TaskDescriptor, etc
#include "util/task_queue_recent.h"  // for TaskSystemRecent

TQ_RetType 
TaskSystemInterval::TQ_Dequeue(int CoreNum, TaskDescriptor &tdesc) 
{
  TQ_RetType retval = TaskSystemBaseline::TQ_Dequeue(CoreNum, tdesc);
  // FIXME: This could get called a few times...need a good way to deal with it
  // clearing out new result.  For now, only let local core number 0 reset
  // it. 
  if (retval == TQ_RET_SYNC && CoreInOrderLegacy::LocalCoreNum(CoreNum) == 0)  {
    ClusterInfo[CoreInOrderLegacy::ClusterNum(CoreNum)].NextInterval();
  }
  // Add the task we are dequeing to the list of tasks that this cluster has
  // had *this* cycle
  if (retval == TQ_RET_SUCCESS) {
    ClusterInfo[CoreInOrderLegacy::ClusterNum(CoreNum)].InsertTask(tdesc);
  }
// TQ found a Sync, we need to update the interval for all of the tasks

  return retval;
}

void 
TaskSystemInterval::SyncCallback() 
{ 
  for (int i = 0; i < rigel::NUM_CLUSTERS; i++) {
    ClusterInfo[i].NextInterval();
  }
}

void 
TaskSystemIntervalLIFO::SyncCallback() 
{ 
  for (int i = 0; i < rigel::NUM_CLUSTERS; i++) {
    ClusterInfo[i].NextInterval();
  }
}
// Initialize a single global task queue.  We should make this more
// modular and probably not define the implementation here.  Do something
// similar to DDR model, maybe?
TaskSystemBaseline *GlobalTaskQueue;

////////////////////////////////////////////////////////////////////////////////
// namespace rigel::task_queue
////////////////////////////////////////////////////////////////////////////////
// Defined latency parameters for the hardware task queue
////////////////////////////////////////////////////////////////////////////////
namespace rigel {
	namespace task_queue {
		//!!!!!!!!TODO!!!!!!!!!!
		//!!!!!!!!TODO!!!!!!!!!!
		// The latency incurred from the core's perspective for each operation
		int LATENCY_ENQUEUE_ONE = 20;
		int LATENCY_ENQUEUE_LOOP = 20;
		int LATENCY_DEQUEUE = 20;
		// The delay from the queue realizing that all cores are blocking until when
		// it broadcasts the message that a sync point has been reached.
		int LATENCY_BLOCKED_SYNC = 100;
		//!!!!!!!!TODO!!!!!!!!!!
		//!!!!!!!!TODO!!!!!!!!!!
	}
}

////////////////////////////////////////////////////////////////////////////////
// TaskSystemBaseline
////////////////////////////////////////////////////////////////////////////////
// Default constructor for the Task Queue 
////////////////////////////////////////////////////////////////////////////////
TaskSystemBaseline::TaskSystemBaseline() : TaskSystemBase() {
	
	TaskQueueDesc tqdesc(0, 1024);
	// FIXME: Default values are needed to bootstrap the queue
	TQ_Init(0, 1024, tqdesc);

}

////////////////////////////////////////////////////////////////////////////////
// TaskSystemBaseline::TQ_Init
////////////////////////////////////////////////////////////////////////////////
// Description: Initializes the task queue; Is typically used by the constructor.
//
// Parameters: tqdesc - task descriptor used to setup TQ size and amount of
// queues
//
// Returns: Success when finished 
////////////////////////////////////////////////////////////////////////////////
TQ_RetType
TaskSystemBaseline::TQ_Init(int CoreNum, int NumThreads, TaskQueueDesc &tqdesc) {
	// Clear all of the per-core state information
	for (int i = 0; i < rigel::CORES_TOTAL; i++) {
		TQ_CoreState[i].Reset();
	}

	// Clear the queue
	TaskQueue.clear();
	QueueMaxSize = tqdesc.MaxSize;

	// Clear the state associated with the queue
	GlobalQueueLock = TQ_UNLOCKED;
	OverflowPending = false;
	UnderflowPending = false;

	// Only one queue in this model and it will be number zero.
	tqdesc = TaskQueueDesc(0);

 	// Init just resets and returns the zero queue
	#ifdef DEBUG_TASKQ
	DEBUG_HEADER();
	cerr << " TQ Initialized" << endl;
	#endif
	Profile::global_task_stats.tq_init_count++;
	return TQ_RET_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// TaskSystemBaseline::TQ_End
////////////////////////////////////////////////////////////////////////////////
// Deallocates the Taskqueue and ends
//
////////////////////////////////////////////////////////////////////////////////
TQ_RetType
TaskSystemBaseline::TQ_End(int CoreNum) {
	// Unblock everyone
	for (int i = 0; i < rigel::CORES_TOTAL; i++) {
		// FIXME: What happens if some are blocked and some are not?  Basically, two
		// pending ends...this is a race condition.  We can fix it by blocking a
		// TQ_End when any core has their current state set to TQ_STATE_END_UNBLOCK
		// still.  Considering this is only called at the end of the program, I
		// think it is okay.
		TQ_CoreState[i].Update(TQ_STATE_END_UNBLOCK);
	}
	#ifdef DEBUG_TASKQ
	DEBUG_HEADER();
	cerr << " TQ END'd" << endl;
	#endif

	Profile::global_task_stats.tq_end_count++;
	return TQ_RET_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// TaskSystemBaseline::TQ_Enqueue
////////////////////////////////////////////////////////////////////////////////
// Enqueues a single task onto the queue. A task descriptor is 
// pushed onto the task queue.
//
// Parameters: tdesc - task descriptor which holds the information for
// the task.
//
// Returns: Overflow or Success signals.
////////////////////////////////////////////////////////////////////////////////
TQ_RetType
TaskSystemBaseline::TQ_Enqueue(int CoreNum, TaskDescriptor &tdesc) {
	if (QueueMaxSize == (int)TaskQueue.size()) {
		// Overflow found
	
		OverflowPending = true;
	//	#ifdef DEBUG_TASKQ
		DEBUG_HEADER();
		std::cerr << " Overflow Found: Make sure you are checking retval"
						" and setting the queue size large enough.\n";
	//	#endif
	assert(0 && "OVERFLOW FOUND!  No benchmarks handle TQ_RET_OVERFLOW yet...but they should!");
		Profile::global_task_stats.tq_overflow++;
		return TQ_RET_OVERFLOW;
	}
	
	// Always add task to the front of the queue to preserve locality.
	TaskQueue.push_front(tdesc);
	assert((TaskQueue.size() <= (unsigned int)QueueMaxSize) 
		&& "Trying to add a task to a full queue!");

	#ifdef DEBUG_TASKQ
	DEBUG_HEADER();
	cerr << " Enqueueing task";
	TQ_DUMP_TASK(tdesc);
	#endif

	Profile::global_task_stats.tq_enqueue_count++;
	return TQ_RET_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// TaskSystemBaseline::TQ_EnqueueLoop
////////////////////////////////////////////////////////////////////////////////
// Enqueues some number of tasks onto the queue. A task descriptor is 
// pushed onto the task queue some number of times.
//
// Parameters: tdesc - task descriptor which holds the information for
// the task.
//             N - Number of total iterations in loop
//             S - Stride of iterations handled by each task
//
// Returns: Overflow or Success signals.
////////////////////////////////////////////////////////////////////////////////
TQ_RetType
TaskSystemBaseline::TQ_EnqueueLoop(int CoreNum, TaskDescriptor &tdesc, int N, int S)
{
//	assert(((N % S) == 0) 
//		&& "Number of iterations per tasks in non-integer.  FIXME?");

	unsigned int num_tasks = (N + S - 1)  / S;
	unsigned int i;

	// Check if we overflow the queue if all tasks are inserted
	if ((TaskQueue.size() + num_tasks) > (unsigned int)QueueMaxSize) {
		// Overflow found
		// #ifdef DEBUG_TASKQ
		DEBUG_HEADER();
		std::cerr << " Overflow Found: Make sure you are checking retval"
						" and setting the queue size large enough.\n";
		// #endif
	
		OverflowPending = true;

		Profile::global_task_stats.tq_overflow++;
		return TQ_RET_OVERFLOW;
	}

	// XXX: May want to penalize some number of cycles for this...handle at the
	// core level maybe?
	for (i = 0; i < num_tasks; i++) {
		// ip and data fields are unchanged
		tdesc.begin = i*S;
		tdesc.end = i*S+S;

		// Handle the case where S does not divide N evenly
		if (tdesc.end > (unsigned int)N) tdesc.end = N;

		TQ_Enqueue(CoreNum, tdesc);
	}


	Profile::global_task_stats.tq_loop_count++;
	return TQ_RET_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// TaskSystemBaseline::TQ_EnqueueLoop
////////////////////////////////////////////////////////////////////////////////
// Dequeues a single task from the queue. A task descriptor is 
// pushed onto the task queue some number of times.
//
// Parameters: tdesc - task descriptor which holds the information for
// the task. (returned)
//
// Returns: The Return State (Success, Block, End, Sync)
////////////////////////////////////////////////////////////////////////////////
TQ_RetType 
TaskSystemBaseline::TQ_Dequeue(int CoreNum, TaskDescriptor &tdesc) {
	TQ_RetType RetState = TQ_RET_BLOCK;
	TaskDescriptor pending_tdesc;

	switch (TQ_CoreState[CoreNum].CurrState) {
		case (TQ_STATE_PENDING_DEQUEUE): 
		{
			// If the value is ready, return it to the requesting core
			if (TQ_CoreState[CoreNum].CheckReady()) {
				tdesc = TQ_CoreState[CoreNum].PendingTaskDescriptor;
				TQ_CoreState[CoreNum].Update(TQ_STATE_ACTIVE);
				RetState = TQ_RET_SUCCESS;
				break;
			} else {
				// Otherwise, we wait...
				RetState = TQ_RET_BLOCK;
				Profile::global_task_stats.cycles_blocked++;
				break;
			}
		}
		case (TQ_STATE_ACTIVE):
		case (TQ_STATE_BLOCKING):
			// Remove a task if possible and return it
			if (TaskQueue.empty()) {
				#ifdef DEBUG_TASKQ
				DEBUG_HEADER();
				cerr << "Empty (core # " << dec << CoreNum << ")" << endl;
				#endif

				TQ_CoreState[CoreNum].Update(TQ_STATE_BLOCKING);
				RetState = TQ_RET_BLOCK;
				Profile::global_task_stats.cycles_blocked++;
				break;
			}
			// TODO: Check for underflow

			// Remove element from head of queue, save it for when it is available and
			// update state of this core
			SelectNextTask(CoreNum, pending_tdesc);
			TQ_CoreState[CoreNum].PendingTaskDescriptor = pending_tdesc;

			// Set the latency and the next state
			TQ_CoreState[CoreNum].Update(TQ_STATE_PENDING_DEQUEUE);
			RetState = TQ_RET_BLOCK;
			
			#ifdef DEBUG_TASKQ
			DEBUG_HEADER();
			cerr << " Returning (after possible delay): ";
			TQ_DUMP_TASK(pending_tdesc);
			#endif
			
			Profile::global_task_stats.tq_dequeue_count++;
			break;
		case (TQ_STATE_PENDING_END):
			// Update the state to acknowledge reaching sync point
			TQ_CoreState[CoreNum].Update(TQ_STATE_ACTIVE);
			// Wake up the task, no new task is fetched this cycle.
			RetState = TQ_RET_END;

			#ifdef DEBUG_TASKQ
			DEBUG_HEADER();
			cerr << " TQ_END Found" << endl;
			#endif

			// XXX: Pending count?
			break;
		case (TQ_STATE_INVALID):
			assert(0 && "Invalid TaskQueue state found!");
			break;
		// TQ_STATE_END_UNBLOCK - All tasks have finished
		case (TQ_STATE_PENDING_UNBLOCK):
			#ifdef DEBUG_TASKQ
			DEBUG_HEADER();
			cerr << " Sync point reached (STATE_PENDING_UNBLOCK): ";
			TQ_DUMP_TASK(tdesc);
			#endif

			RetState = TQ_RET_BLOCK;
			// Next cycle we wake up from the barrier
			if (TQ_CoreState[CoreNum].CheckReady()) {
				TQ_CoreState[CoreNum].Update(TQ_STATE_UNBLOCK);
				break;
			}

			// Another cycle waiting in the barrier...
			break;
		case (TQ_STATE_UNBLOCK):
			#ifdef DEBUG_TASKQ
			DEBUG_HEADER();
			cerr << " Sync point delivered (STATE_UNBLOCK): ";
			TQ_DUMP_TASK(tdesc);
			#endif
			
			// Barrier reached, go back to fetching other tasks
			TQ_CoreState[CoreNum].Update(TQ_STATE_ACTIVE);
			// Wakup core
			RetState = TQ_RET_SYNC;
		
			// XXX: Barrier Count?
			break;
		default:
			assert(0 && "Unknown TaskQueue state found!");
	}

	return RetState;
}

////////////////////////////////////////////////////////////////////////////////
// TaskSystemBase::PerCycle
////////////////////////////////////////////////////////////////////////////////
// Clocks the Cores in reference to the Task Queue. Checks that a sync state
// has not yet been hit. 
////////////////////////////////////////////////////////////////////////////////
void
TaskSystemBase::PerCycle() {
	int block_count = 0;

	// After all work has been done this cycle, i.e., all modifications to the
	// NextState values have occurred, we want to clock all of the
	// registers and update the state for the next cycle.
	for (int i = 0; i < rigel::CORES_TOTAL; i++) {
		if (TQ_CoreState[i].CheckIfAtSync()) block_count++;
		TQ_CoreState[i].Clock();
	}

	// If all cores are at TQ_STATE_BLOCKING, then a sync point has been reached
	if (block_count == rigel::CORES_TOTAL) {
		#ifdef DEBUG_TASKQ
		DEBUG_HEADER();
		cerr << " Number of cores blocking: " << dec << block_count;
		cerr << " of " << dec << rigel::CORES_TOTAL<< endl;
		#endif
		// The fact that we change the state when a barrier occurs precludes having
		// one fast core reeenter a pending barrier and cause an error.  We also
		// wait to deliever the barrier until LATENCY_BLOCKED_SYNC has elapsed.
		for (int i = 0; i < rigel::CORES_TOTAL; i++) {
			TQ_CoreState[i].Set(TQ_STATE_PENDING_UNBLOCK);
			TQ_CoreState[i].Update(TQ_STATE_PENDING_UNBLOCK);
		}
		SyncCallback();
	}

}

////////////////////////////////////////////////////////////////////////////////
// AffinityDataInterval::Clear()
////////////////////////////////////////////////////////////////////////////////
// Clears out the data for task tracking 
////////////////////////////////////////////////////////////////////////////////
void
AffinityDataInterval::Clear() {
	v1_match.clear();
	v2_match.clear();
	v3_match.clear();
	v4_match.clear();
	tasks_match.clear();
	range_match.clear();
}

////////////////////////////////////////////////////////////////////////////////
// AffinityDataInterval::InsertTask
////////////////////////////////////////////////////////////////////////////////
// Insert a task into the tracking framework for matching later
// and interval information
//
// Parameters: TaskDescriptor tdesc - task to be inserted and tracked
////////////////////////////////////////////////////////////////////////////////
void
AffinityDataInterval::InsertTask(const TaskDescriptor &tdesc) {
	tasks_match.insert(tdesc);
	v1_match.insert(tdesc.v1);
	v2_match.insert(tdesc.v2);
	v3_match.insert(tdesc.v3);
	v4_match.insert(tdesc.v4);
	range_match.insert(std::pair<uint32_t, uint32_t>(tdesc.v3, tdesc.v4));
}

////////////////////////////////////////////////////////////////////////////////
// AffinityDataInterval::CheckTaskPartial
////////////////////////////////////////////////////////////////////////////////
// Check a task to see if any of the taskdescriptor fields match. Searches
// through all tasks that are being tracked. 
//
// Parameters: TaskDescriptor tdesc - task used as the key
//
// Returns: The number (out of 4) fields that matched.
////////////////////////////////////////////////////////////////////////////////
int
AffinityDataInterval::CheckTaskPartial(const TaskDescriptor &tdesc) {
	int matches = 0;

	if (v1_match.count(tdesc.v1)) matches++;
	if (v2_match.count(tdesc.v2)) matches++;
	if (v3_match.count(tdesc.v3)) matches++;
	if (v4_match.count(tdesc.v4)) matches++;

	return matches;
}

////////////////////////////////////////////////////////////////////////////////
// Stack_SingleMatch()
////////////////////////////////////////////////////////////////////////////////
// Given a stack of task descriptors, a key task descriptor, and a field to 
// match we search until we find a match.
//
// Parameters: list<> task_stack - hold a stack of Task Descriptors
//             TaskDescriptor tdesc - task descriptor to try and match
//             int field - key for matching
// Returns: the location of the first match, otherwise 0
////////////////////////////////////////////////////////////////////////////////
int Stack_SingleMatch(std::list<TaskDescriptor> &task_stack, const TaskDescriptor &tdesc, int field) {
	std::list<TaskDescriptor>::iterator iter;
	iter = task_stack.begin();
	// i tracks how deep we have to go into the list to find a task
	for (int i = task_stack.size(); iter != task_stack.end(); i--) {
		switch (field) {
			case 1: if ((*iter).v1 == tdesc.v1) {return i;} else {break;};
			case 2: if ((*iter).v2 == tdesc.v2) {return i;} else {break;};
			case 3: if ((*iter).v3 == tdesc.v3) {return i;} else {break;};
			case 4: if ((*iter).v4 == tdesc.v4) {return i;} else {break;};
			default: assert(0 && "filed must be in the range [1..4]");
		}
		iter++;
	}
	// Nothing found in task stack
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Stack_NearSingleMatch()
////////////////////////////////////////////////////////////////////////////////
// Given a stack of task descriptors, a key task descriptor, and a field to 
// match we search until we find a match. A match occurs when the value is
// within a range.
//
// Parameters: list<> task_stack - hold a stack of Task Descriptors
//             TaskDescriptor tdesc - task descriptor to try and match
//             int field - key for matching
//             int range - the range that the field can differ by and
//               still be a near match
// Returns: Location of the first match, otherwise 0
////////////////////////////////////////////////////////////////////////////////
int Stack_NearSingleMatch(std::list<TaskDescriptor> &task_stack, 
	const TaskDescriptor &tdesc, int field, int range) 
{
	std::list<TaskDescriptor>::iterator iter;
	iter = task_stack.begin();
	// i tracks how deep we have to go into the list to find a task
	for (int i = task_stack.size(); iter != task_stack.end(); i--) {
		uint32_t stack_val, tdesc_val;
		switch (field) {
			case 1: 
				stack_val = (*iter).v1;
				tdesc_val = tdesc.v1;
				break;
			case 2: 
				stack_val = (*iter).v2;
				tdesc_val = tdesc.v2;
				break;
			case 3: 
				stack_val = (*iter).v3;
				tdesc_val = tdesc.v3;
				break;
			case 4: 
				stack_val = (*iter).v4;
				tdesc_val = tdesc.v4;
				break;
			default: assert(0 && "field must be in the range [1..4]");
		}
		if ( 	(stack_val >= (tdesc_val - range)) && 
					(stack_val < (tdesc_val + range))) { return i; }
		iter++;
	}
	// Nothing found in task stack
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Stack_FullMatch()
////////////////////////////////////////////////////////////////////////////////
// Given a stack of task descriptors, a key task descriptor to match we 
// search until we find an exact match. In order to be a match, all the fields
// in the task descriptors being compared must match.
//
// Parameters: list<> task_stack - hold a stack of Task Descriptors
//             TaskDescriptor tdesc - task descriptor to try and match
//
// Returns: location in stack of match, otherwise 0
////////////////////////////////////////////////////////////////////////////////
int Stack_FullMatch(std::list<TaskDescriptor> &task_stack, const TaskDescriptor &tdesc) {
	std::list<TaskDescriptor>::iterator iter;
	iter = task_stack.begin();
	// i tracks how deep we have to go into the list to find a task
	for (int i = task_stack.size(); iter != task_stack.end(); i--) {
		if (*iter == tdesc) {
			return i;
		}
		iter++;
	}
	// Nothing found in task stack
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// AffinityDataInterval_LIFO::CheckTaskStack_FullMatch
////////////////////////////////////////////////////////////////////////////////
// Interface function to perform a Full match of a task descriptor
//
// Parameters: TaskDescriptor tdesc - task descriptor to try and match
//
// Returns: location in stack of match, otherwise 0
////////////////////////////////////////////////////////////////////////////////
int
AffinityDataInterval_LIFO::CheckTaskStack_FullMatch(const TaskDescriptor &tdesc) {
	return Stack_FullMatch(task_stack, tdesc);
}

////////////////////////////////////////////////////////////////////////////////
// Stack_RangeMatch()
////////////////////////////////////////////////////////////////////////////////
// Given a stack of task descriptors, a key task descriptor, we search the 
// stack for a match using the third and fourth task descriptor fields as
// a key. 
//
// Parameters: list<> task_stack - hold a stack of Task Descriptors
//             TaskDescriptor tdesc - task descriptor to try and match
//
// Returns: Location of the first match, otherwise 0
////////////////////////////////////////////////////////////////////////////////
int Stack_RangeMatch(std::list<TaskDescriptor> &task_stack, const TaskDescriptor &tdesc) {
	std::list<TaskDescriptor>::iterator iter;
	iter = task_stack.begin();
#ifdef TQ_TRACK_SCHED
#if 0
	fprintf(stderr, "0x%08llx: TQ_SCHED_RANGEMATCH Top: "
									"{0x%08x, 0x%08x, 0x%08x, 0x%08x} ",
									rigel::CURR_CYCLE,
									tdesc.v1, tdesc.v2, tdesc.v3, tdesc.v4
					);
#endif
#endif
	// Does the range match? FIXME FIXME FIXME FIXME 
	for (int i = task_stack.size(); iter != task_stack.end(); i--) {
		if (((*iter).v3 == tdesc.v3) && ((*iter).v4 == tdesc.v4)) {
#ifdef TQ_TRACK_SCHED
#if 0
	fprintf(stderr, " Found: %4d\n", i);
#endif
#endif
			return i;
		}
		iter++;
	}
	// Nothing found in task stack
#ifdef TQ_TRACK_SCHED
#if 0
	fprintf(stderr, " None Found\n");
#endif
#endif
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Stack_NearRangeMatch()
////////////////////////////////////////////////////////////////////////////////
// Given a stack of task descriptors, a key task descriptor, we search the 
// stack for a match using the third and fourth task descriptor fields as
// a key. The function finds a match when those fields are in range.
//
// Parameters: list<> task_stack - hold a stack of Task Descriptors
//             TaskDescriptor tdesc - task descriptor to try and match
//             int range - difference in the 3rd and 4th fields which signals
//               a match
//
// Returns: Location of the first match, otherwise 0
////////////////////////////////////////////////////////////////////////////////
int Stack_NearRangeMatch(std::list<TaskDescriptor> &task_stack, 
	const TaskDescriptor &tdesc, int range) 
{
  std::list<TaskDescriptor>::iterator iter;
	iter = task_stack.begin();
	// How large one iteration chunk is
	uint32_t span = tdesc.v4 - tdesc.v3;
	// Limits we are willing to accept
	uint32_t start = tdesc.v3 - (range * span);
	uint32_t end = tdesc.v4 + (range * span);
	if (start > end) start = 0;
	
	for (int i = task_stack.size(); iter != task_stack.end(); i--) {
		if ( ((*iter).v3 >= start) && ((*iter).v4 < end )) {
			return i;
		}
		iter++;
	}
	// Nothing found in task stack
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// AffinityDataInterval_LIFO::CheckTaskStack_RangeMatch()
////////////////////////////////////////////////////////////////////////////////
// A wrapper for calling a range match (for third and fourth) 
//
// Parameters: TaskDescriptor tdesc - task descriptor to try and match
//
// Returns: Location of the first match, otherwise 0
////////////////////////////////////////////////////////////////////////////////
int
AffinityDataInterval_LIFO::CheckTaskStack_RangeMatch(const TaskDescriptor &tdesc) {
	return Stack_RangeMatch(task_stack, tdesc);
}

////////////////////////////////////////////////////////////////////////////////
// Stack_InsertTask()
////////////////////////////////////////////////////////////////////////////////
// Inserts a task descriptor onto a stack. If the stack is full, it removes 
// oldest one and places the new one on the top.
//
// Parameters: list<> task_stack - hold a stack of Task Descriptors
//             TaskDescriptor tdesc - task descriptor to insert
//             int TaskStackMaxSize - Size of the stack
//             int InsertedTaskCount - how many tasks are on the stack already
//
////////////////////////////////////////////////////////////////////////////////
void 
Stack_InsertTask(std::list<TaskDescriptor> &task_stack, 
	const TaskDescriptor &tdesc, 
	int TaskStackMaxSize, 
	int InsertedTaskCount) 
{
	// We keep a bounded number of tasks.  Remove from front, add to back.
	// Tasks at the rear are "warmest" while tasks at the front of the list
	// are "coldest" in the cache.
	if (InsertedTaskCount == TaskStackMaxSize) {
		task_stack.pop_back();
	}
	task_stack.push_front(tdesc);
}

////////////////////////////////////////////////////////////////////////////////
// AffinityDataInterval_LIFO::InsertTask()
////////////////////////////////////////////////////////////////////////////////
// Wrapper for Stack_InsertTask. 
//
// Parameters: TaskDescriptor tdesc - task descriptor to insert
//
////////////////////////////////////////////////////////////////////////////////
void
AffinityDataInterval_LIFO::InsertTask(const TaskDescriptor &tdesc) {
	Stack_InsertTask(task_stack, tdesc, TaskStackMaxSize, InsertedTaskCount() );
}

////////////////////////////////////////////////////////////////////////////////
// TaskSystemIntervalLIFO::SelectNextTask
////////////////////////////////////////////////////////////////////////////////
// Selects the next task. Schedules appropriately and removes and task from 
// the stack and returns it to a core. 
//
// Parameters: TaskDescriptor tdesc - task descriptor
//             int CoreNum - the requesting core
//
////////////////////////////////////////////////////////////////////////////////
void 
TaskSystemIntervalLIFO::SelectNextTask(int CoreNum, TaskDescriptor &tdesc) {
	std::deque<TaskDescriptor>::iterator task_iter, last_task, ret_iter;
	task_iter = TaskSystemBaseline::TaskQueue.begin();
	int cluster_num = CoreInOrderLegacy::ClusterNum(CoreNum);
	int affinity = 0, max_affinity = 0;
	bool sched_with_affinity = false;		// Track whether affinity was used
	int index = 0;

	// NOTE: Into this function we have the assumption that the TQ is not empty
	assert(task_iter != TaskSystemBaseline::TaskQueue.end() 
		&& "Attempting to call SelectNextTask with an empty TQ. TQ must not be empty!");

	// Default is to take the first task
	ret_iter = task_iter;
	while (task_iter != TaskSystemBaseline::TaskQueue.end()) {
		// Advance to next task, save last_task for removal 
		last_task = task_iter++;


		// Unlike the simple Interval scheduler, we try to maximize the affinity of
		// the task  TODO: We should limit the number of tasks we iterate through
		// since this may take a cycle per each.
		affinity = ClusterInfo[cluster_num].GetAffinity(*last_task, 1);
		
		if (affinity > max_affinity || (index >= CoreNum && max_affinity == 0)) {
			max_affinity = affinity;
			ret_iter = last_task;
			sched_with_affinity = true;
		}

		index++;
		// TODO: Add threshold back in. Found a task we like; Exit.
	}
	// Update profiler stats
	if (sched_with_affinity) { 
		Profile::global_task_stats.sched_with_cache_affinity++; 
	} else { 
		Profile::global_task_stats.sched_without_cache_affinity++; 
	}

	tdesc = *ret_iter;

	ClusterInfo[CoreInOrderLegacy::ClusterNum(CoreNum)].InsertTask(tdesc);
	// Remove the task we are returning from the queue.  
	TaskSystemBaseline::TaskQueue.erase(ret_iter);
}


////////////////////////////////////////////////////////////////////////////////
// TaskSystemInterval::SelectNextTask
////////////////////////////////////////////////////////////////////////////////
// Selects the next task. Schedules appropriately and removes and task from 
// the queue and returns it to a core. 
//
// Parameters: TaskDescriptor tdesc - task descriptor
//             int CoreNum - the requesting core
//
////////////////////////////////////////////////////////////////////////////////
void
TaskSystemInterval::SelectNextTask(int CoreNum, TaskDescriptor &tdesc) {
	std::deque<TaskDescriptor>::iterator task_iter, last_task;
	task_iter = TaskSystemBaseline::TaskQueue.begin();
	int cluster_num = CoreInOrderLegacy::ClusterNum(CoreNum);
	TaskDescriptor ret_tdesc;
	int affinity = 0;

	// NOTE: Into this function we have the assumption that the TQ is not empty
	assert(task_iter != TaskSystemBaseline::TaskQueue.end() 
		&& "Attempting to call SelectNextTask with an empty TQ. TQ must not be empty!");

	while (task_iter != TaskSystemBaseline::TaskQueue.end()) {
		ret_tdesc = *task_iter;

		// Advance to next task, save last_task for removal 
		last_task = task_iter++;

		// Currently we use the hueristic that if this same task was on this cluster
		// last time, we use it.  
		affinity = this->ClusterInfo[cluster_num].GetAffinity(ret_tdesc, 1);

		// Found a task we like; Exit.
		if (affinity > 0) break;
	}
	if (affinity > 0) { Profile::global_task_stats.sched_with_cache_affinity++; }
	else 							{ Profile::global_task_stats.sched_without_cache_affinity++; }
	// Remove the task we are returning from the queue.  last_task ranges from
	// [begin, end)
	TaskSystemBaseline::TaskQueue.erase(last_task);
	
	tdesc = ret_tdesc;
}
// TODO: For performance reasons, we should probably start doing the LIFO's from
// the end and not the beginning sice the threshold starts at the end, not at
// the beginning
template <int CHECK_TYPE>
void
TaskSystemRecent<CHECK_TYPE>::SelectNextTask(int CoreNum, TaskDescriptor &tdesc) {
	std::deque<TaskDescriptor>::iterator task_iter, last_task, ret_iter;
	task_iter = TaskSystemBaseline::TaskQueue.begin();
	int cluster_num = CoreInOrderLegacy::ClusterNum(CoreNum);
	int affinity = 0, max_affinity = 0;
	int index = 0;							// Keep count of depth into list current iter is
	bool sched_with_affinity = false;		// Track whether affinity was used

	// NOTE: Into this function we have the assumption that the TQ is not empty
	assert(task_iter != TaskSystemBaseline::TaskQueue.end() 
		&& "Attempting to call SelectNextTask with an empty TQ. TQ must not be empty!");

	// Default is to take the first task
	ret_iter = task_iter;
	while (task_iter != TaskSystemBaseline::TaskQueue.end()) {
		// Advance to next task, save last_task for removal 
		last_task = task_iter++;

		// Unlike the simple Interval scheduler, we try to maximize the affinity of
		// the task  TODO: We should limit the number of tasks we iterate through
		// since this may take a cycle per each.
		affinity = this->ClusterInfo[cluster_num].GetAffinity(*last_task, 1);
		// Get the task with the highest affinity first, but if none are
		// available, choose one farther into the task to avoid thrashing
		if (affinity > max_affinity || (index >= CoreNum && max_affinity == 0)) {
			max_affinity = affinity;
			ret_iter = last_task;
			sched_with_affinity = true;
		}
		// Track how far into the list we are
		index++;
		// TODO: Add threshold back in. Found a task we like; Exit.
	}
	// Update profiler stats
	if (sched_with_affinity) { 
		Profile::global_task_stats.sched_with_cache_affinity++; 
	} else { 
		Profile::global_task_stats.sched_without_cache_affinity++; 
	}

	// Remove the task we are returning from the queue.  
	tdesc = *ret_iter;
	#ifdef TQ_TRACK_SCHED
	fprintf(stderr, "0x%08llx: TQ_SCHED_SELECT (CR: %4d CL: %3d) tdesc: "
									"{0x%08x, 0x%08x, 0x%08x, 0x%08x} Aff.: %4d "
									"TQsz: %5d RECsz.: %4d\n",
									rigel::CURR_CYCLE,
									CoreNum, cluster_num, tdesc.v1, tdesc.v2, tdesc.v3, tdesc.v4,
									max_affinity,TaskSystemBaseline::TaskQueue.size(),
									ClusterInfo[cluster_num].CurrInterval.InsertedTaskCount()
					);
	#endif
	ClusterInfo[cluster_num].InsertTask(tdesc);
	TaskSystemBaseline::TaskQueue.erase(ret_iter);
}
	

