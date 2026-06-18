add_executable(virasa_input_tests
    test_Actions.cpp
    test_Bindings.cpp
    test_Resolver.cpp
    test_StepBridge.cpp
)

target_link_libraries(virasa_input_tests
    PRIVATE
        virasa_input_lib
        virasa_window_lib
        virasa_ecs_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_input_tests)
