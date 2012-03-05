#ifndef __BACKING_STORE_H__
#define __BACKING_STORE_H__

#include <cstddef>  //For size_t
#include <cstdio>   //For *printf()
#include <cassert>
#include <queue>
#include <inttypes.h>

#include "define.h" //For rigel_log2()
#include "sim.h"    //For rigel::WARN_ON_UNINITIALIZED_MEMORY_ACCESSES
#include "memory/backing_store_callbacks.h"
#include "util/construction_payload.h"
#include "util/checkpointable.h"
#include "rigelsim.pb.h" //Unfortunately, can't forward declare MemoryState because
                         //we use it in restore_state(), and it's difficult to put
                         //restore_state() in a .cpp file because BackingStore is templatized.

//NOTE: This is slow, so don't use it in an inner loop.
template <typename T>
static T getBitmask(const unsigned int lo, const unsigned int hi) {
  assert((hi < (sizeof(T)*8)) && "hi bit larger than type");
  assert((lo < (sizeof(T)*8)) && "lo bit larger than type");
  T x = 0;
  for(unsigned int i = lo; i <= hi; i++)
    x |= ((T)1U << i);
  //printf("getBitmask(%u, %u): %016llX\n", lo, hi, x);
  return x;
}

#define BS_TEMPLATE_DECL template <size_t ADDRESS_SPACE_BITS, size_t RADIX_SHIFT, typename ADDR_T, typename WORD_T, size_t WORDS_PER_CHUNK_SHIFT>
#define BS_TEMPLATE_CLASSNAME BackingStore<ADDRESS_SPACE_BITS, RADIX_SHIFT, ADDR_T, WORD_T, WORDS_PER_CHUNK_SHIFT>

BS_TEMPLATE_DECL
class BackingStore : public rigelsim::Checkpointable {
  public:
    BackingStore(rigel::ConstructionPayload cp, const WORD_T _init_val);
    ~BackingStore();
    void write_word(const ADDR_T addr, const WORD_T value);
    WORD_T read_word(const ADDR_T addr) const;
    // Dump state to a file.
    void dump_to_file(const char *file_name) const;
    void dump_to_file(FILE *f) const;
    void dump_to_protobuf(rigelsim::MemoryState *cs) const;

    void traverse(BSCallbackBase<ADDR_T, WORD_T> &cb) const;
    virtual void save_state() const;
    virtual void restore_state();

