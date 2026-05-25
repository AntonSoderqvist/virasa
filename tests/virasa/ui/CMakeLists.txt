add_executable(virasa_ui_tests
    test_Types.cpp
    test_FontAtlas.cpp
    test_CommandBar.cpp
)

target_link_libraries(virasa_ui_tests
    PRIVATE
        virasa_ui_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_ui_tests)
