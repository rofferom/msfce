cmake_minimum_required(VERSION 3.0)
project(snes_spc)

add_library(snes_spc STATIC
    snes_spc/dsp.cpp
    snes_spc/SNES_SPC.cpp
    snes_spc/SNES_SPC_misc.cpp
    snes_spc/SNES_SPC_state.cpp
    snes_spc/SPC_DSP.cpp
    snes_spc/SPC_Filter.cpp
)

set_target_properties(snes_spc
    PROPERTIES
        CXX_STANDARD 98
)

target_include_directories(snes_spc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
