add_library(virasa_ecs_lib STATIC
    World.cpp
)

target_include_directories(virasa_ecs_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_ecs_lib
    PUBLIC
        virasa_math_lib
    PRIVATE
        virasa_core_lib
)
