cmake_minimum_required(VERSION 3.0)
project(msfe)

# Set global compilation flags
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}         -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -O0 -g")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PKG_SDL2 REQUIRED sdl2)

set(MSFE_SOURCES
    src/65816.h
    src/65816.cpp

    src/apu.h
    src/apu.cpp

    src/controller.h
    src/controller.cpp

    src/dma.h
    src/dma.cpp

    src/log.h
    src/log.cpp

    src/maths.h
    src/maths.cpp

    src/membus.h
    src/membus.cpp

    src/memcomponent.h
    src/memcomponent.cpp

    src/ppu.h
    src/ppu.cpp

    src/utils.h

    src/main.cpp
)

add_executable(${PROJECT_NAME} ${MSFE_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${PKG_SDL2_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME} PUBLIC
    ${PKG_SDL2_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${PKG_SDL2_LIBRARIES}
    -lpthread
)
