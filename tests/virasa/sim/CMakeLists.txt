add_executable(virasa_sim_tests
    test_Tick.cpp
)

target_link_libraries(virasa_sim_tests
    PRIVATE
        virasa_sim_lib
        GTest::gtest_main
)

add_executable(virasa_scene_tests
    test_BehaviorRegistry.cpp
    test_Scene.cpp
)

target_link_libraries(virasa_scene_tests
    PRIVATE
        virasa_scene_lib
        virasa_ecs_lib
        virasa_math_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_sim_tests)
gtest_discover_tests(virasa_scene_tests)
