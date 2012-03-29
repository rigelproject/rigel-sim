#ifndef __PORT_H__
#define __PORT_H__

#define DEBUG_PORT 0

#include "include/port/portmanager.h"

#include <string>
#include <iostream>
#include <cassert>

/// this base class is meaningless
/// FIXME either get rid of or define a general port
/// concept with meaning
class PortBase {
    public:
    protected:
    private:
};

//FIXME Use a function naming convention, not a class naming convention
std::string PortName( std::string parent, int id, std::string suffix, int index = -1);

// forward declarations
template <class T> class OutPortBase;
template <class T> class InPortBase;
template <class, class> class CallbackWrapper;


typedef enum {
  NACK = 0,
  ACK
} port_status_t;

/// default ports are stateless
template <class T> 
class InPortBase : public PortBase {

  public:

    InPortBase(std::string name) : 
      _valid(false),
      _ready(true),
      _name(name)
    { 
      PortManager<T>::registerInPort(this);
    }

    virtual port_status_t recvMsg(T msg) {
      DPRINT(DEBUG_PORT,"%s\n", __PRETTY_FUNCTION__);   
      //msg->Dump();
      // save msg
      if (_ready) {
        data = msg;
        _valid = true;
        _ready = false;
        DPRINT(DEBUG_PORT,"%s ACK\n", __PRETTY_FUNCTION__);
        return ACK;
      } else {
        DPRINT(DEBUG_PORT,"%s NACK\n", __PRETTY_FUNCTION__);
        return NACK;
      }
    };

    virtual T read() {
      if (_valid) {
        _valid = false;
        _ready = true;
        return data;
      } else {
        return NULL; // FIXME this is evil...and stupid
      }
    }

    virtual void attach( OutPortBase<T>* op ) { 
      DPRINT(DEBUG_PORT,"%s %s\n", __PRETTY_FUNCTION__, _name.c_str());   
      op->attach(this); 
    }

    std::string name() { return _name; }
    
    friend class PortManager<T>;

    bool ready() { return _ready; }

  private:
    bool _valid; /// data is valid
    bool _ready; /// ready to accept a message
    T    data;
    std::string _name; ///< port name

};

/// take a callback
/// T: payload
template <class T> 
class InPortCallback : public InPortBase<T> {
  
  public:

    InPortCallback( std::string name, 
                    CallbackWrapper<T,port_status_t>* handler 
    ) : InPortBase<T>(name),
        handler(handler)
    { }

    virtual port_status_t recvMsg(T msg) {
      DPRINT(DEBUG_PORT,"%s\n", __PRETTY_FUNCTION__);
      return (*handler)(msg);
    }

  private:

    CallbackWrapper<T,port_status_t> *handler; 

};

/// 
/// outports could have multiple sinks, but for now just one
/// 
/// TODO: template on port connection type AND payload
template <class T> 
class OutPortBase : public PortBase {

  public:

    OutPortBase(std::string name) : 
      _name(name),
      _connection(NULL) // no connections
    {
      PortManager<T>::registerOutPort(this);
    }

    virtual port_status_t sendMsg( T msg ) {
      //msg->Dump();   
      assert(_connection != NULL && "sending message via unconnected port!");
      return _connection->recvMsg(msg);
    }

    virtual void attach( InPortBase<T>* op ) {
      DPRINT(DEBUG_PORT,"%s %s\n", __func__, _name.c_str());   
      if (_connection) {
        throw ExitSim("OutPort already connected, this port does not support >1 connection");
      }
      _connection = op;
    }

    std::string name() { return _name; }

    std::string connection_name() { 
      if (_connection) {
        return _connection->name(); 
      } else {
        return std::string("unconnected");
      }
    }

    friend class PortManager<T>;

  private:
    std::string     _name; ///< port name
    InPortBase<T>* _connection;

};

/// these ports have state to manage
/// class InPortRegistered : public InPort {
/// };
/// class OutPortRegistered : public OutPort {
/// };

#endif

