add_executable(virasa_physics_tests
    test_PhysicsComponents.cpp
    test_PhysicsWorld.cpp
)

target_link_libraries(virasa_physics_tests
    PRIVATE
        virasa_physics_lib
        virasa_ecs_lib
        virasa_math_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_physics_tests)
