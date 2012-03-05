
////////////////////////////////////////////////////////////////////////////////
// seqnum.h
////////////////////////////////////////////////////////////////////////////////

#ifndef __SEQNUM_H__
#define __SEQNUM_H__

#include "sim.h"

////////////////////////////////////////////////////////////////////////////////
// Struct Name: seq_num_t
////////////////////////////////////////////////////////////////////////////////
// Sequence numbers used by directory protocol FSM.  Each channel between a
// cluster cache and a directory has a FSM that sequences requests.  That way
// the cluster cache can order a probe it recevies to an address that it has
// an outstanding request to.
//
// this should be a class
//
////////////////////////////////////////////////////////////////////////////////
typedef struct seq_num_t 
{
  // Constructor.
  seq_num_t() { reset(); }
  // Assert that we own the channel.
  void ccache_get(int seq_channel);
  // Return the channel to the free pool.
  void ccache_put(int seq_channel);
  // At the directory we set the cycle that we used the request.
  void dir_update(int seq_channel) { directory_ts[seq_channel] = rigel::CURR_CYCLE; }
  // Reset the sequence number.
  void reset();
  // Returns true with a valid, empty channel number in 'chan'.
  bool get_unused_channel(int &chan) const;
  // State of the sequence number FSM.
  bool in_use[rigel::cache::OUTSTANDING_GCACHE_TO_CCACHE_REQS];
  uint64_t cluster_ts[rigel::cache::OUTSTANDING_GCACHE_TO_CCACHE_REQS];
  uint64_t directory_ts[rigel::cache::OUTSTANDING_GCACHE_TO_CCACHE_REQS];
} seq_num_t;


inline void 
seq_num_t::ccache_get(int seq_channel) 
{
  in_use[seq_channel] = true;
  cluster_ts[seq_channel] = rigel::CURR_CYCLE;
}

inline void 
seq_num_t::ccache_put(int seq_channel) 
{
  in_use[seq_channel] = false;
  cluster_ts[seq_channel] = 0;
}

inline void 
seq_num_t::reset() 
{
  for (int i = 0; i < rigel::cache::OUTSTANDING_GCACHE_TO_CCACHE_REQS; i++) {
    in_use[i] = false;
    cluster_ts[i] = 0;
    directory_ts[i] = 0;
  }
}

inline bool 
seq_num_t::get_unused_channel(int &chan) const 
{
  for (int i = 0; i < rigel::cache::OUTSTANDING_GCACHE_TO_CCACHE_REQS; i++) {
    if (!in_use[i]) { chan = i; return true; }
  }
  return false;
}


#endif
