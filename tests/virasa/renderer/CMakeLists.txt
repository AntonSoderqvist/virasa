add_executable(virasa_renderer_tests
    test_Types.cpp
    core/test_Instance.cpp
)

target_link_libraries(virasa_renderer_tests
    PRIVATE
        virasa_renderer_lib
        virasa_core_lib
        GTest::gtest_main
)

#virasa_set_warnings(virasa_renderer_tests)

include(GoogleTest)
gtest_discover_tests(virasa_renderer_tests)
