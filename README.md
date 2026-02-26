# ftr

A minimal [Fuchsia FXT](https://fuchsia.dev/fuchsia-src/reference/tracing/trace-format) trace writer for C/C++. Drop `src/ftr.c` and `src/ftr.h` into your build system, or build as a shared library. Traces can be viewed in [Perfetto](https://ui.perfetto.dev).

<p align="center"><a href="#api">Jump to API docs</a></p>

On x86, `ftr` uses rdtscp for timestamps after calibration, and has incredibly low overhead.
On other platforms, it falls back to the more expensive `clock_gettime(CLOCK_MONOTONIC)` (rough nanosecond resolution) on other platforms (Aarch64).

## Building

```sh
make                          # configure and build
make install                  # install to /usr/local
make install PREFIX=~/.local  # install to a custom prefix
make clean                    # remove build directory
```

To skip building examples, pass the CMake option directly:

```sh
cmake -B build -DFTR_BUILD_EXAMPLES=OFF
cmake --build build
```

### Using with FetchContent

Add ftr to your project as a static library with no examples:

```cmake
include(FetchContent)
FetchContent_Declare(ftr
  GIT_REPOSITORY https://github.com/nickwanninger/ftr.git
  GIT_TAG        main)
set(FTR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftr)

target_link_libraries(myapp PRIVATE ftr_static)
```

## Usage

```c
#include <ftr.h>

void my_function() {
    FTR_SCOPE("my_function");   // records a duration span for this scope
    // ... work ...
}
```

By default, tracing does **not** start automatically. Use `ftr_init_file()` or `ftr_init()` to start it explicitly, or set `FTR_TRACE_PATH` to auto-initialize on startup.

```c
// Write to a file (NULL falls back to FTR_TRACE_PATH env var, then "trace.fxt.gz")
ftr_init_file("my_trace.fxt");

// Or supply your own callback for custom output (network socket, in-memory buffer, etc.)
ftr_init(my_write_fn, my_userdata);
```

## API

### Scopes

- **`FTR_SCOPE(name)`** — Records a duration span from this point to the end of the enclosing block. The name _must_ be a `const char *` (static string).
- **`FTR_FUNCTION()`** — Shorthand for `FTR_SCOPE(__PRETTY_FUNCTION__)`.

### Expression tracing

- **`FTR_EXPR(name, expr)`** — Wraps the evaluation of `expr` in a duration span and returns its value. Use this to trace a single expression inline without introducing a new scope block.

```c
// Trace just the syscall, capture its return value
int ret = FTR_EXPR("usleep", usleep(100));

// Works with any expression type
size_t n = FTR_EXPR("fread", fread(buf, 1, sizeof(buf), fp));
```

> **Note:** Requires GCC or Clang — uses the `({ ... })` statement expression extension. This is not standard C99 but is universally supported by the compilers ftr targets.

### Marks and counters

- **`FTR_MARK(name)`** — Emits an instant event (a single point in time).
- **`FTR_COUNTER(name, value)`** — Records a counter sample. Displayed as a stacked area chart in Perfetto.

### Logging

- **`ftr_logf(fmt, ...)`** — printf-style instant event with a formatted message. Higher overhead (~100ns) than other macros.

### Flow events

Flow events draw arrows between duration events across threads in Perfetto, showing causal relationships (e.g. producer → consumer).

Each of these works like `FTR_SCOPE` — they create a duration span that lasts until the end of the enclosing block — but additionally emit a flow event anchored to that span.

- **`FTR_SCOPE_FLOW_BEGIN(name, flow_id)`** — Begins a flow. Place this where work is initiated.
- **`FTR_SCOPE_FLOW_STEP(name, flow_id)`** — Marks an intermediate step in a flow.
- **`FTR_SCOPE_FLOW_END(name, flow_id)`** — Ends a flow. Place this where work is consumed.

The `flow_id` can be any `uint64_t` or pointer — the macro casts automatically. Use `ftr_new_flow_id()` to generate a unique ID, or pass a pointer to the work item itself:

```c
// Producer thread
void enqueue(work_item *item) {
    FTR_SCOPE_FLOW_BEGIN("work", item);  // pointer as flow ID
    queue_push(item);
}

// Consumer thread
void process(work_item *item) {
    FTR_SCOPE_FLOW_END("work", item);    // same pointer completes the arrow
    do_work(item);
}
```

## Environment variables

- `FTR_TRACE_PATH`: If set at startup, auto-initializes tracing to that file path. Supports `.gz` extension for gzip-compressed output (requires `gzip` on `$PATH`).
- `FTR_DISABLE`: Set to any value to disable tracing entirely at runtime.

## Disabling at compile time

Define `FTR_NO_TRACE` as a preprocessor macro to compile out all macros with zero overhead.
