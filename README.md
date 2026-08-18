# cactus-shell

A Unix shell written in C++23.

Status: early scaffolding. The build, test, and run loop work end to end, but no shell
functionality is implemented yet.

## Prerequisites

- CMake 3.25 or newer
- A C++23 compiler (GCC 13+ or Clang 16+)
- Network access on the first configure, to fetch GoogleTest

## Build

```bash
cmake -S . -B build
cmake --build build -j4
```

## Test

```bash
ctest --test-dir build --output-on-failure
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
| `test/` | GoogleTest unit tests. |

## License

MIT — see [LICENSE](LICENSE).
