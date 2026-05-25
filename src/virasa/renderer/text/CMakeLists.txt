add_library(virasa_renderer_text_lib STATIC
    UiPass.cpp
)

target_include_directories(virasa_renderer_text_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_renderer_text_lib
    PUBLIC
        virasa_renderer_lib
        virasa_ui_lib
        virasa_window_lib
        Vulkan::Vulkan
)