  private:
    void traverse_helper(void * const *ptrptr, ADDR_T addr, unsigned int level, BSCallbackBase<ADDR_T, WORD_T> &cb) const;
    WORD_T *get_word_pointer_or_null(ADDR_T addr) const;
    WORD_T &get_or_create_word_reference(ADDR_T addr);
    WORD_T *get_initialized_chunk();
    void **get_initialized_radix_tree_node(unsigned int level);
    void clean(void * node, unsigned int level);
    const WORD_T init_val;
    const unsigned int tree_levels;
    struct shift_mask {
      ADDR_T mask;
      unsigned int shift;
      unsigned int numbits;
    };
    shift_mask *traversal_data;
    void *TOT; //FIXME should this be a union of void ** and WORD_T *?  Should make this AA-correct
    rigelsim::MemoryState *memory_state;
};
BS_TEMPLATE_DECL
BS_TEMPLATE_CLASSNAME::BackingStore(rigel::ConstructionPayload cp, const WORD_T _init_val)
                                       : init_val(_init_val),
                                         tree_levels((ADDRESS_SPACE_BITS-WORDS_PER_CHUNK_SHIFT-rigel_log2(sizeof(WORD_T))+RADIX_SHIFT-1) / RADIX_SHIFT),
                                         memory_state(cp.memory_state)
{
  traversal_data = new shift_mask[tree_levels+1];
  unsigned int word_shift = rigel_log2(sizeof(WORD_T));
  unsigned int low_bit = word_shift;
  unsigned int high_bit = low_bit + WORDS_PER_CHUNK_SHIFT;

  traversal_data[tree_levels].shift = low_bit;
  traversal_data[tree_levels].mask = getBitmask<ADDR_T>(low_bit, high_bit-1);
  traversal_data[tree_levels].numbits = high_bit-low_bit;
  //Compute size of last radix level
  const unsigned int remaining_bits = (ADDRESS_SPACE_BITS-WORDS_PER_CHUNK_SHIFT-word_shift) % RADIX_SHIFT;
  const unsigned int last_level_bits = (remaining_bits == 0) ? RADIX_SHIFT : remaining_bits;

  if(tree_levels >= 1) {
    low_bit = high_bit;
    high_bit += last_level_bits;
    traversal_data[tree_levels-1].shift = low_bit;
    traversal_data[tree_levels-1].mask = getBitmask<ADDR_T>(low_bit, high_bit-1);
    traversal_data[tree_levels-1].numbits = high_bit-low_bit;
  }
  if(tree_levels >= 2) {
    for(int i = (int)tree_levels-2; i >= 0; i--) {
      low_bit = high_bit;
      high_bit += RADIX_SHIFT;
      traversal_data[i].shift = low_bit;
      traversal_data[i].mask = getBitmask<ADDR_T>(low_bit, high_bit-1);
      traversal_data[i].numbits = high_bit-low_bit;
    }
  }
  // printf("%zu Address space bits, %zu radix shift, %u %zu-byte words per chunk, %u tree_levels\n",
  //        ADDRESS_SPACE_BITS, RADIX_SHIFT, (1U << WORDS_PER_CHUNK_SHIFT), sizeof(WORD_T), tree_levels);
  // for(unsigned int i = 0; i < (tree_levels+1); i++) {
  //   printf("Level %2d: %2u shift, %2u bits, 0x%016llX mask\n", i, traversal_data[i].shift, traversal_data[i].numbits, traversal_data[i].mask );
  // }

  //NOTE: MUST be done after traversal_data is set up.
  if(tree_levels >= 1)
    TOT = reinterpret_cast<void *>(get_initialized_radix_tree_node(0));
  else
    TOT = reinterpret_cast<void *>(get_initialized_chunk());
}

BS_TEMPLATE_DECL
BS_TEMPLATE_CLASSNAME::~BackingStore()
{
  clean(TOT, 0);
}

BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::write_word(const ADDR_T addr, const WORD_T value)
{
  assert((((ADDRESS_SPACE_BITS/8) == sizeof(ADDR_T)) || (rigel_log2(addr) <= (ADDRESS_SPACE_BITS-1))) && "Address too big!");
  WORD_T &slot = get_or_create_word_reference(addr);
  slot = value;
}

BS_TEMPLATE_DECL
WORD_T BS_TEMPLATE_CLASSNAME::read_word(const ADDR_T addr) const
{
  assert((((ADDRESS_SPACE_BITS/8) == sizeof(ADDR_T)) || (rigel_log2(addr) <= (ADDRESS_SPACE_BITS-1))) && "Address too big!");
  WORD_T *ptr = get_word_pointer_or_null(addr);
  if(ptr == NULL) {
    if(rigel::WARN_ON_UNINITIALIZED_MEMORY_ACCESSES)
      fprintf(stderr, "Warning: Reading word 0x%08x which has not been written yet\n", addr);
    return init_val;
  }
  else
    return (*ptr);
}

