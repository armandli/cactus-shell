# The cactus inference engine is a required link-time dependency: it provides
# the C symbols declared in needle_ffi.h. Build it from
# https://github.com/cactus-compute/cactus and point CACTUS_ROOT at the result.
# Note that its kernels are compiled with -march=armv8.2-a, so it only builds
# on arm64 hosts.
# "cactus_engine_core" is the engine's object code alone, without cactus_graph,
# cactus_kernels, or the Apple frameworks/ObjC runtime it needs at link time, so
# it must not be preferred over the self-contained "cactus_engine" artifact.
find_library(CACTUS_ENGINE_LIBRARY
  NAMES cactus_engine_shared cactus_engine cactus_engine_core cactus
  HINTS ${CACTUS_ROOT} ENV CACTUS_ROOT
  PATH_SUFFIXES lib lib64 build cactus-engine/build
)

if(NOT CACTUS_ENGINE_LIBRARY)
  message(FATAL_ERROR
    "Could not find the cactus inference engine library.\n"
    "cactus-shell links against it for Needle model inference.\n"
    "Build it from https://github.com/cactus-compute/cactus on an arm64 host, "
    "then re-run cmake with -DCACTUS_ROOT=/path/to/cactus"
  )
endif()

add_library(cactus_core STATIC
  command.cpp
  json_util.cpp
  needle.cpp
  shell.cpp
  tokenize.cpp
)

target_include_directories(cactus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cactus_core
  PUBLIC simdjson::simdjson ${CACTUS_ENGINE_LIBRARY}
  PRIVATE cactus_warnings
)

add_executable(cactus main.cpp)

target_link_libraries(cactus PRIVATE cactus_core cactus_warnings)
