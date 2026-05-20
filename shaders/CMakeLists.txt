find_package(Vulkan REQUIRED COMPONENTS glslc)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

set(GLSLC Vulkan::glslc)

set(SHADERS
    triangle.vert
    triangle.frag
)

set(SPV_FILES)
foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME "${SHADER}" NAME)
    get_filename_component(SHADER_EXT  "${SHADER}" EXT)
    string(REPLACE "." "" STAGE "${SHADER_EXT}")  # .vert -> vert
    set(SPV_OUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT  "${SPV_OUT}"
        COMMAND ${GLSLC} -fshader-stage=${STAGE} "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}" -o "${SPV_OUT}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}"
        COMMENT "Compiling shader ${SHADER} -> ${SHADER_NAME}.spv"
        VERBATIM
    )
    list(APPEND SPV_FILES "${SPV_OUT}")
endforeach()

add_custom_target(virasa_shaders ALL DEPENDS ${SPV_FILES})
