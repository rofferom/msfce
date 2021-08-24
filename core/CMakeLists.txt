add_library(msfce_core STATIC
    65816.h
    65816.cpp

    apu.h
    apu.cpp

    controller.h
    controller.cpp

    dma.h
    dma.cpp

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

    sram.h
    sram.cpp

    wram.h
    wram.cpp
)

set_target_properties(msfce_core
    PROPERTIES
        CXX_STANDARD 17
)

target_compile_options(msfce_core
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-result
)

target_include_directories(msfce_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(msfce_core
    PUBLIC
        snes_spc
)
