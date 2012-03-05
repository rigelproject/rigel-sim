#ifndef __BACKING_STORE_CALLBACKS_H__
#define __BACKING_STORE_CALLBACKS_H__

#include <cstdio>
#include <cassert>
#include "rigelsim.pb.h"

#define BSCB_TEMPLATE_DECL template <typename ADDR_T, typename WORD_T>
#define BSCB_TEMPLATE_CLASSNAME BSCallbackBase<ADDR_T, WORD_T>

// abstract base class
BSCB_TEMPLATE_DECL
class BSCallbackBase {
public:
  virtual void operator()(ADDR_T startaddr, size_t numwords, const WORD_T data[]) {}
};

BSCB_TEMPLATE_DECL
class BSFileCallback : public BSCB_TEMPLATE_CLASSNAME
{
private:
  bool ownfile; //Whether or not we own the FILE *, and thus
                //whether or not we should close it in the destructor
  FILE *out;

public:
  BSFileCallback(const char *file_name) : ownfile(true) {
    out = fopen(file_name, "w");
    assert(out != NULL);
  }

  BSFileCallback(FILE *_out) : ownfile(false), out(_out) { }

  explicit BSFileCallback(const BSFileCallback &copy) : ownfile(copy.ownfile), out(copy.out) { }

  ~BSFileCallback() { if(ownfile) fclose(out); }

  // override operator "()"
  virtual void operator()(ADDR_T startaddr, size_t numwords, const WORD_T data[]) {
    assert(sizeof(WORD_T) == 4); //If this ever changes, need to make the fprintf code more generic
    assert(sizeof(ADDR_T) == 4);
    bool nonzero = false;
    for(size_t i = 0; i < numwords; i++) {
      if(data[i] != 0) {
        nonzero = true;
        break;
      }
    }
    if(nonzero) {
      fprintf(out, "%08"PRIx32" ", startaddr);
      for(size_t i = 0; i < numwords; i++) {
        fprintf(out, "%08"PRIx32"", data[i]);
      }
      fprintf(out, "\n");
    }
  }
};

BSCB_TEMPLATE_DECL
class BSProtobufCallback : public BSCB_TEMPLATE_CLASSNAME
{
private:
  rigelsim::MemoryState *ms;

public:
  BSProtobufCallback(rigelsim::MemoryState *_ms) : ms(_ms) { }
  explicit BSProtobufCallback(const BSProtobufCallback &copy) : ms(copy.ms) { } 

  virtual void operator()(ADDR_T startaddr, size_t numwords, const WORD_T data[]) {
    assert(sizeof(WORD_T) == 4); //If this ever changes, need to make the fprintf code more generic
    assert(sizeof(ADDR_T) == 4);
    bool nonzero = false;
    for(size_t i = 0; i < numwords; i++) {
      if(data[i] != 0) {
        nonzero = true;
        break;
      }
    }
    if(nonzero) {
      rigelsim::DataBlock *db = ms->add_data();
      db->set_startaddr(startaddr);
      for(size_t i = 0; i < numwords; i++)
        db->add_words(data[i]);
    }
  }
};

#endif //#ifndef __BACKING_STORE_CALLBACKS_H__
