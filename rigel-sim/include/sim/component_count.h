#ifndef __COMPONENT_COUNT_H__
#define __COMPONENT_COUNT_H__

/// could alternately use the CRTP (curiously recurring template pattern) to let objects count
/// instances for ID assignment
namespace rigel {

  class ComponentCount {

    public:

      ComponentCount() : ID_COUNT(0) { }
   
      /// return next ID and increment 
      int operator()() {
        return ID_COUNT++;
      }
   
      int ID_COUNT;

    private:
      //Disallow copy construction
      ComponentCount(const ComponentCount& other);

  };

}

#endif //#ifndef __COMPONENT_COUNT_H__
