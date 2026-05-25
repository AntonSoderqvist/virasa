#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    float invHalfW;
    float invHalfH;
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = vec4(inPos.x * pc.invHalfW - 1.0, inPos.y * pc.invHalfH - 1.0, 0.0, 1.0);
    fragColor = inColor;
}
