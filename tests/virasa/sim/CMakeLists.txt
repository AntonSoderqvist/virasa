add_executable(virasa_sim_tests
    test_AssetCatalog.cpp
    test_ProjectManifest.cpp
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
    test_GameplayComponents.cpp
    test_VehicleComponent.cpp
    test_WheelComponent.cpp
    test_ComponentCodec.cpp
    test_ProjectLoader.cpp
    test_SceneSerializer.cpp
    behaviors/test_SpinBehavior.cpp
    behaviors/test_PhysicsBehavior.cpp
    behaviors/test_VehicleBehavior.cpp
    behaviors/test_ChaseCameraBehavior.cpp
    test_Builtins.cpp
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
