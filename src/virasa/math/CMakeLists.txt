add_library(virasa_math_lib INTERFACE)

target_include_directories(virasa_math_lib
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(virasa_math_lib
    INTERFACE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
)

target_link_libraries(virasa_math_lib
    INTERFACE
        glm::glm
)
