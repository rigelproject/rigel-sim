////////////////////////////////////////////////////////////////////////////////
// task_queue_recent.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the definition of a scheduler discipline for the RTM
//  hardware TQ implementation.
//
////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <list>
#include <string>
#include "util/task_queue.h"
#include "RandomLib/Random.hpp"

namespace rigel {
  extern RandomLib::Random RNG;
}

template <int CHECK_TYPE>
struct AffinityDataInterval_Recent : public AffinityDataInterface {
	public:
		AffinityDataInterval_Recent () : 
			TaskStackMaxSize(rigel::task_queue::INTERVAL_RECENT_STACK_SIZE) 
		{ }

		// Clear out all elements from the task stack
		virtual void Clear() { task_stack.clear(); }
		// Returns the number of elements currently in the task stack
		virtual int InsertedTaskCount() { return task_stack.size(); }
		// Insert a task at the rear of the stack (list)
		virtual void InsertTask(const TaskDescriptor &tdesc) {
			Stack_InsertTask(task_stack, tdesc, TaskStackMaxSize, InsertedTaskCount() );
		}
		// Do a full check against our stack
		virtual int CheckTask(const TaskDescriptor &tdesc) { 
			// FULL MATCH
			if (CHECK_TYPE == 0)
				return this->CheckTaskStack_RangeMatch(tdesc); 
			// PER-FIELD MATCH
			if (CHECK_TYPE == 1)
				return this->CheckTaskStack_SingleMatch(tdesc, 1); 
			if (CHECK_TYPE == 2)
				return this->CheckTaskStack_SingleMatch(tdesc, 2); 
			if (CHECK_TYPE == 3)
				return this->CheckTaskStack_SingleMatch(tdesc, 3); 
			if (CHECK_TYPE == 4)
				return this->CheckTaskStack_SingleMatch(tdesc, 4); 
			// NEAR on v1, v2 and exact on v3,v4
			if (CHECK_TYPE == 50) {
				int range = this->CheckTaskStack_RangeMatch(tdesc); 
				int near1 =  this->CheckTaskStack_NearSingleMatch(tdesc, 1, 8); 
				int near2 =  this->CheckTaskStack_NearSingleMatch(tdesc, 1, 8); 
				return (range + near1 + near2);
			}
			// NEAR on v1, v2 and Near on v3,v4
			if (CHECK_TYPE == 51) {
				int range = this->CheckTaskStack_NearRangeMatch(tdesc); 
				int near1 =  this->CheckTaskStack_NearSingleMatch(tdesc, 1, 8); 
				int near2 =  this->CheckTaskStack_NearSingleMatch(tdesc, 1, 8); 
				return (range + near1 + near2);
			}
			if (CHECK_TYPE == 52) {
        //FIXME is this really what we want to do?
				return rigel::RNG.Integer();
			}
			if (CHECK_TYPE == 53) {
				int range = this->CheckTaskStack_NearRangeMatch(tdesc);
				int v1 = this->CheckTaskStack_SingleMatch(tdesc, 1); 
				return (v1 && range);
			}
			return this->CheckTaskStack_RangeMatch(tdesc); 
		}
		virtual ~AffinityDataInterval_Recent() {}

		// Stack gives LIFO ordering on tasks.  Insertions happen at dequeue along
		// with removals at the end to keep the size bounded.  Insertions occur at
		// the end and values drop off at the beginning.
		std::list<TaskDescriptor> task_stack;

		// Maximum size of task stack.  Removals happen at insertion time (in
		// necessary)
		int TaskStackMaxSize;

