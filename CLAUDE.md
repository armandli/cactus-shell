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
  - `tokenize.{h,cpp}` — quote-aware splitter, a pure function from a line to argv. Only
    the typed `cd` builtin uses it; it is off the model path.
  - `tools.{h,cpp}` — `ToolCatalog` reads the catalog from a JSON config file and
    validates it. The parsed `ToolSpec`s drive both the JSON schema the model sees and
    the argv built from its reply, so the two cannot drift. `parse` is the testable seam
    (text in, catalog out); `load` and `discover` add the filesystem.
  - `command.{h,cpp}` — `ToolCall` → `Command` by walking a `ToolSpec`, plus
    `fork`/`execvp`/`waitpid`.
  - `shell.{h,cpp}` — the REPL. `run(std::istream&, std::ostream&)` is the seam that makes
    the loop, the builtins, and the confirmation prompt testable without a model.
- `cactus-tools.json` — the shipped tool catalog. Adding a tool is one entry here, not a
  code change.
- `test/` — GoogleTest unit tests, one `*_test.cpp` per module, added to `cactus_tests`.

Logic must live in `cactus_core`, not `main.cpp` — anything in `main.cpp` cannot be tested.

## Tool config

`ToolCatalog::discover()` loads the first of these that exists:

1. `./cactus-tools.json`
2. `~/.config/cactus-shell/tools.json`
3. the path baked in as `CACTUS_DEFAULT_TOOL_CONFIG` (the repo's `cactus-tools.json`)

Nothing is compiled in as a fallback, so a missing config means no tools at all. The
catalog loads lazily on the first model request, which is what keeps `exit`, `quit`, and
the typed `cd` builtin working with no config on disk.

The file is a JSON array of tools:

```json
{ "name": "rm", "program": "rm", "description": "Delete files permanently",
  "risky": true,
  "params": [
    { "name": "recursive", "kind": "flag", "flag": "-r",
      "description": "Delete directories and everything inside them" },
    { "name": "paths", "kind": "positional_list", "required": true,
      "description": "Files or directories to delete" }
  ] }
```

`kind` is `flag`, `option`, `number`, `positional`, or `positional_list`. `risky`,
`in_process`, and `required` default to `false`; `flag` and `params` default to empty.
Parameters become argv in declaration order, so list them in the order the program
expects — `find` declares `path` before `-name`.

Load rejects a config that breaks any invariant the argv builder relies on: missing or
empty `name`/`program`/`description`, duplicate tool or parameter names, an unknown
`kind`, a `flag`/`option`/`number` with no flag spelling or a positional kind carrying
one, or `in_process` on anything but `cd`. Errors name the offending entry
(`ls.path.kind`, `entry 7.name`), since this is a file a human edits.

**Trust model.** The config file is as trusted as the binary. Anything it names, the
shell will run — `sh`, `sudo`, whatever — and there is no denylist or permission check
behind it. `./cactus-tools.json` in particular means `cd`-ing into a directory picks up
that directory's catalog. Do not load a config you did not write. This is an accepted
trade: someone who can write the config can usually replace the binary too.

What survives that trade: the model still cannot name a program. It picks an entry from
the loaded catalog, and a name outside it fails as `ExecError::UnknownTool` and runs
nothing. Each JSON value the model fills in becomes exactly one argv entry handed to
`execvp`, with no splitting or quoting step in between, so nothing model-generated ever
reaches `/bin/sh`. Argument values are still model-chosen, so tools with `risky` set
confirm with the user before running.

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
- Encapsulated data members are `mUpperCamelCase` — `mModel`, `mPendingComma`. Public
  fields of plain data structs keep bare `lower_snake_case` names.
- `not`/`and`/`or` instead of `!`/`&&`/`||`.
- Project headers use angle brackets — `#include <shell.h>`, not `"shell.h"`. This works
  because `src/` is a `PUBLIC` include directory on `cactus_core`.

Builds are `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`. Keep them warning-free.
