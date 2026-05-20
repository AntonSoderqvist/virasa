#version 450

// Hello-triangle fragment shader.
//
// Receives the interpolated per-vertex color from the vertex stage
// and writes it to the single color attachment at location 0. The
// Pipeline contract creates exactly one color attachment
// (colorAttachmentCount = 1) using dynamic rendering, so a single
// out variable at location 0 is correct.

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0);
}