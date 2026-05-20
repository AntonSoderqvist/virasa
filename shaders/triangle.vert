#version 450

// Hello-triangle vertex shader.
//
// No vertex buffer is bound: the pipeline is created with the
// "no vertex input" path (stride 0, zero attributes). The three
// vertex positions and colors are hardcoded here and selected by
// gl_VertexIndex, which runs 0, 1, 2 for a single vkCmdDraw(cmd, 3, 1, 0, 0).
//
// Clip-space positions form a triangle that fills most of the
// viewport: top-center, bottom-right, bottom-left. Vulkan clip space
// has +Y pointing down, so the "top" vertex uses y = -0.5.

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),   // top-center
    vec2(-0.5,  0.5),   // bottom-right
    vec2( 0.5,  0.5)    // bottom-left
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0), // red
    vec3(0.0, 1.0, 0.0), // green
    vec3(0.0, 0.0, 1.0)  // blue
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}