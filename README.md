# cactus-shell

A shell written in C++23 that takes plain English and runs the matching commands.
Natural language is translated to tool calls by the
[Needle](https://github.com/cactus-compute/needle) model, running locally through the
[cactus](https://github.com/cactus-compute/cactus) inference engine.

Status: early, but the loop is closed. You type English, the model picks a tool out of a
fixed catalog and fills in its named arguments, and the shell runs it.

The model never names a program. It chooses from the 26 entries in `cactus-tools.json` —
`ls`, `grep`, `mv`, `rm`, and so on — each with a described, typed argument schema, so
`-a` is a documented boolean called `all` rather than a flag spelling the model has to
recall. Anything the catalog does not cover fails closed and runs nothing.

Nothing reaches `/bin/sh`. Each JSON value the model fills in becomes exactly one `execvp`
argv entry, with no splitting or quoting step in between, so a mistranslation cannot become
a metacharacter hazard. That costs pipes, redirects, and globbing. Tools marked destructive
(`rm`, `rmdir`, `mv`, `cp`, `mkdir`, `touch`, `chmod`, `kill`) stop for a `y/N`
confirmation first. Argument *values* are still model-chosen, so `rm` with a path of `/`
remains expressible — which is what the confirmation is for.

A reply carrying several tool calls runs them in order and stops at the first failure.

## The tool catalog

The catalog is a JSON file, not compiled in. The shell loads the first of
`./cactus-tools.json`, `~/.config/cactus-shell/tools.json`, or the `cactus-tools.json`
shipped in this repo. Adding a tool is one entry:

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

`kind` is `flag`, `option`, `number`, `positional`, or `positional_list`; parameters
become argv in the order they are declared. A config that would build a broken argv —
duplicate names, an unknown kind, a flag with no spelling — is rejected at load with the
offending entry named.

**The config file is as trusted as the binary.** Anything it names, the shell will run,
including `sh` or `sudo`; there is no denylist behind it. `./cactus-tools.json` means
`cd`-ing into a directory picks up that directory's catalog, which is both the convenient
case and the dangerous one. Do not load a config you did not write.

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
| `src/tools.h` | `ToolCatalog` — loads and validates the catalog config, renders the model's schema. |
| `cactus-tools.json` | The shipped tool catalog. |
| `src/command.h` | Tool call → `Command` via the catalog, plus `fork`/`execvp`. |
| `src/shell.h` | The REPL, with `std::istream`/`std::ostream` injected for tests. |
| `test/` | GoogleTest unit tests. |

## License

MIT — see [LICENSE](LICENSE).
