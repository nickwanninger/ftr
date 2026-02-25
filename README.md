# ftr

A minimal [Fuchsia FXT](https://fuchsia.dev/fuchsia-src/reference/tracing/trace-format) trace writer for C/C++. Drop `src/ftr.c` and `src/ftr.h` into your build system, or build as a shared library. Traces can be viewed in [Perfetto](https://ui.perfetto.dev).

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

## Usage

```c
#include <ftr.h>

void my_function() {
    FTR_SCOPE("my_function");   // records a duration span for this scope
    // ... work ...
}
```

Tracing starts automatically on program init and the output goes to `trace.fxt.gz` in the working directory.

## API

### Scopes

- **`FTR_SCOPE(name)`** — Records a duration span from this point to the end of the enclosing block. The name is interned on first use.
- **`FTR_FUNCTION()`** — Shorthand for `FTR_SCOPE(__PRETTY_FUNCTION__)`.

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

- `FTR_TRACE_PATH`: Override the output file path (supports `.gz` extension for compressed output (requires `gzip` command to be installed))
- `FTR_DISABLE`: Set to any value to disable tracing entirely at runtime.

## Disabling at compile time

Define `FTR_NO_TRACE` as a preprocessor macro to compile out all macros with zero overhead.
