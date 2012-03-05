#include "read_mem_trace_c.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <stdio.h>

struct extended_info_q_el {
  struct extended_info elem;
  struct extended_info_q_el* next;
};

struct extended_info_q {
  int size;
  struct extended_info_q_el* front;
  struct extended_info_q_el* back;
};

struct mem_trace_reader {
  gzFile infile;
  struct extended_info_q* threaded_buffer; // array of queues
  int thread_count;
};

void push_back(struct extended_info_q* eiq, struct extended_info* ei)
{
  struct extended_info_q_el* newel = (struct extended_info_q_el*)malloc(sizeof(struct extended_info_q_el));
  memcpy(&newel->elem, ei, sizeof(struct extended_info));
  newel->next = 0;
  if (eiq->back) {
    eiq->back->next = newel;
  } else {
    eiq->front = newel;
  }
  eiq->back = newel;
}

void pop_front(struct extended_info_q* eiq)
{
  struct extended_info_q_el* front = eiq->front;
  if (front == 0) { return; }
  if (eiq->back == eiq->front) {
    eiq->back = 0;
  }
  eiq->front = front->next;
  free(front);
}

struct mem_trace_reader* init_trace_reader(const char* filename)
{
  struct mem_trace_reader* mtr = (struct mem_trace_reader*)malloc(sizeof(struct mem_trace_reader));
  mtr->infile = gzopen(filename, "rb");
  mtr->threaded_buffer = 0;
  mtr->thread_count = 0;
  buffer_a_thread_max(mtr, 0, 1);
  return mtr;
}

void destroy_mem_trace_reader(struct mem_trace_reader* mtr)
{
  int i;
  struct extended_info_q_el* qelp;
  struct extended_info_q_el* qlast = 0;
  gzclose(mtr->infile);
  for (i = 0; i < mtr->thread_count; ++i) {
    // free everything in threaded buffer
    for (qelp = mtr->threaded_buffer[i].front;
	 qelp != 0; qelp = qelp->next) {
      if (qlast) {
        free(qlast);
        qlast=NULL;
      }
      qlast = qelp;
    }
    if (qlast) {
      free(qlast);
      qlast=NULL;
    }
  }
  free(mtr->threaded_buffer);
  mtr->threaded_buffer = NULL;
  free(mtr);
  mtr = NULL;
}

int get_next_access(struct mem_trace_reader* mtr, struct extended_info* ei)
{
  int nread = 0;
  unsigned interval = 0;
  struct stream_entry se1;
  struct access_info ai1;
  int temp;
  while (1) {
    nread = gzread(mtr->infile, &se1, sizeof(struct stream_entry));
    if (nread < sizeof(struct stream_entry)) {return 0;}
    if (se1.code == thread_change) {
      interval += se1.interval;
      nread = gzread(mtr->infile, &temp, sizeof(int)); 
      if (nread < sizeof(int)) {return 0;}
   } else if (se1.code == skip) {
      interval += se1.interval;
    } else {
      interval += se1.interval;
      nread = gzread(mtr->infile, &ai1, sizeof(struct access_info));
      if (nread < sizeof(struct access_info)) {return 0;}
      ei->interval = interval;
      ei->read = (se1.code == mem_read) ? 1 : 0;
      ei->ai = ai1;
      return 1;
    }
  }
}

int buffer_threads_min(struct mem_trace_reader* mtr, int num_hw_threads, int min_insts)
{
  int nread;
  struct stream_entry se1;
  struct access_info ai1;
  struct extended_info ei;
  int temp;
  int thi;
  int insti, interval;
  struct extended_info_q* newbuf;
  if (mtr->thread_count < num_hw_threads) {
    newbuf = (struct extended_info_q*)calloc(sizeof(struct extended_info_q), num_hw_threads);
    if (mtr->thread_count) {
      // copy over old buffer
      memcpy(newbuf, mtr->threaded_buffer, mtr->thread_count * sizeof(struct extended_info_q));
      // free old buffer
      free(mtr->threaded_buffer);
    }
    mtr->threaded_buffer = newbuf;
    mtr->thread_count = num_hw_threads;
  }
  for (thi = 0; thi < num_hw_threads; ++thi) {
    insti = 0;
    // enqueue instructions for a HW thread
    while (insti < min_insts) {
      interval = 0;
      while (1) {
	nread = gzread(mtr->infile, &se1, sizeof(struct stream_entry));
	if (nread < sizeof(struct stream_entry)) {return thi;}
	if (se1.code == thread_change) {
	  // new SW thread
	  interval += se1.interval;
	  nread = gzread(mtr->infile, &temp, sizeof(int));
	  if (nread < sizeof(int)) {return thi;}
	} else if (se1.code == skip) {
	  interval += se1.interval;
	} else {
	  interval += se1.interval;
	  nread = gzread(mtr->infile, &ai1, sizeof(struct access_info));
	  if (nread < sizeof(struct access_info)) {return thi;}
	  ei.interval = interval;
	  ei.read = (se1.code == mem_read) ? 1 : 0;
	  ei.ai = ai1;
	  break;
	}
      }
      // enqueue access info
      push_back(&mtr->threaded_buffer[thi], &ei);
      insti += interval;
    }
    // find the next thread boundary
    while (1) {
      nread = gzread(mtr->infile, &se1, sizeof(struct stream_entry));
      if (nread < sizeof(struct stream_entry)) {
	return thi;
      }
      if (se1.code == thread_change) {
	nread = gzread(mtr->infile, &temp, sizeof(int));
	if (nread < sizeof(int)) {return thi;}
	break;
      } else if (se1.code != skip) {
	nread = gzread(mtr->infile, &ai1, sizeof(struct access_info));
	if (nread < sizeof(struct access_info)) {return thi;}
      }
    }
  }
  return num_hw_threads;
}

