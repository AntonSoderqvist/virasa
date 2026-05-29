add_library(virasa_renderer_lib STATIC
    core/Instance.cpp
    core/Surface.cpp
    core/Device.cpp
    core/Swapchain.cpp
    core/Context.cpp
    resources/ShaderModule.cpp
    resources/Pipeline.cpp
    resources/Image.cpp
    resources/Buffer.cpp
    resources/BindlessTextureArray.cpp
    resources/Sampler.cpp
    resources/Mesh.cpp
    resources/MeshRegistry.cpp
    material/Visual.cpp
    lighting/LightTable.cpp
    graph/Graph.cpp
    graph/Pass.cpp
    graph/ImageRegistry.cpp
    graph/BufferRegistry.cpp
    geometry/Tangents.cpp
)

target_include_directories(virasa_renderer_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_renderer_lib
    PUBLIC
        Vulkan::Vulkan
        glm::glm
        virasa_math_lib
    PRIVATE
        virasa_core_lib
        virasa_window_lib
        virasa_vendor_mikktspace
)
