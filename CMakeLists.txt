cmake_minimum_required(VERSION 3.24)
project(viras
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options ---
option(VIRASA_BUILD_TESTS "Build tests" ON)

# --- CMake modules ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(VendorLibs)

# Quill
set(QUILL_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/quill EXCLUDE_FROM_ALL)

# SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)

# Vulkan
find_package(Vulkan REQUIRED)

# GLM
# Keep GLM header-only to match previous package-manager usage.
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glm EXCLUDE_FROM_ALL)

# FreeType (FTL-licensed; disable optional deps for a clean static build)
set(FT_DISABLE_ZLIB     TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2    TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG      TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI   TRUE CACHE BOOL "" FORCE)
add_subdirectory(vendor/freetype EXCLUDE_FROM_ALL)

# Jolt Physics (rigid-body dynamics). All JPH_* defines and SIMD compile
# options are set PUBLIC on the Jolt target, so linking it propagates a
# matching ABI to consumers automatically. Disable Jolt's global build-flag
# overrides, LTO, install/export, and -Werror so it slots cleanly into our tree.
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/JoltPhysics/Build EXCLUDE_FROM_ALL)

# --- Engine Modules ---
add_subdirectory(src/virasa/core)
add_subdirectory(src/virasa/math)
add_subdirectory(src/virasa/sim)
add_subdirectory(src/virasa/window)
add_subdirectory(src/virasa/renderer)
add_subdirectory(src/virasa/ecs)
add_subdirectory(src/virasa/physics)
add_subdirectory(src/virasa/input)
add_subdirectory(src/virasa/ui)
add_subdirectory(src/virasa/renderer/text)
add_subdirectory(src/virasa/renderer/scene)
add_subdirectory(src/virasa/editor)

# --- Shaders ---
add_subdirectory(shaders)

# --- Fonts (copied next to the binary so relative paths resolve from build/) ---
configure_file(
    "${PROJECT_SOURCE_DIR}/vendor/JetBrainsMono/fonts/ttf/JetBrainsMono-Regular.ttf"
    "${CMAKE_BINARY_DIR}/fonts/JetBrainsMono-Regular.ttf"
    COPYONLY
)

# --- Tests ---
if(VIRASA_BUILD_TESTS)
    enable_testing()
    # GoogleTest
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/googletest EXCLUDE_FROM_ALL)
    add_subdirectory(tests/virasa/core)
    add_subdirectory(tests/virasa/math)
    add_subdirectory(tests/virasa/sim)
    add_subdirectory(tests/virasa/window)
    add_subdirectory(tests/virasa/renderer)
    add_subdirectory(tests/virasa/ecs)
    add_subdirectory(tests/virasa/physics)
    add_subdirectory(tests/virasa/input)
    add_subdirectory(tests/virasa/ui)
    add_subdirectory(tests/virasa/renderer/text)
    add_subdirectory(tests/virasa/editor)
    add_subdirectory(tests/virasa/renderer/scene)
endif()

# --- Editor executable ---
add_executable(virasa_editor src/virasa/editor/main.cpp)

target_link_libraries(virasa_editor
    PRIVATE
        virasa_core_lib
        virasa_window_lib
        virasa_renderer_lib
        virasa_math_lib
        virasa_ecs_lib
        virasa_ui_lib
        virasa_renderer_text_lib
        virasa_editor_lib
)

# --- Examples ---
# Examples (box-stack, dreidel) are now data-driven projects (project.json +
# scenes) loaded by virasa_editor at runtime, not built C++ subdirectories.