int buffer_threads_max(struct mem_trace_reader* mtr, int num_hw_threads, int max_insts)
{
  int nread;
  struct stream_entry se1;
  struct access_info ai1;
  struct extended_info ei;
  int temp;
  int thi;
  int insti, interval;
  struct extended_info_q* newbuf;
  if (mtr->thread_count < num_hw_threads) {
    newbuf = (struct extended_info_q*)calloc(sizeof(struct extended_info_q), num_hw_threads);
    if (mtr->thread_count) {
      // copy over old buffer
      memcpy(newbuf, mtr->threaded_buffer, mtr->thread_count * sizeof(struct extended_info_q));
      // free old buffer
      free(mtr->threaded_buffer);
    }
    mtr->threaded_buffer = newbuf;
    mtr->thread_count = num_hw_threads;
  }
  for (thi = 0; thi < num_hw_threads; ++thi) {
    insti = buffer_a_thread_max(mtr, thi, max_insts);
    if (insti == 0) { break; }
  }
  return thi;
}

int buffer_one_sw_thread(struct mem_trace_reader* mtr, int hw_thread)
{
  int nread;
  struct stream_entry se1;
  struct access_info ai1;
  struct extended_info ei;
  int temp;
  int interval = 0;
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

int buffer_a_thread_max(struct mem_trace_reader* mtr, int hw_thread, int max_insts)
{
  int nread;
  struct stream_entry se1;
  struct access_info ai1;
  struct extended_info ei;
  int temp;
  int insti, interval;
  struct extended_info_q* newbuf;
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
  insti = 0;
  // enqueue instructions for a HW thread
  while (insti < max_insts) {
    interval = 0;
    while (1) {
      nread = gzread(mtr->infile, &se1, sizeof(struct stream_entry));
      if (nread < sizeof(struct stream_entry)) {return insti;}
      if (se1.code == thread_change) {
	// new SW thread
	nread = gzread(mtr->infile, &temp, sizeof(int));
	if (nread < sizeof(int)) {return insti;}
	return insti;
      } else if (se1.code == skip) {
	interval += se1.interval;
      } else {
	interval += se1.interval;
	nread = gzread(mtr->infile, &ai1, sizeof(struct access_info));
	if (nread < sizeof(struct access_info)) {return insti;}
	ei.interval = interval;
	ei.read = (se1.code == mem_read) ? 1 : 0;
	ei.ai = ai1;
	break;
      }
    }
    // enqueue access info
    push_back(&mtr->threaded_buffer[hw_thread], &ei);
    insti += interval;
  }
  // find the next thread boundary
  while (1) {
    nread = gzread(mtr->infile, &se1, sizeof(struct stream_entry));
    if (nread < sizeof(struct stream_entry)) {
      return insti;
    }
    if (se1.code == thread_change) {
      nread = gzread(mtr->infile, &temp, sizeof(int));
      if (nread < sizeof(int)) {return insti;}
      break;
    } else if (se1.code != skip) {
      nread = gzread(mtr->infile, &ai1, sizeof(struct access_info));
      if (nread < sizeof(struct access_info)) {return insti;}
    }
  }
  return insti;
}

int get_next_thread_access(struct mem_trace_reader* mtr, int thread, struct extended_info* ei)
{
  if (mtr->thread_count > thread &&
      mtr->threaded_buffer[thread].front != 0) {
    memcpy(ei, &mtr->threaded_buffer[thread].front->elem, sizeof(struct extended_info));
    pop_front(&mtr->threaded_buffer[thread]);
    return 1;
  } else {
    return 0;
  }
}
