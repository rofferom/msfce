add_executable(msfce_tests
    membus_unittest.cpp

    tests.cpp
)

target_link_libraries(msfce_tests PRIVATE
    msfce_core

    gmock
    gtest
)