BS_TEMPLATE_DECL
WORD_T * BS_TEMPLATE_CLASSNAME::get_word_pointer_or_null(ADDR_T addr) const
{
  //printf("get_word_ptr %08X:\n--------\n", addr);
  void ** const *ptrptr = reinterpret_cast<void ** const *>(&TOT);
  //Traverse radix tree RADIX_SHIFT bits at a time.
  for(unsigned int i = 0; i < tree_levels; i++) {
    if(*ptrptr == NULL)
    {
      //printf("Returning NULL from Level %u\n", i);
      return NULL;
    }
    const ADDR_T idx = (addr & traversal_data[i].mask) >> traversal_data[i].shift;
    //printf("Level %u: mask %08X shift %u idx %08X\n", i, traversal_data[i].mask, traversal_data[i].shift, idx);
    void **ptr = *ptrptr;
    ptrptr = reinterpret_cast<void ***>(&(ptr[idx]));
  }
  WORD_T *dataptr = reinterpret_cast<WORD_T *>(*ptrptr);
  if(dataptr == NULL) {
    //printf("Returning NULL from dataptr, addr = %08X\n", addr);
    return NULL;
  }
  const ADDR_T idx = (addr & traversal_data[tree_levels].mask) >> traversal_data[tree_levels].shift;
  //printf("Data Idx = %08X", idx);
  return &(dataptr[idx]);
}



BS_TEMPLATE_DECL
WORD_T & BS_TEMPLATE_CLASSNAME::get_or_create_word_reference(ADDR_T addr)
{
  //printf("get_or_create %08X:\n-------\n", addr);
  void ***ptrptr = reinterpret_cast<void ***>(&TOT);
  //Traverse radix tree RADIX_SHIFT bits at a time.
  for(unsigned int i = 0; i < tree_levels; i++) {
    if(*ptrptr == NULL) {
      //printf("Initializing radix tree node at level %u\n", i);
      *ptrptr = get_initialized_radix_tree_node(i);
    }
    const ADDR_T idx = (addr & traversal_data[i].mask) >> traversal_data[i].shift;
    //printf("Level %u: mask %08X shift %u idx %08X\n", i, traversal_data[i].mask, traversal_data[i].shift, idx);
    void **ptr = *ptrptr;
    ptrptr = reinterpret_cast<void ***>(&(ptr[idx]));
  }
  WORD_T **dataptrptr = reinterpret_cast<WORD_T **>(ptrptr);
  if(*dataptrptr == NULL) {
    //printf("Allocating chunk at 0x%08x\n", addr);
    *dataptrptr = get_initialized_chunk();
  }
  const ADDR_T idx = (addr & traversal_data[tree_levels].mask) >> traversal_data[tree_levels].shift;
  //printf("Data Idx = %08X\n", idx);
  WORD_T *dataptr = *dataptrptr;
  return dataptr[idx];
}

BS_TEMPLATE_DECL
WORD_T * BS_TEMPLATE_CLASSNAME::get_initialized_chunk()
{
  size_t size = 1U << WORDS_PER_CHUNK_SHIFT;
  //printf("Allocating chunk of size %zu\n", size);
  WORD_T *ptr = new WORD_T[size];
  for(size_t i = 0; i < size; i++)
    ptr[i] = init_val;
  return ptr;
}

BS_TEMPLATE_DECL
void ** BS_TEMPLATE_CLASSNAME::get_initialized_radix_tree_node(unsigned int level)
{
  assert(tree_levels != 0 && "Should never call get_initialized_radix_tree_node() if tree_levels == 0");
  const unsigned int shift_of_above_segment = (level == 0) ? ADDRESS_SPACE_BITS : traversal_data[level-1].shift;
  const unsigned int shift = shift_of_above_segment - traversal_data[level].shift;
  size_t size = (size_t)1U << shift;
  //printf("Allocating radix tree node at level %u of size %zu\n", level, size);
  void **ptr = new void *[size];
  for(size_t i = 0; i < size; i++)
    ptr[i] = NULL;
  return ptr;
}

BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::clean(void * node, unsigned int level)
{
  if(level >= tree_levels) {
    WORD_T *dataptr = reinterpret_cast<WORD_T *>(node);
    if(dataptr != NULL)
      delete[] dataptr;
  }
  else {
    void **nodeptr = reinterpret_cast<void **>(node);
    const unsigned int shift_of_above_segment = (level == 0) ? ADDRESS_SPACE_BITS : traversal_data[level-1].shift;
    const unsigned int shift = shift_of_above_segment - traversal_data[level].shift;    
    for(size_t i = 0; i < ((size_t)1U << shift); i++)
      if(nodeptr[i] != NULL)
        clean(nodeptr[i], level+1);
    delete[] nodeptr;
  }
}


BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::traverse(BSCallbackBase<ADDR_T, WORD_T> &cb) const
{
  traverse_helper(reinterpret_cast<void * const *>(TOT), 0, 0, cb);
}

#define BS_WORDS_PER_LINE 8

BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::traverse_helper(void * const *ptrptr, ADDR_T addr, unsigned int level, BSCallbackBase<ADDR_T, WORD_T> &cb) const
{
  assert(sizeof(WORD_T) == 4);
  //printf("traverse_helper(%p, %08x, %d)\n", ptrptr, addr, level);
  if(level == tree_levels) {
    WORD_T const *dataptr = reinterpret_cast<WORD_T const *>(ptrptr);
    int numwords = 1 << traversal_data[level].numbits;
    assert(numwords % BS_WORDS_PER_LINE == 0);
    int numlines = numwords / BS_WORDS_PER_LINE;
    for(int i = 0; i < numlines; i++) {
      cb(addr+(i*BS_WORDS_PER_LINE*sizeof(ADDR_T)), BS_WORDS_PER_LINE, dataptr+(i*BS_WORDS_PER_LINE));
    }
  }
  else { //Interior pointer level
    int numpointers = 1 << traversal_data[level].numbits;
    for(int i = 0; i < numpointers; i++) {
      if(ptrptr[i] != NULL) {
        //printf("index %d -> ", i);
        traverse_helper(reinterpret_cast<void * const *>(ptrptr[i]), addr | (i << traversal_data[level].shift), level+1, cb);
      }
    }
  }
}

// Dump the memory state to a file.
// File format is ASCII.  Each line consists of "<HEX LINE BASE ADDRESS> <HEX LINE DATA>"
// For a standard 32-bit address space and 32-byte cache lines, this means lines will be
// 8 (address) + 1 (space) + 64 (data) = 73 characters long.
// TODO: More human-friendly file format is implemented too, but not exposed.
// By default, exclude all cache lines which have all zero values.
// If the consumer of this dump does not implicitly know that all lines not dumped have all-zero
// values, or if we change our default memory value to be non-zero, or if we want to know exactly
// how much data was touched (memory footprint), the second argument should be set to false.
BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::dump_to_file(const char *file_name) const {
	BSFileCallback<ADDR_T, WORD_T> cb(file_name);
  traverse(cb);
}

BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::dump_to_file(FILE *f) const {
  BSFileCallback<ADDR_T, WORD_T> cb(f);
  traverse(cb);
}

BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::dump_to_protobuf(rigelsim::MemoryState *ms) const {
  BSProtobufCallback<ADDR_T, WORD_T> cb(ms);
  traverse(cb);
}

BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::save_state() const {
  dump_to_protobuf(memory_state);
}

BS_TEMPLATE_DECL
void BS_TEMPLATE_CLASSNAME::restore_state() {
  for(::google::protobuf::RepeatedPtrField< rigelsim::DataBlock >::const_iterator
    it = memory_state->data().begin(), end = memory_state->data().end(); it != end; ++it) {
    ADDR_T addr = it->startaddr();
    for(::google::protobuf::RepeatedField< google::protobuf::uint32 >::const_iterator
      it2 = it->words().begin(), end2 = it->words().end(); it2 != end2; ++it2) {
      write_word(addr, *it2);
      addr += sizeof(google::protobuf::uint32); //FIXME should assert that sizeof(google::protobuf::uint32) == sizeof(WORD_T)
      //or otherwise unify the 2 types   
    }    
  }
}

#endif //#ifndef __BACKING_STORE_H__
