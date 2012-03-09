#ifndef __PACKET_H__
#define __PACKET_H__

#include "define.h"

class Packet {
  
  public:

    /// constructor
    Packet(icmsg_type_t m,
           uint32_t a,
           uint32_t d, 
           int gcoreid,
           int gtid
    ) :
       _msgType(m),
       _addr(a),
       _data(d),
       _gcoreid(gcoreid),
       _gtid(gtid)
    {
    };

    /// destructor
    ~Packet() { };

    /// Dump
    void Dump() {
      printf("m: %d a:0x%08x d:0x%08x c:%d gtid:%d\n", 
        _msgType, _addr, _data, _gcoreid, _gtid);
    };

    /// accessors
    
    icmsg_type_t msgType() { return _msgType; }
    uint32_t     addr()    { return _addr;    }
    uint32_t     data()    { return _data;    }
    uint32_t     gcoreid()  { return _gcoreid;  }
    uint32_t     gtid()    { return _gtid;    }

    // FIXME: assumes even distribution...
    int          cluster_tid() { return _gtid % rigel::THREADS_PER_CLUSTER; } 
    int          local_coreid() { return _gcoreid % rigel::CORES_PER_CLUSTER; } 

    void addr(uint32_t a) { _addr = a; }
    void data(uint32_t d) { _data = d; }
    void msgType(icmsg_type_t t) { _msgType = t; }

  private:
    icmsg_type_t _msgType; // interconnect message type
    uint32_t     _addr;
    uint32_t     _data;
    int          _gcoreid;
    int          _gtid;

};

typedef Packet* PacketPtr;


#endif
