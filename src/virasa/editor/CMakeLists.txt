add_library(virasa_editor_lib STATIC
    Buffer.cpp
    Motions.cpp
    CommandBarView.cpp
    EditorView.cpp
    HierarchyView.cpp
    EntityEditorView.cpp
    ViewManager.cpp
    EditorApp.cpp
    io/ImageLoader.cpp
    io/GltfLoader.cpp
)

target_include_directories(virasa_editor_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_editor_lib
    PUBLIC
        virasa_core_lib
        virasa_window_lib
        virasa_ui_lib
        virasa_ecs_lib
        virasa_renderer_lib
        virasa_renderer_scene_lib
    PRIVATE
        virasa_vendor_stb
        virasa_vendor_tinygltf
)
