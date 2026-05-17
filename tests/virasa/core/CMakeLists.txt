add_executable(virasa_core_tests
    test_Logger.cpp
)

target_link_libraries(virasa_core_tests
    PRIVATE
        virasa_core_lib
        GTest::gtest_main
)

#virasa_set_warnings(virasa_core_tests)

include(GoogleTest)
gtest_discover_tests(virasa_core_tests)
