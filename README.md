# cactus-shell

A shell written in C++23 that takes plain English and runs the matching commands.
Natural language is translated to tool calls by the
[Needle](https://github.com/cactus-compute/needle) model, running locally through the
[cactus](https://github.com/cactus-compute/cactus) inference engine.

Status: early, but the loop is closed. You type English, the model answers with a
`run_command` tool call, and the shell runs it.

The command string never reaches `/bin/sh`. It is model-generated, so routing it through a
shell would turn every mistranslation into a metacharacter hazard; instead the line is
tokenized here and handed straight to `execvp`. That costs pipes, redirects, and globbing.
Commands whose program is destructive (`rm`, `dd`, `mkfs`, `chmod`, `sudo <any of those>`,
and friends) stop for a `y/N` confirmation first.

## Prerequisites

- CMake 3.25 or newer
- A C++23 compiler (GCC 13+ or Clang 16+)
- **An arm64 host.** The cactus engine is a required link-time dependency and its kernels
  are built with `-march=armv8.2-a`, so it does not build on x86_64.
- Network access on the first configure, to fetch simdjson and GoogleTest

### Building the cactus engine

```bash
git clone https://github.com/cactus-compute/cactus
cd cactus && source ./setup
```

Then point this project at it with `-DCACTUS_ROOT`. Model weights come from
[Cactus-Compute on Hugging Face](https://huggingface.co/Cactus-Compute).

## Build

```bash
cmake -S . -B build -DCACTUS_ROOT=/path/to/cactus
cmake --build build -j4
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

The Needle tests need real weights and skip without them:

```bash
CACTUS_NEEDLE_MODEL=/path/to/weights ctest --test-dir build --output-on-failure
```

Pass `-DCACTUS_BUILD_TESTS=OFF` at configure time to skip building tests entirely, which
also skips the GoogleTest download.

## Run

```bash
./build/cactus /path/to/weights         # or set CACTUS_NEEDLE_MODEL
```

```
cactus$ list the files here
> ls
CMakeLists.txt  LICENSE  README.md  src  test
cactus$ cd /tmp
cactus$ exit
```

`cd`, `exit`, and `quit` are handled in-process and never reach the model.

## Layout

| Path | Contents |
| --- | --- |
| `src/` | All source code. `cactus_core` static library plus the `cactus` executable. |
| `src/json_util.h` | `JsonBuilder` to write JSON, `JsonDoc` to parse it, both over simdjson. |
| `src/needle.h` | `NeedleClient` — feeds prompts to the model, returns parsed JSON replies. |
| `src/needle_ffi.h` | The subset of cactus's C FFI that this project links against. |
| `src/tokenize.h` | Quote-aware splitter that turns a command line into argv. |
| `src/command.h` | Tool call → `Command`, the risky-program check, and `fork`/`execvp`. |
| `src/shell.h` | The REPL, with `std::istream`/`std::ostream` injected for tests. |
| `test/` | GoogleTest unit tests. |

## License

MIT — see [LICENSE](LICENSE).
