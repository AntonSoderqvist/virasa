add_executable(virasa_renderer_scene_tests
    test_SceneRenderer.cpp
)

target_link_libraries(virasa_renderer_scene_tests
    PRIVATE
        virasa_renderer_scene_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(virasa_renderer_scene_tests)
