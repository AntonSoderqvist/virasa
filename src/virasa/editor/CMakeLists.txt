add_library(virasa_editor_lib STATIC
    Buffer.cpp
    Motions.cpp
)

target_include_directories(virasa_editor_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_editor_lib
    PUBLIC
        virasa_window_lib
)
