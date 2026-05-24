cmake_minimum_required(VERSION 3.24)
project(viras
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options ---
option(VIRASA_BUILD_TESTS "Build tests" ON)

# --- CMake modules ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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
find_package(glm REQUIRED)

# --- Engine Modules ---
add_subdirectory(src/virasa/core)
add_subdirectory(src/virasa/window)
add_subdirectory(src/virasa/renderer)

# --- Shaders ---
add_subdirectory(shaders)

# --- Tests ---
if(VIRASA_BUILD_TESTS)
    enable_testing()
    # GoogleTest
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/googletest EXCLUDE_FROM_ALL)
    add_subdirectory(tests/virasa/core)
    add_subdirectory(tests/virasa/window)
    add_subdirectory(tests/virasa/renderer)
endif()

# --- Editor executable ---
add_executable(virasa_editor src/virasa/editor/main.cpp)

target_link_libraries(virasa_editor
    PRIVATE
        virasa_core_lib
        virasa_window_lib
        virasa_renderer_lib
)
