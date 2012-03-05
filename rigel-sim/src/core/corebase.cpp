#include "core/corebase.h"
#include "sim.h"
#include "util/construction_payload.h"

ComponentCount CoreBaseCount;

CoreBase::CoreBase(
  rigel::ConstructionPayload &cp, 
  ComponentCount& count
) :
  ComponentBase(cp.parent,
                cp.change_name("CoreBase").component_name.c_str(), 
                count),
  mem_backing_store(cp.backing_store),
  core_state(cp.core_state),
  halted_(0)
{
}
