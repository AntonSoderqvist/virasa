add_executable(virasa_window_tests
    test_Events.cpp
    test_Types.cpp
)

target_link_libraries(virasa_window_tests
    PRIVATE
        virasa_window_lib
        GTest::gtest_main
)

#virasa_set_warnings(virasa_window_tests)

include(GoogleTest)
gtest_discover_tests(virasa_window_tests)
