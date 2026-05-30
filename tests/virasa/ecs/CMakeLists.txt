add_executable(virasa_ecs_tests
    test_Types.cpp
    test_Components.cpp
    test_ComponentSystem.cpp
    test_TransformSystem.cpp
    test_World.cpp
)

target_link_libraries(virasa_ecs_tests
    PRIVATE
        virasa_ecs_lib
        virasa_math_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_ecs_tests)
