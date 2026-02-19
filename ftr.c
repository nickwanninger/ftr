#include "./ftr.h"
#include <assert.h>
#include <stdio.h>
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
//
//  Word 0 — header:
//   [3:0]   type        = 2
//   [15:4]  size_words  = 1 + ⌈str_len / 8⌉
//   [30:16] str_index   1-based; events reference strings by this index
//   [31]    _reserved
//   [46:32] str_len     byte length of the string (without padding)
//   [63:47] _reserved
//  Words 1..N — UTF-8 string bytes, zero-padded to the next 8-byte boundary
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
// We only emit DurationComplete (event_type = 4): a single record that
// encodes both the begin and end of a span — no matching begin/end pairs
// needed.
//
//  Word 0 — header:
//   [3:0]   type         = 4
//   [15:4]  size_words   = 3  (header + start_ts + end_ts, no args)
//   [19:16] event_type   = 4  (DurationComplete)
//   [23:20] arg_count    = 0
//   [31:24] thread_ref   1-based index into the thread table
//   [47:32] category_ref 1-based index into the string table
//   [63:48] name_ref     1-based index into the string table
//  Word 1 — begin timestamp (nanoseconds)
//  Word 2 — end   timestamp (nanoseconds)
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

#define FXT_MAX_STRINGS 256  // max unique interned strings
#define FXT_STRING_MAXLEN 63 // max string length in bytes

// Debug buffer: stores event data for printing at the end
#define DEBUG_BUFFER_SIZE (1024 * 1024) // 1MB buffer
typedef struct {
  const char *name;
  ftr_timestamp_t start_ns;
  ftr_timestamp_t end_ns;
} debug_event_t;

static debug_event_t debug_buffer[DEBUG_BUFFER_SIZE / sizeof(debug_event_t)];
static size_t debug_buffer_count = 0;

static inline void debug_event_record(const char *name,
                                      ftr_timestamp_t start_ns,
                                      ftr_timestamp_t end_ns) {
  if (debug_buffer_count >= (DEBUG_BUFFER_SIZE / sizeof(debug_event_t))) {
    return; // Buffer full, silently drop
  }
  debug_buffer[debug_buffer_count].name = name;
  debug_buffer[debug_buffer_count].start_ns = start_ns;
  debug_buffer[debug_buffer_count].end_ns = end_ns;
  debug_buffer_count++;
}

static FILE *out_stream = NULL;

static inline void fxt_write_u64(FILE *f, uint64_t v) {
  // Write in little-endian byte order regardless of host endianness
  uint8_t b[8] = {
      (uint8_t)(v >> 0),  (uint8_t)(v >> 8),  (uint8_t)(v >> 16),
      (uint8_t)(v >> 24), (uint8_t)(v >> 32), (uint8_t)(v >> 40),
      (uint8_t)(v >> 48), (uint8_t)(v >> 56),
  };
  fwrite(b, 1, 8, f);
}

static const uint8_t zeros[8] = {0};
static inline void fxt_write_str_padded(FILE *f, const char *s, size_t len) {
  size_t pad = (8 - len % 8) % 8;
  fwrite(s, 1, len, f);
  if (pad) {
    fwrite(zeros, 1, pad, f);
  }
}

void ftr_open(const char *trace_path) {
  uint64_t ticks_per_sec = 1000000000ULL;
  assert(out_stream == NULL && "ftr_open called while already open");

  out_stream = fopen(trace_path, "wb");
  setvbuf(out_stream, NULL, _IOFBF, 65536);

  fxt_write_u64(out_stream, FXT_MAGIC);

  // Initialization: declare 1 tick = 1 nanosecond
  fxt_init_hdr init = {0};
  init.type = 1;
  init.size_words = 2;
  fxt_write_u64(out_stream, init.raw);
  fxt_write_u64(out_stream, ticks_per_sec);
}

void ftr_close() {
  ftr_debug_dump();
  fclose(out_stream);
  out_stream = NULL;
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

  static uint64_t last = 0;
  if (t <= last)
    t = last + 1;
  last = t;
  return t;
}

void ftr_write_span(uint64_t pid, uint64_t tid, const char *name,
                    ftr_timestamp_t start_ns, ftr_timestamp_t end_ns) {
  debug_event_record(name, start_ns, end_ns);

  const char *cat = "app";
  size_t cat_len = 3;
  size_t cat_words = (cat_len + 7) / 8;

  size_t name_len = strlen(name);
  size_t name_words = (name_len + 7) / 8;

  // size = 1 (header) + 2 (koids) +  name_words + 2 (timestamps)
  size_t size_words = 1 + 3 + name_words + cat_words + 1;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 4; // DurationComplete
  ev.arg_count = 0;
  ev.thread_ref = 0; // 0 = inline thread
  ev.name_ref = (uint16_t)(0x8000 | name_len);
  // ev.category_ref = 0;
  ev.category_ref = (uint16_t)(0x8000 | cat_len);
  FILE *out = out_stream;
  fxt_write_u64(out, ev.raw);
  fxt_write_u64(out, start_ns); // Timestamp word.
  fxt_write_u64(out, pid);
  fxt_write_u64(out, tid);
  fxt_write_str_padded(out, cat, cat_len);
  fxt_write_str_padded(out, name, name_len);
  fxt_write_u64(out, end_ns);
}

uint16_t ftr_intern_string(const char *s) {
  //
  return 0;
}

void ftr_write_spani(uint16_t name_ref, ftr_timestamp_t start_ns,
                     ftr_timestamp_t end_ns) {


  // TODO: cache an interned pid/tid pair for the current thread!
  uint64_t pid = getpid();
  uint64_t tid = (uint64_t)pthread_self();

  // size = 1 (header) + 2 (koids) + 2 (timestamps)
  size_t size_words = 1 + 3 + 1;

  fxt_event_hdr ev = {0};
  ev.type = 4;
  ev.size_words = (uint64_t)size_words;
  ev.event_type = 4; // DurationComplete
  ev.arg_count = 0;
  ev.thread_ref = 0; // 0 = inline thread
  ev.name_ref = name_ref;
  ev.category_ref = 0;
  FILE *out = out_stream;
  fxt_write_u64(out, ev.raw);
  fxt_write_u64(out, start_ns); // Timestamp word.
  fxt_write_u64(out, pid);
  fxt_write_u64(out, tid);
  fxt_write_u64(out, end_ns);
}
