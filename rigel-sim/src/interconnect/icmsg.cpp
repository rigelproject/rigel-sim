
////////////////////////////////////////////////////////////////////////////////
// icmsg.cpp
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIx64
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, stderr
#include <iomanip>                      // for operator<<, setw
#include <iostream>                     // for operator<<, cerr, etc
#include <set>                          // for set, set<>::const_iterator, etc
#include <string>                       // for char_traits, string
#include <vector>                       // for vector, etc
#include "memory/address_mapping.h"  // for AddressMapping
#include "define.h"         // for ::IC_MSG_LINE_WRITEBACK_REQ, etc
#include "gcache_request_state_machine.h"
#include "icmsg.h"          // for ICMsg
#include "sim.h"            // for CURR_CYCLE, GCACHE_SETS

void
ICMsg::dump(std::string callee_file, int callee_line) const
{
	  using std::cerr;
    //DEBUG_HEADER();
    fprintf(stderr, "0x%08"PRIx64" (from %s:%d):", rigel::CURR_CYCLE, callee_file.c_str(), callee_line);
    cerr << " MSG#: " << std::setw(2) <<   msg_number;
    cerr << " TYPE#: " << type;
    cerr << " (ADDR, STATE): ";
    for(std::vector<GCacheRequestStateMachine>::const_iterator it = state_machines.begin(),
                                                            end = state_machines.end();
        it != end; ++it)
    {
      cerr << "(" << (*it) << ")";
    }
    cerr << " DEST_CLUSTER: " << std::dec << get_cluster();
    cerr << " GCACHE PENDED?: " << std::dec << check_gcache_pended();
    cerr << " @ 0x" << std::hex << pend_set_cycle;
    cerr << " G$ BANK " << std::dec << AddressMapping::GetGCacheBank(get_addr()) << " SET "
         << AddressMapping::GetGCacheSet(get_addr(), rigel::cache::GCACHE_SETS);
    cerr << " READY @ 0x" << std::hex << get_ready_cycle();
    cerr << std::dec;
    cerr << "\n";
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
icmsg_type_t
ICMsg::bcast_notify_convert(icmsg_type_t t) {
  switch(t)
  {
    case (IC_MSG_BCAST_UPDATE_REQ): return IC_MSG_BCAST_UPDATE_NOTIFY;
    case (IC_MSG_BCAST_INV_REQ): return IC_MSG_BCAST_INV_NOTIFY;

    default: assert( 0 && "Should not use this type of message with bcast_notify_convert()" );
  }
  assert(0 && "CANNOT REACH HERE!");
  return IC_MSG_NULL;
}

// Return the address of the message
uint32_t ICMsg::get_addr() const { return get_sms_begin()->getAddr(); }

//Copy set of addresses over from e.g. MSHR.
//Takes two arguments which are iterators (probably to some STL container) representing
//the beginning and end of the sequence of addresses to add.
void ICMsg::copy_addresses(std::set<uint32_t>::const_iterator begin, std::set<uint32_t>::const_iterator end)
{
  for(std::set<uint32_t>::const_iterator it = begin, _end = end; it != _end; ++it)
  {
    state_machines.push_back(GCacheRequestStateMachine(*it));
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ICMsg::copy_addresses(std::vector<GCacheRequestStateMachine>::const_iterator begin, std::vector<GCacheRequestStateMachine>::const_iterator end)
{
  for(std::vector<GCacheRequestStateMachine>::const_iterator it = begin, _end = end; it != _end; ++it)
  {
    state_machines.push_back(GCacheRequestStateMachine(*it));
  }
}

std::vector<GCacheRequestStateMachine>::iterator ICMsg::get_sms_begin() { return state_machines.begin(); }
std::vector<GCacheRequestStateMachine>::iterator ICMsg::get_sms_end() { return state_machines.end(); }
std::vector<GCacheRequestStateMachine>::const_iterator ICMsg::get_sms_begin() const { return state_machines.begin(); }
std::vector<GCacheRequestStateMachine>::const_iterator ICMsg::get_sms_end() const { return state_machines.end(); }

void ICMsg::add_addr(uint32_t addr) { state_machines.push_back(GCacheRequestStateMachine(addr)); }

void ICMsg::remove_addr(std::vector<GCacheRequestStateMachine>::iterator it) {
  //assert(0 && "Are we sure we want to allow this?");
  state_machines.erase(it);
}

void ICMsg::remove_all_addrs() { state_machines.clear(); }

bool ICMsg::all_addrs_done() const {
  for(std::vector<GCacheRequestStateMachine>::const_iterator it = state_machines.begin(), end = state_machines.end();
      it != end; ++it)
  {
    if(!it->isDone())
      return false;
  }
  return true;
}

