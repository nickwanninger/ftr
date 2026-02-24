# ftr

A minimal [Fuchsia FXT](https://fuchsia.dev/fuchsia-src/reference/tracing/trace-format) trace writer for C/C++. Drop `ftr.c` and `ftr.h` into your build system and start tracing.
Traces can be viewed in [Perfetto](https://ui.perfetto.dev).

On x86, `ftr` uses rdtscp for timestamps after calibration, and falls back to clock_gettime(CLOCK_MONOTONIC) on other platforms (rough nanosecond resolution).

## Usage

```c
#include "ftr.h"

void my_function() {
    FTR_SCOPE("my_function");   // records a duration span for this scope
    // ... work ...
}
```

Tracing starts automatically on program init and the output goes to `trace.fxt.gz` in the working directory.

## Environment variables

- `FTR_TRACE_PATH`: Override the output file path (supports `.gz` extension for compressed output (requires `gzip` command to be installed))
- `FTR_DISABLE`: Set to any value to disable tracing entirely at runtime.

## Disabling at compile time

Define `FTR_NO_TRACE` as a preprocessor macro to compile out all macros with zero overhead.
