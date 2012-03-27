////////////////////////////////////////////////////////////////////////////////
// cluster.h
////////////////////////////////////////////////////////////////////////////////
//
// definition for the cluster base class
//
// cluster is a container for cores
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __CLUSTER_BASE_H__
#define __CLUSTER_BASE_H__

#include "sim.h"
#include "sim/componentbase.h"
#include "util/util.h"

extern ComponentCount ClusterBaseCount;

// forward declarations
class TileInterconnectBase;
class BroadcastManager;
class Profile;

class Packet;
template<class T> class InPortBase;
template<class T> class OutPortBase;

#include "util/construction_payload.h"
#include "util/checkpointable.h"

////////////////////////////////////////////////////////////////////////////////
// Class: Cluster
////////////////////////////////////////////////////////////////////////////////
class ClusterBase : public ComponentBase, public rigelsim::Checkpointable {

  //////////////////////////////////////////////////////////////////////////////
  // public methods
  //////////////////////////////////////////////////////////////////////////////
  public:

    /// constructors and destructors
    
    /// standard constructor
    ClusterBase(rigel::ConstructionPayload &cp);
    
    virtual      ~ClusterBase()  = 0;
    virtual void  Heartbeat()    = 0;      /// heartbeat
    virtual int   PerCycle()     = 0;      /// steps the cores and caches forward one cycle
    virtual void  EndSim()       = 0;      /// called at the end of simulation for wrapping up
    virtual int   halted()       = 0;      /// Returns 1 if halted, 0 otherwise
    virtual void save_state() const = 0;
    virtual void restore_state() = 0;

    // FIXME: replace with generic port connections
    virtual InPortBase<Packet*>* get_inport() { throw ExitSim("illegal in this class"); return NULL; }
    virtual OutPortBase<Packet*>* get_outport() { throw ExitSim("illegal in this class"); return NULL; }

    /// simple accessor implementations
    rigel::GlobalBackingStoreType * getBackingStore() { return backing_store; }
    int       GetClusterID()   { return _cluster_id;   }

    //CacheModel*    getCacheModel()    { return caches;   }
    Profile*  getProfiler()      { return profiler; }

  //////////////////////////////////////////////////////////////////////////////
  // public data
  //////////////////////////////////////////////////////////////////////////////
  public:

  //////////////////////////////////////////////////////////////////////////////
  // protected data
  //////////////////////////////////////////////////////////////////////////////
  protected:
    Profile               *profiler; // remove from base?
    TileInterconnectBase  *TileInterconnect; // remove from base?
    rigel::GlobalBackingStoreType *backing_store; // remove from base?
    int _cluster_id; // Cluster ID number assigned as constructor "id" input
    unsigned int *which_cores_to_clock;
    rigelsim::ClusterState *cluster_state;

  //////////////////////////////////////////////////////////////////////////////
  // private methods
  //////////////////////////////////////////////////////////////////////////////
  private:

  //////////////////////////////////////////////////////////////////////////////
  // private data
  //////////////////////////////////////////////////////////////////////////////
  private:


};

 #endif
