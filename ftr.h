#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// FTR is a simple trace writer "Function TRace" that produces fuscia ftx trace
// files from a simple API. It is designed to be used in high performance code
// with many threads without a huge overhead.

extern void ftr_open(const char *trace_path);
extern void ftr_close();
extern void ftr_debug_dump(void);

// An FXT trace atom.
typedef uint64_t ftr_atom_t;
typedef uint64_t ftr_timestamp_t;
typedef uint8_t ftr_thread_t; // 1-based thread index
typedef uint16_t ftr_str_t;   // 1-based string index

extern void ftr_write_span(uint64_t pid, uint64_t tid, const char *name,
                           ftr_timestamp_t start_ns, ftr_timestamp_t end_ns);

// Nanosecond timestamp from a monotonic clock.
extern ftr_timestamp_t ftr_now_ns(void);

struct ftr_event_t {
  const char *name_ref;
  ftr_timestamp_t start_ns;
};

static inline struct ftr_event_t ftr_begin_event(const char *name) {
  struct ftr_event_t e = {name, ftr_now_ns()};
  return e;
}

static inline void ftr_end_event(struct ftr_event_t *e) {
  ftr_timestamp_t end_ns = ftr_now_ns();
  uint64_t pid = getpid();
  uint64_t tid = (uint64_t)pthread_self();
  ftr_write_span(0, 0, e->name_ref, e->start_ns, end_ns);
}

#define FTR_CONCAT_(a, b) a##b
#define FTR_CONCAT(a, b) FTR_CONCAT_(a, b)
#define FTR_SCOPE(name)                                                        \
  __attribute__((cleanup(ftr_end_event))) struct ftr_event_t FTR_CONCAT(       \
      __event_, __LINE__) = ftr_begin_event(name)

#ifdef __cplusplus

struct FtrScope {
  const char *name_ref;
  ftr_timestamp_t start_ns;

  FtrScope(const char *name) : name_ref(name), start_ns(ftr_now_ns()) {}
  ~FtrScope() {
    ftr_timestamp_t end_ns = ftr_now_ns();
    uint64_t pid = getpid();
    uint64_t tid = (uint64_t)pthread_self();
    ftr_write_span(0, 0, name_ref, start_ns, end_ns);
  }
};
}
#endif