#ifndef __PACKET_H__
#define __PACKET_H__

#include "define.h"

class Packet {
  
  public:

    /// constructor
    Packet(icmsg_type_t m,
           uint32_t a,
           uint32_t d
    ) :
       _msgType(m),
       _addr(a),
       _data(d)
    {
    };

    /// destructor
    ~Packet() { };

    /// Dump
    void Dump() {};

    /// accessors
    
    icmsg_type_t msgType() { return _msgType; }
    uint32_t     addr()    { return _addr;    }
    uint32_t     data()    { return _data;    }

  private:
    icmsg_type_t _msgType; // interconnect message type
    uint32_t     _addr;
    uint32_t     _data;

};

typedef Packet* PacketPtr;


#endif
