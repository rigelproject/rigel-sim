#ifndef __GCACHE_REQUEST_STATE_MACHINE_H__
#define __GCACHE_REQUEST_STATE_MACHINE_H__

#include <cassert>
#include <ostream>
#include <iomanip>
#include <stdint.h>

static const char *const GCacheRequestStateNames[] = {
  "GCRS_WAITING_TO_PEND",
  "GCRS_PENDING",
  "GCRS_WAITING_TO_REPLY",
  "GCRS_DONE"
};

class GCacheRequestStateMachine
{
  public:
    enum GCacheRequestState {
      GCRS_WAITING_TO_PEND = 0,
      GCRS_PENDING,
      GCRS_WAITING_TO_REPLY,
      GCRS_DONE
    };

    GCacheRequestStateMachine(uint32_t _addr) : addr(_addr), state(GCRS_WAITING_TO_PEND) { }
    GCacheRequestStateMachine(const GCacheRequestStateMachine & other)
      : addr(other.addr), state(other.state) { }
    //Events
    void pend() {
      assert(state == GCRS_WAITING_TO_PEND && "pend()ed in wrong state");
      state = GCRS_PENDING;
    }
    void dataBack() {
      assert(state == GCRS_PENDING && "dataBack()ed in wrong state");
      state = GCRS_WAITING_TO_REPLY;
    }
    void replySent() {
      assert(state == GCRS_WAITING_TO_REPLY && "replySent()ed in wrong state");
      state = GCRS_DONE;
    }
    void wasValid() {
      assert(state == GCRS_WAITING_TO_PEND && "wasValid()ed in wrong state");
      state = GCRS_WAITING_TO_REPLY;
    }
    //Meta
    uint32_t getAddr() const { return addr; }
    const char *getState() const { return GCacheRequestStateNames[state]; }
    bool isWaitingToPend() const { return (state == GCRS_WAITING_TO_PEND); }
    bool isPending() const { return (state == GCRS_PENDING); }
    bool isWaitingToReply() const { return (state == GCRS_WAITING_TO_REPLY); }
    bool isDone() const { return (state == GCRS_DONE); }

    //Operators
    bool operator<(const GCacheRequestStateMachine& other) const { //For sorting in a std::map<>
      return (addr < other.addr);
    }
    //Defined in gcache_request_state_machine.cpp
    friend std::ostream& operator<<(std::ostream&, const GCacheRequestStateMachine &sm);

  protected:
    uint32_t addr;
    GCacheRequestState state;
};

#endif //#ifndef __GCACHE_REQUEST_STATE_MACHINE_H__
