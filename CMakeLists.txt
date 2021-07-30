cmake_minimum_required(VERSION 3.0)
project(msfe)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(external/googletest)

add_subdirectory(src)
add_subdirectory(tests)