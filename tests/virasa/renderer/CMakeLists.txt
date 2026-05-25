add_executable(virasa_renderer_tests
    test_Types.cpp
    core/test_Instance.cpp
    core/test_Surface.cpp
    core/test_Device.cpp
    core/test_Swapchain.cpp
    core/test_Context.cpp
    resources/test_ShaderModule.cpp
    resources/test_Pipeline.cpp
    resources/test_Image.cpp
    resources/test_Buffer.cpp
    resources/test_BindlessTextureArray.cpp
    resources/test_Sampler.cpp
    resources/test_Mesh.cpp
    resources/test_MeshRegistry.cpp
    geometry/test_Primitives.cpp
    material/test_Visual.cpp
    lighting/test_LightTable.cpp
    graph/test_Graph.cpp
    graph/test_Types.cpp
    graph/test_Pass.cpp
    graph/test_ImageRegistry.cpp
    graph/test_BufferRegistry.cpp
)

target_link_libraries(virasa_renderer_tests
    PRIVATE
        virasa_renderer_lib
        virasa_core_lib
        virasa_window_lib
        virasa_math_lib
        GTest::gtest_main
)

#virasa_set_warnings(virasa_renderer_tests)

include(GoogleTest)
gtest_discover_tests(virasa_renderer_tests)
