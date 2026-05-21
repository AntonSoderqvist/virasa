#version 450

// virasa cube vertex shader
//
// Inputs:
//   location 0: vec3 position (object-space corner of the unit cube
//               centered on the origin, in the range [-0.5, +0.5]^3)
//
// Push constants (vertex stage, offset 0, size 64):
//   mat4 mvp    -- pre-computed proj * view * model, column-major.
//                  Includes Vulkan-clip-space Y inversion and 0..1
//                  depth (see virasa.editor.main:
//                  main_computes_static_mvp_with_glm).
//
// Outputs:
//   gl_Position -- the homogeneous clip-space position.
//
// Per virasa.renderer.resources.Pipeline::pipeline_shader_stages_main_entry,
// the entry point is "main".

layout(location = 0) in vec3 in_position;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(in_position, 1.0);
}