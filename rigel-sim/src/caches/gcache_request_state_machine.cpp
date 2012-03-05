#include <iomanip>                      // for operator<<, setfill, setw, etc
#include <ostream>                      // for operator<<, basic_ostream, etc
#include <string>                       // for char_traits
#include "gcache_request_state_machine.h"

std::ostream& operator<<(std::ostream& os, const GCacheRequestStateMachine &sm) {
  os << "(" << std::hex << std::setfill('0') << std::setw(8) << sm.addr;
  os << ", " << GCacheRequestStateNames[sm.state] << ")";
  return os;
}

