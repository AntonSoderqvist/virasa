add_executable(virasa_renderer_text_tests
    test_UiPass.cpp
)

target_link_libraries(virasa_renderer_text_tests
    PRIVATE
        virasa_renderer_text_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_renderer_text_tests)
