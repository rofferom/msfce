cmake_minimum_required(VERSION 3.0)
project(msfe)

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}         -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-result")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -O0 -g")

add_subdirectory(external/googletest)

add_subdirectory(src)
add_subdirectory(tests)