#ifndef __CALLBACK_H__
#define __CALLBACK_H__

template <class Payload, class ReturnType> 
class CallbackWrapper {
  public:
    virtual ReturnType operator() (Payload p) = 0;
    CallbackWrapper() { };
  private:
};

// for wrapping a member function
template < class Obj, 
           class Payload,
           class ReturnType,
           ReturnType (Obj::*Func)(Payload) 
>
class MemberCallbackWrapper : public CallbackWrapper<Payload,ReturnType> {

  public: 
    MemberCallbackWrapper(Obj *o)
      : object(o) { }

    ReturnType operator() (Payload p) {
      (object->*Func)(p);
    }

    protected:
      Obj* object;
};
#endif
