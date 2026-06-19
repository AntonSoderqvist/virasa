add_library(virasa_physics_lib STATIC
    PhysicsWorld.cpp
)

target_include_directories(virasa_physics_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Jolt is an implementation detail: its headers and types live entirely in
# PhysicsWorld.cpp behind a pimpl, so it is linked PRIVATE. The public surface
# depends only on engine types.
target_link_libraries(virasa_physics_lib
    PUBLIC
        virasa_math_lib
        virasa_ecs_lib
    PRIVATE
        Jolt
)
