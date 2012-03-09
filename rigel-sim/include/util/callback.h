#ifndef __CALLBACK_H__
#define __CALLBACK_H__

template <class P> 
class CallbackWrapper {
  public:
    virtual void operator() (P p) = 0;
    CallbackWrapper() { };
  private:
};

template < class Obj, 
           class Payload,
           void (Obj::*Func)(Payload) 
>
class MemberCallbackWrapper : public CallbackWrapper<Payload> {

  public: 
    MemberCallbackWrapper(Obj *o)
      : object(o) { }

    void operator() (Payload p) {
      (object->*Func)(p);
    }

    protected:
      Obj* object;
};
#endif
