# Input action layer: maps raw window input through an authored binding table
# into per-step action snapshots, bridged into the ECS World as a resource.
# Actions.h is header-only; the rest carry .cpp translation units.
add_library(virasa_input_lib STATIC
    Bindings.cpp
    Resolver.cpp
    StepBridge.cpp
)

target_include_directories(virasa_input_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_input_lib
    PUBLIC
        virasa_window_lib
        virasa_ecs_lib
)
