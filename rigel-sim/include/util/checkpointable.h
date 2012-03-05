#ifndef __CHECKPOINTABLE_H__
#define __CHECKPOINTABLE_H__

namespace rigelsim {
  class Checkpointable {
    public:
      virtual void save_state() const = 0;
      virtual void restore_state() = 0;
  };
}

#endif //#ifndef __CHECKPOINTABLE_H__
