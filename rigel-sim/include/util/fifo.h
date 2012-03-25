#ifndef __FIFO_H__
#define __FIFO_H__

#include <queue>

/// fixed-size FIFO / circular buffer
///
/// wrap std::queue for a fixed-size circular buffer FIFO for now
/// STL does not guaranteed fixed size, which we will always have
/// so we could implement a potentially more efficient fixed-size buffer
/// or use something like boost later
///
/// for static sizes, consider templating on CAPACITY
///
/// TODO: CAPACITY of 0 is considered infinite (regular std::queue)
///       would be more efficient to know this statically


template <class T>
class fifo : public std::queue<T> {

  public:
    fifo(unsigned c) : 
      std::queue<T>(),
      _capacity(c)
    { }

    /// override push
    /// return true if push succeeds, false if push fails
    bool push( const T& x ) {

      if (std::queue<T>::size() < _capacity) {
        std::queue<T>::push(x);
        return true;
      } else {
        return false;
      }

    }

    private:
      const unsigned _capacity;

};

#endif
