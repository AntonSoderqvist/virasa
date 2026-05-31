add_library(virasa_window_lib STATIC
    Platform.cpp
    InputState.cpp
)

target_include_directories(virasa_window_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_window_lib
    PUBLIC
    Vulkan::Vulkan
        SDL3::SDL3-static
        virasa_core_lib
)
