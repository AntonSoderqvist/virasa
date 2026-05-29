# Third-party single-header / small-source vendored libraries.
#
# Expected layout under vendor/:
#   vendor/stb/stb_image.h
#   vendor/tinygltf/tiny_gltf.h
#   vendor/mikktspace/mikktspace.c
#   vendor/mikktspace/mikktspace.h
#
# Each dependency is exposed as an imported / interface target so that
# downstream targets do not have to know the include path or compile flag.

# --- stb_image (header-only) ----------------------------------------------
set(_STB_DIR "${PROJECT_SOURCE_DIR}/vendor/stb")
if(NOT EXISTS "${_STB_DIR}/stb_image.h")
    message(FATAL_ERROR
        "stb_image.h not found at ${_STB_DIR}/stb_image.h. "
        "Place stb_image.h under vendor/stb/ before configuring.")
endif()
add_library(virasa_vendor_stb INTERFACE)
target_include_directories(virasa_vendor_stb INTERFACE "${_STB_DIR}")

# --- tinygltf (header-only) -----------------------------------------------
set(_TINYGLTF_DIR "${PROJECT_SOURCE_DIR}/vendor/tinygltf")
if(NOT EXISTS "${_TINYGLTF_DIR}/tiny_gltf.h")
    message(FATAL_ERROR
        "tiny_gltf.h not found at ${_TINYGLTF_DIR}/tiny_gltf.h. "
        "Place tiny_gltf.h (and its bundled json.hpp) under vendor/tinygltf/ "
        "before configuring.")
endif()
add_library(virasa_vendor_tinygltf INTERFACE)
target_include_directories(virasa_vendor_tinygltf INTERFACE "${_TINYGLTF_DIR}")
# tinygltf decodes images via stb_image; disable its own decoder so the
# editor.io.ImageLoader path is the single decode site.
target_compile_definitions(virasa_vendor_tinygltf INTERFACE
    TINYGLTF_NO_STB_IMAGE
    TINYGLTF_NO_STB_IMAGE_WRITE
    TINYGLTF_NO_EXTERNAL_IMAGE
)

# --- MikkTSpace (C source + header) ---------------------------------------
set(_MIKKT_DIR "${PROJECT_SOURCE_DIR}/vendor/mikktspace")
if(NOT EXISTS "${_MIKKT_DIR}/mikktspace.c" OR NOT EXISTS "${_MIKKT_DIR}/mikktspace.h")
    message(FATAL_ERROR
        "mikktspace.c/.h not found under ${_MIKKT_DIR}. "
        "Place mikktspace.c and mikktspace.h under vendor/mikktspace/ "
        "before configuring.")
endif()
add_library(virasa_vendor_mikktspace STATIC "${_MIKKT_DIR}/mikktspace.c")
target_include_directories(virasa_vendor_mikktspace PUBLIC "${_MIKKT_DIR}")
set_target_properties(virasa_vendor_mikktspace PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LINKER_LANGUAGE C
)
