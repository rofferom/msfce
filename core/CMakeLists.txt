add_library(msfce_core STATIC
    include/msfce/core/controller.h
    include/msfce/core/log.h
    include/msfce/core/renderer.h
    include/msfce/core/snes.h

    src/65816.h
    src/65816.cpp

    src/apu.h
    src/apu.cpp

    src/controllerports.h
    src/controllerports.cpp

    src/dma.h
    src/dma.cpp

    src/log.cpp

    src/maths.h
    src/maths.cpp

    src/membus.h
    src/membus.cpp

    src/memcomponent.h
    src/memcomponent.cpp

    src/ppu.h
    src/ppu.cpp

    src/scheduler.h
    src/schedulertask.h
    src/schedulertask.cpp

    src/snesimpl.h
    src/snesimpl.cpp

    src/sram.h
    src/sram.cpp

    src/wram.h
    src/wram.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(msfce_core
    PUBLIC
        snes_spc
)
