#ifdef __cplusplus
extern "C" {
#endif

#ifndef __READ_MEM_TRACE_H__
#define __READ_MEM_TRACE_H__

#include <stdint.h>

enum ctrl_code_e {
  skip = 0,
  thread_change = 1,
  mem_write = 2,
  mem_read = 3
};

struct stream_entry {
  uint8_t code : 2;
  uint8_t interval : 6;
};

// for memory accesses
struct access_info {
  uint32_t eip;
  uint32_t addr;
};

struct extended_info {
  uint32_t interval : 31; // number of non-mem insts being skipped
  uint32_t read : 1; // 1 if read, 0 if write
  struct access_info ai;
};

struct mem_trace_reader;

struct mem_trace_reader* init_trace_reader(const char* filename);
void destroy_mem_trace_reader(struct mem_trace_reader* mtr);
// sequential stream read
// returns 1 if success
int get_next_access(struct mem_trace_reader* mtr, struct extended_info* ei);
// threaded stream read 
// HW threads plus total insts per thread
// returns number of threads enqueued
// buffers as many SW threads per HW thread as needed until each HW thread has min_insts insts
int buffer_threads_min(struct mem_trace_reader* mtr, int num_hw_threads, int min_insts);
// buffers only 1 SW thread per HW thread, up to max_insts insts
int buffer_threads_max(struct mem_trace_reader* mtr, int num_hw_threads, int max_insts);
// actually buffers 1 SW thread, does not look at interval
// returns number of cycles covered by this SW thread
int buffer_one_sw_thread(struct mem_trace_reader* mtr, int hw_thread);
// buffers the next SW thread into the specified HW thread
// returns number of insts enqueued
int buffer_a_thread_max(struct mem_trace_reader* mtr, int hw_thread, int max_insts);
// returns 1 if success
int get_next_thread_access(struct mem_trace_reader* mtr, int thread, struct extended_info* ei);

#endif

#ifdef __cplusplus
}
#endif
