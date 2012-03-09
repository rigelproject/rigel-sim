////////////////////////////////////////////////////////////////////////////////
// cluster.h
////////////////////////////////////////////////////////////////////////////////
//
// definition for the cluster class
//
// cluster is a container for cores
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _CLUSTER_LEGACY_H_
#define _CLUSTER_LEGACY_H_
#include "sim.h"
#include "clusterbase.h"
#include <cassert>
#include <bitset>

class GlobalCache;
class CacheModel;
class TileInterconnectBase;
class BroadcastManager;
class Profile;
class CoreInOrderLegacy;
class UserInterfaceLegacy;
namespace rigel {
  class ConstructionPayload;
}

////////////////////////////////////////////////////////////////////////////////
// Class: ClusterLegacy
////////////////////////////////////////////////////////////////////////////////
class ClusterLegacy : public ClusterBase {

  //////////////////////////////////////////////////////////////////////////////
  // public methods
  //////////////////////////////////////////////////////////////////////////////
  public:

    /// constructors and destructors
    /// standard constructor
    ClusterLegacy(rigel::ConstructionPayload cp);

    /// empty destructor
    ~ClusterLegacy();

    int      PerCycle();                /// steps the cores and caches forward one cycle
    void     ClockClusterCache();       /// 
    void     Dump();
    int      halted();                  /// Returns 1 if halted, 0 otherwise
    void     Heartbeat();               /// print heartbeat status message
    void     EndSim();                  /// called at termination of simulation for wrapup
    void     ProfileCycles(int i);      /// 
    void     ProfileStuckBehind(int i); /// 
    uint32_t GetCorePC(int c);          ///
    void     SleepCore(int i);          /// 
    void     WakeCore(int i);           /// 

    // simple accessor implementations
    rigel::GlobalBackingStoreType * getBackingStore() { return backing_store; }
    CoreInOrderLegacy &GetCore(int c) const             { return *cores[c];    }
    int GetClusterID()                          { return id_num;       }

    //dump C$, per-core, and per-thread locality info
    void dump_locality();

    //HACK we only have these functions because cores are private.  There should be a better interface.
    std::string getTLBConfigStream(size_t i) const;
    size_t getNumCoreTLBs() const;

    void get_total_tlb_hits_misses(size_t tlb_num, uint64_t &ptiHits, uint64_t &ptiMisses,
                                   uint64_t &ptdHits, uint64_t &ptdMisses,
                                   uint64_t &ptuHits, uint64_t &ptuMisses,
                                   uint64_t &pciHits, uint64_t &pciMisses,
                                   uint64_t &pcdHits, uint64_t &pcdMisses,
                                   uint64_t &pcuHits, uint64_t &pcuMisses) const;

    //dump C$, per-core, and per-thread TLB info
    void dump_tlb();

    // fast barrier access methods
    int cluster_sync(int core); // add core/thread to given barrier
    int get_sync_status(int core);            // returns

    // accessors
    CacheModel*    getCacheModel()    { return caches;   }
    UserInterfaceLegacy* getUserInterface() { return ui;       }
    //Profile*       getProfiler()      { return profiler; }

    virtual void save_state() const;
    virtual void restore_state();

  //////////////////////////////////////////////////////////////////////////////
  // public data
  //////////////////////////////////////////////////////////////////////////////
  public:
    //Profile               *profiler; //FIXME should be protected after we modularize
  //////////////////////////////////////////////////////////////////////////////
  // protected data
  //////////////////////////////////////////////////////////////////////////////
  protected:
    CacheModel            *caches;
    //TileInterconnectBase  *TileInterconnect;
    UserInterfaceLegacy         *ui; // made un-private so core can touch this

  //////////////////////////////////////////////////////////////////////////////
  // private methods
  //////////////////////////////////////////////////////////////////////////////
  private:

  //////////////////////////////////////////////////////////////////////////////
  // private data
  //////////////////////////////////////////////////////////////////////////////
  private:
    CoreInOrderLegacy ** cores;
    int id_num; // Cluster ID number assigned as constructor "id" input
	std::bitset<rigel::CORES_PER_CLUSTER> core_sleeping;
    //Used for pseudo-MT to track which cores should be clocked next cycle.
    unsigned int *which_cores_to_clock;
    //Use this to make sure we switch threads at least once every 200 cycles or so.
    unsigned int *cycles_clocked;
    // fast barrier values
    bool enter_barrier[rigel::CORES_PER_CLUSTER];
    bool exit_barrier[rigel::CORES_PER_CLUSTER];
    //int  barrier_timeout = 1000; // maximum number of cycles to wait at barrier before aborting
    enum BARRIER_STATUS { WAIT, READY } barrier_status;

};

 #endif
