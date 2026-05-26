add_library(virasa_renderer_scene_lib STATIC
    SceneRenderer.cpp
)

target_include_directories(virasa_renderer_scene_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_renderer_scene_lib
    PUBLIC
        virasa_renderer_lib
        virasa_renderer_text_lib
        virasa_ui_lib
        virasa_ecs_lib
        virasa_math_lib
        Vulkan::Vulkan
    PRIVATE
        virasa_core_lib
)
