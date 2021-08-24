set(MSFCE_TEST_SOURCES
    membus_unittest.cpp

    tests.cpp
)

add_executable(msfce_tests ${MSFCE_TEST_SOURCES})

target_link_libraries(msfce_tests PUBLIC
    msfce_lib
    gmock
    gtest
)