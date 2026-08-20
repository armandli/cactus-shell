# cactus-shell

A shell written in C++23 that takes natural-language English and runs the corresponding
commands. Translation is done by the Needle model via the cactus inference engine.

## Commands

```bash
cmake -S . -B build -DCACTUS_ROOT=/path/to/cactus   # configure
cmake --build build -j4                              # build
ctest --test-dir build --output-on-failure           # test
./build/cactus /path/to/weights                      # run
```

## Dependencies

- **cactus engine** — required at link time, provides the C symbols in `needle_ffi.h`.
  Build from https://github.com/cactus-compute/cactus and pass `-DCACTUS_ROOT`.
  **Its kernels are compiled with `-march=armv8.2-a`, so it only builds on arm64.**
  On x86_64 the configure step fails by design.
- **simdjson** and **GoogleTest** — fetched automatically via `FetchContent` on the first
  configure (needs network). Both prefer a system install if one exists.

`needle_ffi.h` redeclares the handful of cactus C functions we call instead of including
`<cactus_engine.h>`. That upstream header pulls in `cactus_graph.h`, which includes
`<arm_neon.h>` unconditionally and declares its own C++ `namespace cactus` that collides
with ours. Keep those declarations byte-compatible with upstream.

## Layout

- `src/` — all source. `cactus_core` (static lib) holds every testable piece; `main.cpp`
  is a thin wrapper that only constructs `Shell` and calls `run()`. New modules go in
  `src/` and must be added to the `cactus_core` sources list in `src/CMakeLists.txt`.
  - `json_util.{h,cpp}` — `JsonBuilder` writes JSON (separators handled automatically),
    `JsonDoc::parse` reads it. Both report failure through `std::expected<T, JsonError>`.
  - `needle.{h,cpp}` — `NeedleClient` loads a model, renders chat/tool JSON, and parses
    the reply into `NeedleReply` (text plus `ToolCall`s).
  - `tokenize.{h,cpp}` — quote-aware splitter, a pure function from a line to argv.
  - `command.{h,cpp}` — `ToolCall` → `Command`, `is_risky`, and `fork`/`execvp`/`waitpid`.
  - `shell.{h,cpp}` — the REPL. `run(std::istream&, std::ostream&)` is the seam that makes
    the loop, the builtins, and the confirmation prompt testable without a model.
- `test/` — GoogleTest unit tests, one `*_test.cpp` per module, added to `cactus_tests`.

Logic must live in `cactus_core`, not `main.cpp` — anything in `main.cpp` cannot be tested.

Model-generated command lines must never reach `/bin/sh`. They are split by `tokenize()`
and handed to `execvp` as an explicit argv, so a mistranslation cannot become a
metacharacter injection. Programs on the `is_risky` denylist need confirmation first, and
that check looks through `sudo` at its arguments.

`needle_test.cpp` holds integration tests that need real weights. They skip unless
`CACTUS_NEEDLE_MODEL` points at a Needle weights directory:

```bash
CACTUS_NEEDLE_MODEL=/path/to/weights ctest --test-dir build --output-on-failure
```

## Style

Governed by the `format-cpp` and `refactor-cpp` skills in `.claude/skills/`. Run
`refactor-cpp` before `format-cpp`. Highlights that affect how you write new files:

- 2-space indent; namespace bodies are **not** indented.
- `#ifndef FILENAME_H` header guards, never `#pragma once`.
- `struct` over `class`; `protected` over `private` for member functions.
- Functions `lower_snake_case`, types `UpperCamelCase`.
- `not`/`and`/`or` instead of `!`/`&&`/`||`.
- Project headers use angle brackets — `#include <shell.h>`, not `"shell.h"`. This works
  because `src/` is a `PUBLIC` include directory on `cactus_core`.

Builds are `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`. Keep them warning-free.
