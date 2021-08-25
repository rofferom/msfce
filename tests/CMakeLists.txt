add_executable(msfce_tests
    membus_unittest.cpp

    tests.cpp
)

target_include_directories(msfce_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/src
)

target_link_libraries(msfce_tests PRIVATE
    msfce_core

    gmock
    gtest
)