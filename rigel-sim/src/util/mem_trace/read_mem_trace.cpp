#include "read_mem_trace_c.h"
#include <vector>
#include <deque>
#include <zlib.h>

struct mem_trace_reader {
public:
  mem_trace_reader(char* filename);
  ~mem_trace_reader();

  // sequential stream read
  bool get_next_access(extended_info& ei);
  // threaded stream read 
  // HW threads plus total insts per thread
  int buffer_threads(int num_threads, int num_insts);
  bool get_next_access(int thread, extended_info& ei);

private:
  gzFile infile;
  std::vector<std::deque<extended_info> > threaded_buffer;

};

extern "C" mem_trace_reader* init_trace_reader(const char* filename) 
{
  mem_trace_reader* retval = new mem_trace_reader(filename);
  return retval;
}

extern "C" void destroy_mem_trace_reader(mem_trace_reader* mtr) 
{
  delete mtr;
}

extern "C" int get_next_access(mem_trace_reader* mtr, extended_info* ei) 
{
  return mtr->get_next_access(*ei);
}

extern "C" int buffer_threads(mem_trace_reader* mtr, int num_threads, int num_insts) 
{
  return mtr->buffer_threads(num_threads, num_insts);
}

extern "C" int get_next_thread_access(mem_trace_reader* mtr, int thread, extended_info* ei) 
{
  return mtr->get_next_access(thread, *ei);
}

mem_trace_reader::mem_trace_reader(char* filename)
{
  infile = gzopen(filename, "rb");
}

mem_trace_reader::~mem_trace_reader()
{
  gzclose(infile);
}

bool mem_trace_reader::get_next_access(extended_info& ei)
{
  int nread = 0;
  unsigned interval = 0;
  stream_entry se1;
  access_info ai1;
  int temp;
  while (true) {
    nread = gzread(infile, &se1, sizeof(stream_entry));
    if (nread < sizeof(stream_entry)) {return false;}
    if (se1.code == thread_change) {
      interval += se1.interval;
      nread = gzread(infile, &temp, sizeof(int)); 
      if (nread < sizeof(int)) {return false;}
   } else if (se1.code == skip) {
      interval += se1.interval;
    } else {
      interval += se1.interval;
      nread = gzread(infile, &ai1, sizeof(access_info));
      if (nread < sizeof(access_info)) {return false;}
      ei.interval = interval;
      ei.read = (se1.code == mem_read) ? 1 : 0;
      ei.ai = ai1;
      return true;
    }
  }
}

int mem_trace_reader::buffer_threads(int num_threads, int num_insts)
{
  int nread;
  stream_entry se1;
  access_info ai1;
  extended_info ei;
  int temp;
  threaded_buffer.resize(num_threads);
  for (int thi = 0; thi < num_threads; ++thi) {
    int insti = 0;
    // enqueue instructions for a HW thread
    while (insti < num_insts) {
      unsigned interval = 0;
      while (true) {
	nread = gzread(infile, &se1, sizeof(stream_entry));
	if (nread < sizeof(stream_entry)) {return thi;}
	if (se1.code == thread_change) {
	  // new SW thread
	  interval += se1.interval;
	  nread = gzread(infile, &temp, sizeof(int));
	  if (nread < sizeof(int)) {return thi;}
	} else if (se1.code == skip) {
	  interval += se1.interval;
	} else {
	  interval += se1.interval;
	  nread = gzread(infile, &ai1, sizeof(access_info));
	  if (nread < sizeof(access_info)) {return thi;}
	  ei.interval = interval;
	  ei.read = (se1.code == mem_read) ? 1 : 0;
	  ei.ai = ai1;
	  break;
	}
      }
      // enqueue access info
      threaded_buffer[thi].push_back(ei);
      insti += interval;
    }
    // find the next thread boundary
    while (true) {
      nread = gzread(infile, &se1, sizeof(stream_entry));
      if (nread < sizeof(stream_entry)) {
	return thi;
      }
      if (se1.code == thread_change) {
	nread = gzread(infile, &temp, sizeof(int));
	if (nread < sizeof(int)) {return thi;}
	break;
      } else if (se1.code != skip) {
	nread = gzread(infile, &ai1, sizeof(access_info));
	if (nread < sizeof(access_info)) {return thi;}
      }
    }
  }
  return num_threads;
}

bool mem_trace_reader::get_next_access(int thread, extended_info& ei)
{
  if (threaded_buffer.size() > thread &&
      threaded_buffer[thread].size() > 0) {
    ei = threaded_buffer[thread].front();
    threaded_buffer[thread].pop_front();
    return true;
  } else {
    return false;
  }
}

int buffer_one_sw_thread(struct mem_trace_reader* mtr, int hw_thread)
{
  int nread;
  struct stream_entry se1;
  struct access_info ai1;
  struct extended_info ei;
  int temp;
  int interval;
  struct extended_info_q* newbuf;

  //NOTE: This code should never be used
  if (mtr->thread_count <= hw_thread) {
    newbuf = (struct extended_info_q*)calloc(sizeof(struct extended_info_q), hw_thread+1);
    if (mtr->thread_count) {
      // copy over old buffer
      memcpy(newbuf, mtr->threaded_buffer, mtr->thread_count * sizeof(struct extended_info_q));
      // free old buffer
      free(mtr->threaded_buffer);
    }
    mtr->threaded_buffer = newbuf;
    mtr->thread_count = hw_thread+1;
  }

  // enqueue instructions for one SW thread
  while (1) {
    nread = gzread(mtr->infile, &se1, sizeof(struct stream_entry));
    if (nread < sizeof(struct stream_entry)) {return interval;}
    if (se1.code == thread_change) {
      // new SW thread
      nread = gzread(mtr->infile, &temp, sizeof(int));
      if (nread < sizeof(int)) {return interval;}
      return interval;
    } else if (se1.code == skip) {
      interval += se1.interval;
    } else {
      interval += se1.interval;
      nread = gzread(mtr->infile, &ai1, sizeof(struct access_info));
      if (nread < sizeof(struct access_info)) {return interval;}
      ei.interval = se1.interval;
      ei.read = (se1.code == mem_read) ? 1 : 0;
      ei.ai = ai1;
      push_back(&mtr->threaded_buffer[hw_thread], &ei);
      break;
    }
  }
  return interval;
}

