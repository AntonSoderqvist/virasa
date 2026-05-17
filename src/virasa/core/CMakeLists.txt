add_library(virasa_core_lib STATIC
    Logger.cpp
)

target_include_directories(virasa_core_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_core_lib
    PUBLIC
        quill::quill
)
