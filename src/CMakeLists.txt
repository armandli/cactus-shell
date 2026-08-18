add_library(cactus_core STATIC
  shell.cpp
)

target_include_directories(cactus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cactus_core PRIVATE cactus_warnings)

add_executable(cactus main.cpp)

target_link_libraries(cactus PRIVATE cactus_core cactus_warnings)
