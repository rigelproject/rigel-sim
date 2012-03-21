
// forward declarations
template<class T> class InPortBase;
template<class T> class OutPortBase;

template <typename T>
class PortManager {

  public:

    static void Dump() {

      // inports
      {
        typename std::map< std::string, InPortBase<T*> >::iterator it;
        for ( it = InPorts.begin(); it != InPorts.end(); ++it ) {
          std::cout << (*it).first << " => " << (*it).second << std::endl;
        }
      }

      // outports
      {
        typename std::map< std::string, OutPortBase<T*> >::iterator it;
        for ( it = OutPorts.begin(); it != OutPorts.end(); ++it ) {
          std::cout << (*it).first << " => " << (*it).second << std::endl;
        }
      }

    }

  private:

    static std::map< std::string, InPortBase<T>* >  InPorts;
    static std::map< std::string, OutPortBase<T>* > OutPorts;

};

template <class T> std::map< std::string, InPortBase<T>* >  PortManager<T>::InPorts;
template <class T> std::map< std::string, OutPortBase<T>* > PortManager<T>::OutPorts;


