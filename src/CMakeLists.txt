# Find dependencies
find_package(PkgConfig REQUIRED)

pkg_check_modules(PKG_SDL2 REQUIRED sdl2)
pkg_check_modules(PKG_EPOXY REQUIRED epoxy)

# msfe lib
add_library(msfe_lib
    65816.h
    65816.cpp

    apu.h
    apu.cpp

    controller.h
    controller.cpp

    dma.h
    dma.cpp

    frontend.h

    frontend_dummy.h
    frontend_dummy.cpp

    frontend_sdl2.h
    frontend_sdl2.cpp

    log.h
    log.cpp

    maths.h
    maths.cpp

    membus.h
    membus.cpp

    memcomponent.h
    memcomponent.cpp

    ppu.h
    ppu.cpp

    scheduler.h
    schedulertask.h
    schedulertask.cpp

    snes.h
    snes.cpp

    wram.h
    wram.cpp
)

set_property(TARGET msfe_lib PROPERTY CXX_STANDARD 17)

target_include_directories(msfe_lib PUBLIC
    ${PKG_SDL2_INCLUDE_DIRS}
    ${PKG_EPOXY_INCLUDE_DIRS}

    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_directories(msfe_lib PUBLIC
    ${PKG_SDL2_LIBRARY_DIRS}
    ${PKG_EPOXY_LIBRARY_DIRS}
)

target_link_libraries(msfe_lib PUBLIC
    ${PKG_SDL2_LIBRARIES}
    ${PKG_EPOXY_LIBRARIES}

    -lstdc++fs
    -lpthread
)

# msfe
add_executable(msfe main.cpp)
target_link_libraries(msfe msfe_lib)