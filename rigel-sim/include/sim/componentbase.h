
#ifndef __COMPONENT_BASE_H__
#define __COMPONENT_BASE_H__

#include <stdint.h>
#include <vector>
#include <string>
#include <cstdio>

#include "sim/component_count.h"

using rigel::ComponentCount;
extern ComponentCount UnspecifiedCount;

// forward declarations

////////////////////////////////////////////////////////////////////////////////
/// class Componentbase
/// implements an abstract class interface to be used by components of the
/// simulator
////////////////////////////////////////////////////////////////////////////////
class ComponentBase {

  /// public methods
  public:

    /// default constructor
    ComponentBase( 
      ComponentBase* parent_component,     /// pointer to our parent component, can be NULL
      const char* cname = "ComponentBase",  /// a string name for this object, with default
      ComponentCount& idf = UnspecifiedCount                    /// functor to assign per class ID
    );

    /// no frills constructor
    //

    /// destructor
    ~ComponentBase() { }

    // TODO: allow edge ticks?
    // virtual void ClockTick()  = 0 ;

    ////////////////////////////////////////////////////////////////////////////
    /// virtual
    /// required to be implemented by derived classes
    ////////////////////////////////////////////////////////////////////////////

    /// compute new outputs based on new inputs
    /// TODO: rename from PerCycle to Update or something else
    virtual int PerCycle()     = 0 ; 

    /// dump object state 
    /// TODO: optionally dump contained objects 
    /// TODO: make const
    virtual void Dump() = 0;  

    /// for printing intermittent status data
    virtual void Heartbeat() = 0;   

    /// called at end of simulation
    virtual void EndSim() = 0;

    ////////////////////////////////////////////////////////////////////////////
    /// non-virtual base class functions
    /// interface exported by the base class
    ////////////////////////////////////////////////////////////////////////////

    void printHierarchy( int indent = 0 );
    void dumpHierarchy();
    void addChild( ComponentBase* c );

    ////////////////////////////////////////////////////////////////////////////
    /// accessors
    ////////////////////////////////////////////////////////////////////////////
    
    uint32_t ComponentID()                 { return component_id;  }
    int      id()                          { return id_;           }
    const char* name()                     { return name_.c_str(); }
    std::vector<ComponentBase*> &children(){ return children_;     }
    ComponentBase* parent()                { return parent_;       }

    static ComponentBase* component(unsigned int idx) {
			return components.at(idx);
    }

    /// return pointer to requested root component if it exists
    /// otherwise, return NULL
    static ComponentBase* rootComponent(unsigned int idx) {
      if (idx < rootComponents.size()) {
			  return rootComponents[idx];
      } else {
        return NULL;
      }
    }

    /// adds this component to the list of root components
    int setRootComponent() {
      rootComponents.push_back(this);
      isRoot = true;
      return true;
    }

  private:

  // private data members
  private:

    uint32_t    component_id; /// unique instantiation id for this class
    int         id_;           /// an id field to be used by various derived objects as desired
    std::string name_;        /// string naming for this object
    bool        isRoot;       /// true if this is a root component

    ComponentBase*              parent_;   /// pointer to this object's parent, if it exists
    std::vector<ComponentBase*> children_; /// list of this object's children

    /// static members
    static uint32_t COMPONENT_ID_COUNT;    /// for tracking globally unique component IDs
    static std::vector<ComponentBase*> components;    /// for tracking globally unique component IDs
    static std::vector<ComponentBase*> rootComponents;    /// for tracking globally unique component IDs

};

#endif
