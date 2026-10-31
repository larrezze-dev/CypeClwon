include(GoogleTest)
add_executable(core_tests core_tests.cpp)
target_link_libraries(core_tests PRIVATE cc_core GTest::gtest GTest::gtest_main)
gtest_discover_tests(core_tests)

