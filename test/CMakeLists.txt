include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
  SYSTEM
  FIND_PACKAGE_ARGS NAMES GTest
)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(cactus_tests
  json_util_test.cpp
  needle_test.cpp
  shell_test.cpp
)

target_link_libraries(cactus_tests PRIVATE
  cactus_core
  cactus_warnings
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cactus_tests)
