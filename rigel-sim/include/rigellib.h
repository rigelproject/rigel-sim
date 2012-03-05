#ifndef __RIGELLIB_H__ 
#define __RIGELLIB_H__

// Defines for global parameters. Is used in other parts of the 
// toolchain, including API and Libraries

// All structures that we cannot safely dynamically allocate after
// discovering the number of clusters at runtime (mostly in TQ_Init) we have to
// use this maximum as a catch-all.  It wastes space and it is a kludge, but it
// is fast and works.
#define __MAX_NUM_CLUSTERS 256
#define __NUM_CORES_PER_CLUSTER 8
#define __NUM_CLUSTERS_PER_TILE 16
#define __MAX_THREADS_PER_CORE 4
#define __MAX_NUM_CORES (__MAX_NUM_CLUSTERS*__NUM_CORES_PER_CLUSTER)
#define __MAX_NUM_THREADS (__MAX_NUM_CORES*__MAX_THREADS_PER_CORE)
#define __MAX_NUM_TILES (__MAX_NUM_CLUSTERS/__NUM_CLUSTERS_PER_TILE)
//DANGER WILL ROBINSON: Use 8 until bugzilla bug against RTM is fixed
#define __DEFAULT_TASK_GROUP_SIZE 8
//#define __DEFAULT_TASK_GROUP_SIZE __NUM_CORES_PER_CLUSTER
#define __DEFAULT_GLOBAL_TQ_SIZE (1<<18)
#define __DEFAULT_TILE_TQ_SIZE (1<<18)
#define __CACHE_LINE_SIZE_BYTES 32
#define __CACHE_LINE_SIZE_WORDS 8

 ///////////////////////////////////////////////////////////////////
 // Typedef event_track_t
 ///////////////////////////////////////////////////////////////////
 // Enumerated type used for keeping track of the core state 
 // in the simulator
 ///////////////////////////////////////////////////////////////////
 // TODO TODO TODO Use these enum values throughout our software rather than
 // hardcoding them (I'm looking at you, rigel.h)
 typedef enum {
   EVT_RTM_TASK_START = 1,        //1
   EVT_RTM_TASK_END,              //2
   EVT_RTM_TASK_RESET,            //3
   EVT_RTM_DEQUEUE_START,         //4
   EVT_RTM_DEQUEUE_END,           //5
   EVT_RTM_DEQUEUE_RESET,         //6
   EVT_RTM_ENQUEUE_START,         //7
   EVT_RTM_ENQUEUE_END,           //8
   EVT_RTM_ENQUEUE_RESET,         //9
   EVT_RTM_ENQUEUE_GROUP_START,   //10
   EVT_RTM_ENQUEUE_GROUP_END,     //11
   EVT_RTM_ENQUEUE_GROUP_RESET,   //12
   EVT_RTM_BARRIER_START,         //13
   EVT_RTM_BARRIER_END,           //14
   EVT_RTM_BARRIER_RESET,         //15
   EVT_RTM_IDLE_START,            //16
   EVT_RTM_IDLE_END,              //17
   EVT_RTM_IDLE_RESET,            //18
   EVT_RF_DUMP,                   //19
   EVT_RF_LOAD,                   //20
   EVT_RCUDA_KERNEL_LAUNCH_START, //21
   EVT_RCUDA_KERNEL_LAUNCH_END,   //22
   EVT_RCUDA_BLOCK_FETCH_START,   //23
   EVT_RCUDA_BLOCK_FETCH_END,     //24
   EVT_RCUDA_KERNEL_START,        //25
   EVT_RCUDA_KERNEL_END,          //26
   EVT_RCUDA_BARRIER_START,       //27
   EVT_RCUDA_BARRIER_END,         //28
   EVT_RCUDA_THREAD_ID_START,     //29
   EVT_RCUDA_THREAD_ID_END,       //30
   EVT_RCUDA_SYNC_THREADS_START,  //31
   EVT_RCUDA_SYNC_THREADS_END,    //32
   EVT_RCUDA_INIT_START,          //33
   EVT_RCUDA_INIT_END,            //34
   EVT_MEM_DUMP,                  //35
   EVT_EVENT_COUNT                //36
 } event_track_t;

 ///////////////////////////////////////////////////////////////////
 // Typedef sprf_names_t
 ///////////////////////////////////////////////////////////////////
 // Enumerated type used for accessing the Special purpose 
 // register file. Add here in order to add more SPRegs 
 // in the simulator
 // NOTE: be sure to update sprf_strings[] when modifying this enum!
 ///////////////////////////////////////////////////////////////////
 typedef enum {
    // Unique core number for the chip [0..N-1]
    SPRF_CORE_NUM = 0,
    // Total number of cores on the chip (N).
    SPRF_CORES_TOTAL = 1,
    // Number of cores in a cluster, nominally 8.  TODO: This may become
    // threads, but we may also want to not overload it and leave it as
    // cores/cluster.
    SPRF_CORES_PER_CLUSTER = 2,
    // Total number of threads per cluster
    SPRF_THREADS_PER_CLUSTER = 3,
//    SPRF_UNDEF_3 = 3,
    // Unique cluster number for the chip [0..N/(cores per cluster)-1].
    SPRF_CLUSTER_ID = 4,
    // Total number of clusters.
    SPRF_NUM_CLUSTERS = 5,
    // Total number of SPRF's.
    // Not used yet.
    SPRF_THREAD_NUM = 6,
    SPRF_THREADS_TOTAL = 7,
    SPRF_THREADS_PER_CORE = 8,
    SPRF_SLEEP = 9,
    SPRF_CURR_CYCLE_LOW = 10,
    SPRF_CURR_CYCLE_HIGH = 11,
    SPRF_ENABLE_INCOHERENT_MALLOC = 12,
    // Startup code allocates a region of the address space to use for the
    // hybrid coherence fine-grain region table.
    SPRF_HYBRIDCC_BASE_ADDR = 13,
    // Enable/disable non-blocking atomics for the thread.  If enabled, only the
    // CCache must take the request before proceeding, no need to wait for a
    // response.
    SPRF_NONBLOCKING_ATOMICS = 14, 
    // Set the minimum stack address.  Only the minimum value is kept so it can
    // be called multiple times.  It is called from crt0.S.
    SPRF_STACK_BASE_ADDR = 15,
		SPRF_IS_SIM = 16,
		SPRF_ARGC = 17,
		SPRF_ARGV = 18,
    SPRF_NUM_SREGS
 } sprf_names_t;
#endif
