cmake_minimum_required(VERSION 3.0)
project(msfce)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(external/googletest)
add_subdirectory(external/snes_spc)

add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(tests)
