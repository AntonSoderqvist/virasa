add_library(virasa_ecs_lib STATIC
    ComponentSystem.cpp
    TransformSystem.cpp
    World.cpp
    Scheduler.cpp
)

target_include_directories(virasa_ecs_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_ecs_lib
    PUBLIC
        Vulkan::Vulkan
        virasa_math_lib
        virasa_sim_lib
    PRIVATE
        virasa_core_lib
)
