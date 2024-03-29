
#include <map>
#include <iostream>
#include <fstream>
#include "util/util.h" // for ExitSim

// forward declarations
template<class T> class InPortBase;
template<class T> class OutPortBase;

template <typename T>
class PortManager {

  public:

    static void Dump() {

      // inports
      #if 0 
      {
        typename std::map< std::string, InPortBase<T>* >::iterator it;
        for ( it = InPorts.begin(); it != InPorts.end(); ++it ) {
          std::cout << "InPort: " << (*it).first << std::endl;
        }
      }
      #endif

      // outports
      typename std::map< std::string, OutPortBase<T>* >::iterator it;
      {
        for ( it = OutPorts.begin(); it != OutPorts.end(); ++it ) {
          std::cout << "OutPort: " << (*it).first << " -> " << (*it).second->connection_name() << std::endl;
        }
      }

    }

    static void DumpGraphviz() {

      std::fstream portgraph;

      portgraph.open("ports.dot", std::fstream::out);

      portgraph << "digraph PortGraph {" << std::endl;

      {
        typename std::map< std::string, OutPortBase<T>* >::iterator it;
        for ( it = OutPorts.begin(); it != OutPorts.end(); ++it ) {
          portgraph << (*it).second->owner()->name() << "_" << (*it).second->owner()->id() << " -> ";
          portgraph << (*it).first << " -> " << (*it).second->connection_name() << std::endl;
        }
      }
      {
        typename std::map< std::string, InPortBase<T>* >::iterator it;
        for ( it = InPorts.begin(); it != InPorts.end(); ++it ) {
          portgraph << (*it).second->owner()->name() << "_" << (*it).second->owner()->id() << " -> ";
          portgraph << (*it).first << std::endl;
        }
      }

      portgraph << "}";

      portgraph.close();

    }

    static void registerInPort( InPortBase<T>* p ) {

      typename std::map< std::string, InPortBase<T>* >::iterator it;
      it = InPorts.find(p->name());

      // register this port by name
      if (it == InPorts.end()) {
        InPorts[p->name()] = p;
      } else {
        printf("port name exists: %s\n", p->name().c_str());
        throw ExitSim("port name conflict!");
      }

    }

    static void registerOutPort( OutPortBase<T>* p ) {

      typename std::map< std::string, OutPortBase<T>* >::iterator it;
      it = OutPorts.find(p->name());

      if (it == OutPorts.end()) {
        OutPorts[p->name()] = p;
      } else {
        printf("port name exists: %s\n", p->name().c_str());
        throw ExitSim("port name conflict!");
      }

    }


  private:

    static std::map< std::string, InPortBase<T>* >  InPorts;
    static std::map< std::string, OutPortBase<T>* > OutPorts;

};

template <class T> std::map< std::string, InPortBase<T>* >  PortManager<T>::InPorts;
template <class T> std::map< std::string, OutPortBase<T>* > PortManager<T>::OutPorts;


