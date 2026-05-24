add_executable(virasa_math_tests
    test_Types.cpp
    test_Transform.cpp
    test_Projection.cpp
)

target_link_libraries(virasa_math_tests
    PRIVATE
        virasa_math_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_math_tests)
