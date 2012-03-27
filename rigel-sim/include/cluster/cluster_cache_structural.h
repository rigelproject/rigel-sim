#ifndef __CLUSTER_CACHE_STRUCTURAL_H__
#define __CLUSTER_CACHE_STRUCTURAL_H__

#include "cluster/cluster_cache_base.h"
#include "util/util.h"
#include <utility>
#include "util/fifo.h"
#include "port/port.h"

// forward declarations
template<class T> class InPortBase;
template<class T> class OutPortBase;
class Packet;

namespace rigel {
	class ConstructionPayload;
	class ComponentCount;
}


class ClusterCacheStructural : public ClusterCacheBase {

  public:

    /// constructor
    ClusterCacheStructural(
      rigel::ConstructionPayload cp,
      InPortBase<Packet*>* in,       ///< unused, the functional cache don't care!
      OutPortBase<Packet*>* out      ///< unused, the functional cache don't care!
    );

    /// component interface functions
    int  PerCycle();
    void EndSim();
    void Dump();
    void Heartbeat();

  protected:

    std::vector<clustercache::LDLSTC_entry_t> LinkTable;

  private: // we don't want these visible to inherited classes

    void doMemoryAccess(Packet* p);
    void doLocalAtomic(Packet* p);
    void doGlobalAtomic(Packet* p);

    void handleCoreSideInputs();
    void handleMemsideInputs();

    void handleCoreSideRequests();
    void handleCoreSideReplies();

    void handleCCacheRequests();
    void handleCCacheReplies();

    void memsideBypass();

    void handleMemsideReplies();
    void handleMemsideRequests();

    // ports
    InPortBase<Packet*>* memside_in;
    OutPortBase<Packet*>* memside_out;

    // internal FIFOs
    std::vector< fifo<Packet*> > coreside_requests;
    std::vector< fifo<Packet*> > coreside_replies;

    fifo<Packet*> ccache_requests;
    fifo<Packet*> ccache_replies;

    fifo<Packet*> memside_requests;
    fifo<Packet*> memside_replies;

};

#endif
