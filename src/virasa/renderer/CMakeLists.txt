add_library(virasa_renderer_lib STATIC
    core/Instance.cpp
    core/Surface.cpp
    core/Device.cpp
)

target_include_directories(virasa_renderer_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_renderer_lib
    PUBLIC
        Vulkan::Vulkan
    PRIVATE
        virasa_core_lib
        virasa_window_lib
)
