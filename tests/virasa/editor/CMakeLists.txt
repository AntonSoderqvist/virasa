add_executable(virasa_editor_tests
    test_Buffer.cpp
    test_Motions.cpp
)

target_link_libraries(virasa_editor_tests
    PRIVATE
        virasa_editor_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_editor_tests)
