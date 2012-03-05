#include "sim/componentbase.h"
#include <cassert>

uint32_t ComponentBase::COMPONENT_ID_COUNT = 0;
std::vector<ComponentBase*> ComponentBase::components;     /// for tracking globally unique component IDs
std::vector<ComponentBase*> ComponentBase::rootComponents; /// for tracking root components
ComponentCount UnspecifiedCount;

/// default constructor
ComponentBase::ComponentBase( 
  ComponentBase* parent_component,      /// pointer to our parent component, can be NULL
  const char* cname,                    /// a string name for this object, with default
  ComponentCount& idf                   /// assigns user-definable per-class object ID for this component
) : 
  component_id(ComponentBase::COMPONENT_ID_COUNT++),
  id_(idf()),
  name_(cname),
  parent_(parent_component),
  children_()
{ 
  // get a globally unique id for this component 
  assert(component_id == components.size()); // make sure id==index at insertion
  components.push_back(this);

  // if this object has a parent, add this child to its list
  if (parent_ != NULL) {
    parent_->addChild(this); 
  } else {
    printf("Warning: object %s %d has NULL parent \n", name_.c_str(), component_id);
  } 

}

/// pretty-print the object hierarchy
void 
ComponentBase::printHierarchy( int indent ) {
  fprintf(stderr,"%*sI am %s[%d] (global component %d)\n",indent,"",name_.c_str(),id_,component_id);
  indent+=2;
  for(size_t i=0; i<children_.size(); ++i) {
    children_[i]->printHierarchy(indent);
  }
}

/// add a child object to this parent
void 
ComponentBase::addChild( ComponentBase* c ) {
  // push_back is ok since this list doesn't change after startup...
  children_.push_back(c);
}

/// call Dump() down entire hierarchy
void
ComponentBase::dumpHierarchy() {
  Dump();
  for(size_t i=0; i<children_.size(); ++i) {
    children_[i]->Dump();
  }
}

