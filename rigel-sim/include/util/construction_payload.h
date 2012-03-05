#ifndef __CONSTRUCTION_PAYLOAD_H__
#define __CONSTRUCTION_PAYLOAD_H__

#include <string>
#include "user.config" //Needed for typedefs, e.g. MemoryTimingType

class ComponentBase; //FIXME Relocate ComponentBase to rigel namespace
class BroadcastManager;
class TileInterconnectBase;
class Syscall;

namespace rigelsim {
  class MemoryState;
  class CacheState;
  class ThreadState;
  class CoreState;
  class ClusterState;
  class ChipState;
  //class HostState; //FIXME Not yet implemented
}

namespace rigel {
    class ComponentCount;
    class TileNetworkBase;

    class ConstructionPayload {
    public:
        //Provide a facility to change the component_name of a payload
        //such that it can be done from the initializer list of another class' constructor.
        //This way, the other classes can specify default component names that
        //will take effect if none has been explicitly set by the caller.
        //This should be used like:
        //DerivedFoobar(ConstructionPayload cp) : BaseFoobar(cp.change_name("DerivedFoobar")) { ... }
        ConstructionPayload &change_name(const char *new_name, bool override = false) {
          std::string s(new_name);
          return change_name(s, override);
        }
        ConstructionPayload &change_name(std::string &new_name, bool override = false) {  
          if(component_name.empty() || override)
            component_name = new_name;
          return (*this);
        }
				ConstructionPayload &change_component_count(ComponentCount &cc, bool override = false) {
					if((component_count == NULL) || override)
						component_count = &cc;
					return (*this);
				}

        ComponentBase *parent;
        int component_index; //Commonly used to tell a component which member of an array of components it is.
                             //For example, which global cache bank am I, which core in the cluster am I?
        MemoryTimingType *memory_timing;
        GlobalBackingStoreType *backing_store;
        std::string component_name;
        ComponentCount *component_count;
        GlobalNetworkType *global_network;
        TileInterconnectBase *tile_network;
        BroadcastManager *broadcast_manager;
        Syscall *syscall;

        //Protocol buffer object pointers
        rigelsim::MemoryState *memory_state;
        rigelsim::CacheState *cache_state;
        rigelsim::ThreadState *thread_state;
        rigelsim::CoreState *core_state;
        rigelsim::ClusterState *cluster_state;
        rigelsim::ChipState *chip_state;
        //rigelsim::HostState *host_state;
    };
}

#endif //#ifndef __CONSTRUCTION_PAYLOAD_H__
