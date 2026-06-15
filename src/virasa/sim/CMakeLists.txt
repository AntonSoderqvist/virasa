add_library(virasa_sim_lib STATIC
    AssetCatalog.cpp
    ProjectManifest.cpp
    Tick.cpp
)

target_include_directories(virasa_sim_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# ProjectManifest is a pure-data JSON seam (no ecs dependency, so this leaf
# target stays cycle-free) but needs the vendored nlohmann/json header.
target_link_libraries(virasa_sim_lib
    PUBLIC
        virasa_vendor_tinygltf
)

# Higher sim layer: depends on ecs (World/Scheduler). Kept in a separate
# target so the leaf virasa_sim_lib (Tick) stays free of an ecs dependency,
# avoiding a cycle with virasa_ecs_lib (which links virasa_sim_lib for Tick).
add_library(virasa_scene_lib STATIC
    BehaviorRegistry.cpp
    ComponentCodec.cpp
    ProjectLoader.cpp
    Scene.cpp
    SceneSerializer.cpp
    behaviors/SpinBehavior.cpp
    Builtins.cpp
)

target_include_directories(virasa_scene_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(virasa_scene_lib
    PUBLIC
        virasa_ecs_lib
        virasa_sim_lib
        virasa_math_lib
        virasa_vendor_tinygltf
)
