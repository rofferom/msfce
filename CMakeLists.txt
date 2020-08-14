cmake_minimum_required(VERSION 3.0)
project(msfe)

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}         -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -O0 -g")

set(MSFE_SOURCES
    src/65816.h
    src/65816.cpp

    src/apu.h
    src/apu.cpp

    src/log.h
    src/log.cpp

    src/maths.h
    src/maths.cpp

    src/membus.h
    src/membus.cpp

    src/ppu.h
    src/ppu.cpp

    src/utils.h

    src/main.cpp
)

add_executable(${PROJECT_NAME} ${MSFE_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
    external/spdlog/include
)

