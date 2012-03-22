#ifndef __PORT_H__
#define __PORT_H__

#define DEBUG_PORT 0

#include "include/port/portmanager.h"

#include <string>

/// this base class is meaningless, either get rid of or define a general port
/// concept with meaning
class PortBase {
    public:
    protected:
    private:
};

// forward declarations
template <class T> class OutPortBase;
template <class T> class InPortBase;
template <class P> class CallbackWrapper;


typedef enum {
  NACK = 0,
  ACK
} port_status_t;

/// default ports are stateless
template <class T> 
class InPortBase : public PortBase {

  public:

    InPortBase(std::string name) : 
      _name(name),
      valid(false),
      ready(true)
    { 
      PortManager<T>::registerInPort(this);
    }

    virtual port_status_t recvMsg(T msg) {
      DPRINT(DEBUG_PORT,"%s\n", __PRETTY_FUNCTION__);   
      //msg->Dump();
      // save msg
      if (ready) {
        data = msg;
        valid = true;
        ready = false;
        DPRINT(DEBUG_PORT,"%s ACK\n", __PRETTY_FUNCTION__);
        return ACK;
      } else {
        DPRINT(DEBUG_PORT,"%s NACK\n", __PRETTY_FUNCTION__);
        return NACK;
      }
    };

    virtual T read() {
      if (valid) {
        valid = false;
        ready = true;
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

  private:
    T    data;
    bool valid; /// data is valid
    bool ready; /// ready to accept a message
    std::string _name; ///< port name

};

/// take a callback
/// T: payload
template <class T> 
class InPortCallback : public InPortBase<T> {
  
  public:

    InPortCallback( std::string name, 
                    CallbackWrapper<T>* handler 
    ) : InPortBase<T>(name),
        handler(handler)
    { }

    virtual port_status_t recvMsg(T msg) {
      DPRINT(DEBUG_PORT,"%s\n", __PRETTY_FUNCTION__);
      (*handler)(msg);
      return ACK; // assume handler always works, since it doesn't return anything
    }

  private:

    CallbackWrapper<T> *handler; 

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

    std::string connection_name() { return _connection->name(); }

    friend class PortManager<T>;

  private:
    InPortBase<T>* _connection;
    std::string _name; ///< port name

};

/// these ports have state to manage
/// class InPortRegistered : public InPort {
/// };
/// class OutPortRegistered : public OutPort {
/// };

#endif

