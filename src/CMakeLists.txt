# The Needle 3 engine is a required link-time dependency: it provides the C
# symbols declared in needle_ffi.h. It ships prebuilt, one folder per platform,
# in https://huggingface.co/Cactus-Compute/needle3. Download the folder for
# this host and point NEEDLE_ROOT at its parent (or at the folder itself).
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(needle_os macos)
else()
  set(needle_os linux)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
  set(needle_arch arm64)
else()
  set(needle_arch ${CMAKE_SYSTEM_PROCESSOR})
endif()
set(NEEDLE_PLATFORM "${needle_os}-${needle_arch}" CACHE STRING
  "Needle 3 platform folder to link from, such as macos-arm64")

# The shared library (libneedle3.so) has its C++ stdlib statically linked, so
# it links cleanly with either libstdc++ or libc++. Prefer it; fall back to the
# static archive libneedle.a on platforms where only that ships.
find_library(NEEDLE_LIBRARY
  NAMES needle3 libneedle.a
  HINTS ${NEEDLE_ROOT} ENV NEEDLE_ROOT
  PATH_SUFFIXES ${NEEDLE_PLATFORM}
  NO_DEFAULT_PATH
)

if(NOT NEEDLE_LIBRARY)
  message(FATAL_ERROR
    "Could not find libneedle3.so or libneedle.a for ${NEEDLE_PLATFORM}.\n"
    "cactus-shell links against the Needle 3 engine for model inference.\n"
    "Download the ${NEEDLE_PLATFORM} folder from "
    "https://huggingface.co/Cactus-Compute/needle3, "
    "then re-run cmake with -DNEEDLE_ROOT=/path/to/needle3"
  )
endif()

# When linking the shared library, bake in its directory as a build-time RPATH
# so the binary finds libneedle3.so without LD_LIBRARY_PATH. Set in both the
# current scope (for cactus) and the parent scope (for cactus_tests).
if(NEEDLE_LIBRARY MATCHES "\\.so")
  get_filename_component(_needle_lib_dir "${NEEDLE_LIBRARY}" DIRECTORY)
  set(CMAKE_BUILD_RPATH "${_needle_lib_dir}")
  set(CMAKE_BUILD_RPATH "${_needle_lib_dir}" PARENT_SCOPE)
endif()

add_library(cactus_core STATIC
  command.cpp
  json_util.cpp
  needle.cpp
  shell.cpp
  tokenize.cpp
  tools.cpp
)

target_include_directories(cactus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Last resort in ToolCatalog::discover(). PUBLIC because the config test loads
# the shipped catalog through the same path, and duplicating the expression
# would let the two drift.
target_compile_definitions(cactus_core PUBLIC
  CACTUS_DEFAULT_TOOL_CONFIG="${PROJECT_SOURCE_DIR}/cactus-tools.json"
  CACTUS_DEFAULT_NEEDLE_MODEL="${PROJECT_SOURCE_DIR}/needle3")

target_link_libraries(cactus_core
  PUBLIC simdjson::simdjson ${NEEDLE_LIBRARY}
  PRIVATE cactus_warnings
)

add_executable(cactus main.cpp)

target_link_libraries(cactus PRIVATE cactus_core cactus_warnings)