		// Return the depth in the stack of a matching task  Larger number means
		// better affinity (more recently used)
		int CheckTaskStack_FullMatch(const TaskDescriptor &tdesc) {
			return Stack_FullMatch(task_stack, tdesc);
		}
		// Return true if [begin,end) match {v3, v4}
		int CheckTaskStack_RangeMatch(const TaskDescriptor &tdesc) {
			return Stack_RangeMatch(task_stack, tdesc);
		}
		// Only check on one field
		int CheckTaskStack_SingleMatch(const TaskDescriptor &tdesc, int field = 2) {
			return Stack_SingleMatch(task_stack, tdesc, field);
		}
		// Only check on one field and over +/- range values
		int CheckTaskStack_NearSingleMatch(const TaskDescriptor &tdesc, int field, int range) {
			return Stack_NearSingleMatch(task_stack, tdesc, field, range);
		}
		// Return true if [begin,end) match {v3, v4} +/- (range * tdesc->end -
		// tdesc->begin)
		int CheckTaskStack_NearRangeMatch(const TaskDescriptor &tdesc, int range = 4) {
			return Stack_NearRangeMatch(task_stack, tdesc, range);
		}
};

template <int CHECK_TYPE>
class TaskSystemRecent: public TaskSystemBaseline {
	public: 
		TaskSystemRecent() : 
			ClusterInfo(rigel::NUM_CLUSTERS)
		{
			TaskSystemBaseline::TaskSystemBaseline();
		}
		virtual ~TaskSystemRecent() { }
		virtual std::string GetQueueType() { 
			std::string str("TQ_SYSTEM_RECENT"); 
			char num[8];
			sprintf(num, "%02d", CHECK_TYPE);
			return (str+num);
		}

		// Handle returning a task to the dequeuer.  It is called from TQ_Dequeue,
		// but the child version is used if it is the actual dequeuer.
		virtual void SelectNextTask(int CoreNum, TaskDescriptor &tdesc);

		// Hook the defaults to track affinity
		virtual TQ_RetType TQ_Enqueue(int CoreNum, TaskDescriptor &tdesc) {
			#ifdef TQ_TRACK_SCHED
			int cluster_num = CoreBase::ClusterNum(CoreNum);
			fprintf(stderr, "0x%08llx: TQ_SCHED_INSERT (CR: %4d CL: %3d) tdesc: "
											"{0x%08x, 0x%08x, 0x%08x, 0x%08x} TQsz: %5d \n",
											rigel::CURR_CYCLE,
											CoreNum, cluster_num, tdesc.v1, tdesc.v2, tdesc.v3, tdesc.v4,
											TaskSystemBaseline::TaskQueue.size() 
							);
			#endif
			return TaskSystemBaseline::TQ_Enqueue(CoreNum, tdesc);
		}

		virtual TQ_RetType TQ_Dequeue(int CoreNum, TaskDescriptor &tdesc) {
			return TaskSystemBaseline::TQ_Dequeue(CoreNum, tdesc);
		}

		// TQ found a Sync, we need to update the interval for all of the tasks
		virtual void SyncCallback() { 
			for (int i = 0; i < rigel::NUM_CLUSTERS; i++) {
				ClusterInfo[i].NextInterval();
			}
		}

		std::vector<TQ_ClusterAffinityInfoCurrent<AffinityDataInterval_Recent<CHECK_TYPE> > > ClusterInfo;
		// If we find a task that is this close to us, we stop searching.  Should
		// always be set to a number smaller than TaskStackMaxSize
		//const int RecentSelectionThreshold;
};


typedef TaskSystemRecent<0> TaskSystemRecentFull;
typedef TaskSystemRecent<1> TaskSystemRecentV1;
typedef TaskSystemRecent<2> TaskSystemRecentV2;
typedef TaskSystemRecent<3> TaskSystemRecentV3;
typedef TaskSystemRecent<4> TaskSystemRecentV4;

typedef TaskSystemRecent<50> TaskSystemNearRange;
typedef TaskSystemRecent<51> TaskSystemNearAll;
typedef TaskSystemRecent<52> TaskSystemRand;
typedef TaskSystemRecent<53> TaskSystemTypeMatchRange;

		
