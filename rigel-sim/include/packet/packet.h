#ifndef __PACKET_H__
#define __PACKET_H__

#include "define.h"

// memory request packet

class Packet {
  
  public:

    /// constructor
    Packet(icmsg_type_t m,
           uint32_t a,
           uint32_t d, 
           uint32_t pc,
           int gcoreid,
           int gtid
    ) :
       _msgType(m),
       _addr(a),
       _data(d),
       _pc(pc),
       _gcoreid(gcoreid),
       _gtid(gtid),
       _birthday(rigel::CURR_CYCLE),
       _completed(false)
    { };

    /// constructor
    Packet(icmsg_type_t m) :
       _msgType(m),
       _addr(0),
       _data(0),
       _pc(0),
       _gcoreid(-1), // invalid
       _gtid(-1),    // invalid
       _birthday(rigel::CURR_CYCLE),
       _completed(false)
    {
    };

    void initCorePacket(uint32_t a, uint32_t d, int gcoreid, int gtid) {
      _addr = a;
      _data = d;
      _gcoreid = gcoreid;
      _gtid = gtid;
    }

    /// destructor
    ~Packet() { };

    /// Dump
    void Dump() {
      printf("m: %d a:0x%08x d:0x%08x pc:0x%08x c:%d gtid:%d\n", 
        _msgType, _addr, _data, _pc, _gcoreid, _gtid);
    };

    /// accessors
    
    icmsg_type_t msgType() { return _msgType; }
    uint32_t     addr()    { return _addr;    }
    uint32_t     data()    { return _data;    }
    int          gcoreid() { return _gcoreid; }
    int          gtid()    { return _gtid;    }
    uint32_t     gAtomicOperand() { return _gatomic_operand; }

    // FIXME: assumes even distribution...
    int          cluster_tid() { return _gtid % rigel::THREADS_PER_CLUSTER; } 
    int          local_coreid() { return _gcoreid % rigel::CORES_PER_CLUSTER; } 

    void addr(uint32_t a) { _addr = a; }
    void data(uint32_t d) { _data = d; }
    void pc(uint32_t p)   { _pc = p; }
    void msgType(icmsg_type_t t) { _msgType = t; }
    void gAtomicOperand( uint32_t o ) { _gatomic_operand = o; }

    // TODO: these features could be shared with instr packets in a base class
    // along with counting dynamic (live) and total instances
    uint64_t birthday() { return _birthday; }
    uint64_t age()      { return rigel::CURR_CYCLE - _birthday; }

    bool isCompleted()  { return _completed; }
    void setCompleted() { _completed = true; }

  private:
    icmsg_type_t _msgType; ///< interconnect message type
    uint32_t     _addr;    ///< address of this request
    uint32_t     _data;    ///< 

    uint32_t     _pc;      ///< pc of instruction that generated request, if relevant

    int          _gcoreid; ///< global core ID (should REMOVE, use TID)
    int          _gtid;    ///< global thread ID

    uint32_t     _gatomic_operand; /// global atomic operand

    uint64_t     _birthday; ///< cycle this was constructed

    uint64_t     _id;       ///< unique (modulo 2^64) identifier assigned at creation

    bool         _completed; /// has been fully serviced

};

typedef Packet* PacketPtr;


#endif
