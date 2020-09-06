set(MSFE_TEST_SOURCES
    membus_unittest.cpp

    tests.cpp
)

add_executable(mfse_tests ${MSFE_TEST_SOURCES})

target_link_libraries(mfse_tests PUBLIC
    msfe_lib
    gmock
    gtest
)