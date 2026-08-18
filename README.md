# cactus-shell

A shell written in C++23 that takes plain English and runs the matching commands.
Natural language is translated to tool calls by the
[Needle](https://github.com/cactus-compute/needle) model, running locally through the
[cactus](https://github.com/cactus-compute/cactus) inference engine.

Status: early. The build, test, and run loop work end to end; the JSON layer and the
Needle client are in place, but the shell itself does not execute commands yet.

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
./build/src/cactus
```

## Layout

| Path | Contents |
| --- | --- |
| `src/` | All source code. `cactus_core` static library plus the `cactus` executable. |
| `src/json_util.h` | `JsonBuilder` to write JSON, `JsonDoc` to parse it, both over simdjson. |
| `src/needle.h` | `NeedleClient` — feeds prompts to the model, returns parsed JSON replies. |
| `src/needle_ffi.h` | The subset of cactus's C FFI that this project links against. |
| `test/` | GoogleTest unit tests. |

## License

MIT — see [LICENSE](LICENSE).
