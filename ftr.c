#include "./ftr.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FXT_MAGIC 0x0016547846040010ULL
// Initialization record  (type = 1, always 2 words)
//
//  Word 0 — header:
//   [3:0]   type        = 1
//   [15:4]  size_words  = 2
//   [63:16] _reserved
//  Word 1 — ticks_per_second (we use nanoseconds, so 1 000 000 000)
typedef union {
  struct {
    uint64_t type : 4;        // = 1
    uint64_t size_words : 12; // = 2
    uint64_t _reserved : 48;
  };
  uint64_t raw;
} fxt_init_hdr;

// String record  (type = 2)
typedef union {
  struct {
    uint64_t type : 4;
    uint64_t size_words : 12;
    uint64_t str_index : 15;
    uint64_t _r0 : 1;
    uint64_t str_len : 15;
    uint64_t _r1 : 17;
  };
  uint64_t raw;
} fxt_string_hdr;

// Event record  (type = 4)
typedef union {
  struct {
    uint64_t type : 4;
    uint64_t size_words : 12;
    uint64_t event_type : 4;
    uint64_t arg_count : 4;
    uint64_t thread_ref : 8;
    uint64_t category_ref : 16;
    uint64_t name_ref : 16;
  };
  uint64_t raw;
} fxt_event_hdr;

#define FXT_MAX_STRINGS 0x7FFF // max unique interned strings
#define FXT_STRING_MAXLEN 63   // max string length in bytes

typedef struct {
  const char *key;
} ftr_intern_entry_t;

static ftr_intern_entry_t intern_pool[FXT_MAX_STRINGS];
static uint16_t intern_count = 0;

#define FTR_SHARED_BUF_SIZE (256 * 1024 * 1024) // 256 KB

static FILE *out_stream = NULL;
static uint8_t shared_buf[FTR_SHARED_BUF_SIZE];
static size_t shared_buf_pos = 0;
static atomic_flag shared_buf_lock = ATOMIC_FLAG_INIT;

static inline void buf_lock(void) {
  while (atomic_flag_test_and_set_explicit(&shared_buf_lock,
                                           memory_order_acquire)) {
  }
}

static inline void buf_unlock(void) {
  atomic_flag_clear_explicit(&shared_buf_lock, memory_order_release);
}

// Must be called with the lock held.
static void flush_locked(void) {
  if (shared_buf_pos > 0) {
    fwrite(shared_buf, 1, shared_buf_pos, out_stream);
    shared_buf_pos = 0;
  }
}

// Append `len` bytes from `data` into the shared buffer, flushing first if
// there isn't enough room. Must be called with the lock held.
static void buf_append_locked(const void *data, size_t len) {
  if (shared_buf_pos + len > FTR_SHARED_BUF_SIZE) {
    flush_locked();
  }
  memcpy(shared_buf + shared_buf_pos, data, len);
  shared_buf_pos += len;
}

// ---------------------------------------------------------------------------
// Record-local staging helpers — build into a small stack buffer, then commit
// ---------------------------------------------------------------------------

typedef struct {
  uint8_t data[512]; // max FXT record size we'll ever produce
  size_t pos;
} ftr_record_t;

static inline void rec_u64(ftr_record_t *r, uint64_t v) {
  uint8_t b[8] = {
      (uint8_t)(v >> 0),  (uint8_t)(v >> 8),  (uint8_t)(v >> 16),
      (uint8_t)(v >> 24), (uint8_t)(v >> 32), (uint8_t)(v >> 40),
      (uint8_t)(v >> 48), (uint8_t)(v >> 56),
  };
  memcpy(r->data + r->pos, b, 8);
  r->pos += 8;
}

static inline void rec_str_padded(ftr_record_t *r, const char *s, size_t len) {
  size_t pad = (8 - len % 8) % 8;
  memcpy(r->data + r->pos, s, len);
  r->pos += len;
  if (pad) {
    memset(r->data + r->pos, 0, pad);
    r->pos += pad;
  }
}

