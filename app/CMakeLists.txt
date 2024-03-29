find_package(PkgConfig REQUIRED)

pkg_check_modules(PKG_SDL2 REQUIRED sdl2)
pkg_check_modules(PKG_EPOXY REQUIRED epoxy)
pkg_check_modules(PKG_GLM REQUIRED glm)

pkg_check_modules(PKG_AVCODEC REQUIRED libavcodec)
pkg_check_modules(PKG_AVUTIL REQUIRED libavutil)
pkg_check_modules(PKG_AVFORMAT REQUIRED libavformat)
pkg_check_modules(PKG_SWSCALE REQUIRED libswscale)
pkg_check_modules(PKG_SWRESAMPLE REQUIRED libswresample)

add_executable(msfce
    frontend.h

    renderer_gl.h
    renderer_gl.cpp

    frontend_sdl2/controller_sdl2.h
    frontend_sdl2/controller_sdl2.cpp

    frontend_sdl2/frontend_sdl2.h
    frontend_sdl2/frontend_sdl2.cpp

    recorder/framerecorder.h
    recorder/framerecorder.cpp

    recorder/imagerecorder.h
    recorder/imagerecorder.cpp

    recorder/recorder.h
    recorder/recorder_utils.h
    recorder/recorder.cpp

    recorder/videorecorder.h
    recorder/videorecorder.cpp

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
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-result
)

target_include_directories(msfce
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}

        ${PKG_SDL2_INCLUDE_DIRS}
        ${PKG_EPOXY_INCLUDE_DIRS}
        ${PKG_GLM_INCLUDE_DIRS}

        ${PKG_AVCODEC_INCLUDE_DIRS}
        ${PKG_AVFORMAT_INCLUDE_DIRS}
        ${PKG_AVUTIL_INCLUDE_DIRS}
        ${PKG_SWSCALE_INCLUDE_DIRS}
        ${PKG_SWRESAMPLE_INCLUDE_DIRS}
)

target_link_directories(msfce
    PRIVATE
        ${PKG_SDL2_LIBRARY_DIRS}
        ${PKG_EPOXY_LIBRARY_DIRS}

        ${PKG_AVCODEC_LIBRARY_DIRS}
        ${PKG_AVFORMAT_LIBRARY_DIRS}
        ${PKG_AVUTIL_LIBRARY_DIRS}
        ${PKG_SWSCALE_LIBRARY_DIRS}
        ${PKG_SWRESAMPLE_LIBRARY_DIRS}
)

target_link_libraries(msfce
    PRIVATE
        msfce_core

        ${PKG_SDL2_LIBRARIES}
        ${PKG_EPOXY_LIBRARIES}

        ${PKG_AVCODEC_LIBRARIES}
        ${PKG_AVFORMAT_LIBRARIES}
        ${PKG_AVUTIL_LIBRARIES}
        ${PKG_SWSCALE_LIBRARIES}
        ${PKG_SWRESAMPLE_LIBRARIES}

        -lstdc++fs
        -lpthread
)

if (WIN32)
    target_compile_definitions(msfce_core
        PRIVATE
            __USE_MINGW_ANSI_STDIO=1
    )

    target_link_libraries(msfce
        PRIVATE
            -static-libgcc
            -static-libstdc++
    )

    # Install mingw64 DLLs
    set(MINGW_DLL_LIST
        libwinpthread-1.dll
    )

    foreach(DLL_NAME ${MINGW_DLL_LIST})
        add_custom_command(
            TARGET msfce POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                /usr/${TOOLCHAIN_PREFIX}/lib/${DLL_NAME}
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DLL_NAME}
            COMMENT "Copying ${DLL_NAME}"
        )
    endforeach()

    # Install deps
    set(DEPS_BINARY_DIR ${PKG_SDL2_LIBDIR}/../bin)

    set(DLL_LIST
        avcodec-58.dll
        avformat-58.dll
        avutil-56.dll
        swresample-3.dll
        swscale-5.dll

        SDL2.dll
    )

    foreach(DLL_NAME ${DLL_LIST})
        add_custom_command(
            TARGET msfce POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DEPS_BINARY_DIR}/${DLL_NAME}
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DLL_NAME}
            COMMENT "Copying ${DLL_NAME}"
        )
    endforeach()
endif()
