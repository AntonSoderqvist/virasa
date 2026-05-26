add_library(virasa_ui_lib STATIC
    FontAtlas.cpp
    CommandBar.cpp
    TextPanel.cpp
    HierarchyPanel.cpp
    EntityEditorPanel.cpp
)

target_include_directories(virasa_ui_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_ui_lib
    PUBLIC
        freetype
)