static void commit_record(ftr_record_t *r) {
  if (!out_stream)
    return;
  buf_lock();
  buf_append_locked(r->data, r->pos);
  buf_unlock();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ftr_debug_dump(void) {}

static int out_stream_is_pipe = 0;

static void ftr_open(const char *trace_path) {
  assert(out_stream == NULL && "ftr_open called while already open");

  intern_count = 0;

  size_t len = strlen(trace_path);
  if (len > 3 && strcmp(trace_path + len - 3, ".gz") == 0) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "gzip > '%s'", trace_path);
    out_stream = popen(cmd, "w");
    out_stream_is_pipe = 1;
  } else {
    out_stream = fopen(trace_path, "wb");
    out_stream_is_pipe = 0;
  }

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, FXT_MAGIC);

  fxt_init_hdr init = {0};
  init.type = 1;
  init.size_words = 2;
  rec_u64(&r, init.raw);
  rec_u64(&r, 1000000000ULL); // ticks_per_sec = 1ns

  // Write directly — no other threads are active yet.
  fwrite(r.data, 1, r.pos, out_stream);
}

__attribute__((constructor)) static void ftr_init(void) {
  if (getenv("FTR_DISABLE"))
    return;
  const char *path = getenv("FTR_TRACE_PATH");
  ftr_open(path ? path : "trace.fxt.gz");
}

void ftr_close() {
  buf_lock();
  flush_locked();
  buf_unlock();
  if (out_stream_is_pipe)
    pclose(out_stream);
  else
    fclose(out_stream);
  out_stream = NULL;
}

__attribute__((destructor)) static void on_exit(void) {
  if (out_stream)
    ftr_close();
}

static ftr_timestamp_t ftr_start_time_ns = 0;

ftr_timestamp_t ftr_now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

  ftr_timestamp_t now =
      (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  if (ftr_start_time_ns == 0) {
    ftr_start_time_ns = now - 10;
  }
  uint64_t t = now - ftr_start_time_ns;

  // Ensure each nanosecond measurement is unique, even if the clock doesn't
  // have nanosecond precision. This is a bit of a hack but it prevents issues
  // with nonsense zero-duration spans and strange ordering in perfetto.
  static uint64_t last = 0;
  if (t <= last)
    t = last + 1;
  last = t;
  return t;
}

void ftr_write_span(uint64_t pid, uint64_t tid, const char *name,
                    ftr_timestamp_t start_ns, ftr_timestamp_t end_ns) {

  const char *cat = "app";
  size_t cat_len = 3;
  size_t name_len = strlen(name);
  size_t cat_words = (cat_len + 7) / 8;
  size_t name_words = (name_len + 7) / 8;
  size_t size_words = 1 + 3 + name_words + cat_words + 1;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 4;
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = (uint16_t)(0x8000 | name_len);
  ev.category_ref = (uint16_t)(0x8000 | cat_len);

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, start_ns);
  rec_u64(&r, pid);
  rec_u64(&r, tid);
  rec_str_padded(&r, cat, cat_len);
  rec_str_padded(&r, name, name_len);
  rec_u64(&r, end_ns);

  commit_record(&r);
}

uint16_t ftr_intern_string(const char *s) {
  if (!out_stream)
    return 0;
  for (uint16_t i = 0; i < intern_count; i++) {
    if (intern_pool[i].key == s)
      return i + 1;
  }

  assert(intern_count < FXT_MAX_STRINGS && "intern table full");

  size_t len = strlen(s);
  if (len > FXT_STRING_MAXLEN)
    len = FXT_STRING_MAXLEN;

  uint16_t idx = ++intern_count;
  intern_pool[idx - 1].key = s;

  size_t str_words = (len + 7) / 8;
  fxt_string_hdr sh = {0};
  sh.type = 2;
  sh.size_words = 1 + str_words;
  sh.str_index = idx;
  sh.str_len = (uint64_t)len;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, sh.raw);
  rec_str_padded(&r, s, len);
  commit_record(&r);

  return idx;
}

void ftr_write_spani(uint16_t name_ref, ftr_timestamp_t start_ns,
                     ftr_timestamp_t end_ns) {

  uint64_t pid = getpid();
  uint64_t tid = (uint64_t)pthread_self();
  size_t size_words = 1 + 3 + 1;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 4;
  ev.arg_count = 0;
  ev.thread_ref = 0;
  ev.name_ref = name_ref;
  ev.category_ref = 0;

  ftr_record_t r = {.pos = 0};
  rec_u64(&r, ev.raw);
  rec_u64(&r, start_ns);
  rec_u64(&r, pid);
  rec_u64(&r, tid);
  rec_u64(&r, end_ns);

  commit_record(&r);
}
