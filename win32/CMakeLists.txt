add_executable(msfce
    main.cpp
)

set_target_properties(msfce
    PROPERTIES
        CXX_STANDARD 17
)

target_compile_options(msfce
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wno-missing-field-initializers
        -Wno-unused-but-set-variable
        -Wno-unused-parameter
        -Wno-unused-result
        -Wno-unused-variable
)

target_include_directories(msfce
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(msfce
    PRIVATE
        msfce_core

        -ld3d11
        -ld3dcompiler
)

target_compile_definitions(msfce
    PRIVATE
        UNICODE
)

target_link_libraries(msfce
    PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic -lpthread -Wl,-Bdynamic
)
