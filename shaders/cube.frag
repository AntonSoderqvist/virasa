#version 450

// virasa cube fragment shader
//
// Inputs: none from the vertex stage. The shader uses gl_PrimitiveID
// (a SPIR-V built-in available without any extensions in core 1.3)
// to derive a face index in the range 0..5 and writes a constant
// color per face.
//
// The cube's index buffer lays out 36 indices as 12 triangles in 6
// groups of 2 -- one group per face -- so triangles 0 and 1 belong
// to face 0, triangles 2 and 3 to face 1, and so on. Integer
// division by 2 collapses each pair of triangles into a single face
// color.
//
// Outputs:
//   location 0: vec4 color
//
// Per virasa.renderer.resources.Pipeline::pipeline_shader_stages_main_entry,
// the entry point is "main".

layout(location = 0) out vec4 out_color;

// Six distinct face colors chosen for visual clarity: red, green,
// blue, yellow, magenta, cyan. Index by face id (gl_PrimitiveID / 2).
const vec3 face_colors[6] = vec3[6](
    vec3(0.95, 0.25, 0.25),  // face 0: red
    vec3(0.25, 0.85, 0.30),  // face 1: green
    vec3(0.25, 0.45, 0.95),  // face 2: blue
    vec3(0.95, 0.85, 0.25),  // face 3: yellow
    vec3(0.85, 0.30, 0.85),  // face 4: magenta
    vec3(0.25, 0.85, 0.95)   // face 5: cyan
);

void main() {
    int face = gl_PrimitiveID / 2;
    // Defensive clamp in case gl_PrimitiveID is ever out of range.
    face = clamp(face, 0, 5);
    out_color = vec4(face_colors[face], 1.0);
}