# cactus-shell

A Unix shell written in C++23. Currently scaffolding only — no shell logic implemented yet.

## Commands

```bash
cmake -S . -B build                              # configure
cmake --build build -j4                          # build
ctest --test-dir build --output-on-failure       # test
./build/src/cactus                               # run
```

The first configure downloads GoogleTest via `FetchContent` and needs network access. If
GoogleTest is later installed system-wide, CMake finds it instead (`FIND_PACKAGE_ARGS`).

## Layout

- `src/` — all source. `cactus_core` (static lib) holds every testable piece; `main.cpp`
  is a thin wrapper that only constructs `Shell` and calls `run()`. New modules go in
  `src/` and must be added to the `cactus_core` sources list in `src/CMakeLists.txt`.
- `test/` — GoogleTest unit tests, one `*_test.cpp` per module, added to `cactus_tests`.

Logic must live in `cactus_core`, not `main.cpp` — anything in `main.cpp` cannot be tested.

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
