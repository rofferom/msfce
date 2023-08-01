add_executable(msfce_tests
    membus_unittest.cpp

    tests.cpp
)

set_target_properties(msfce_tests PROPERTIES
    CXX_STANDARD 17
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