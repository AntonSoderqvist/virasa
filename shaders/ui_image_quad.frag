#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragTint;
layout(location = 2) flat in uint fragSlot;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 sampled = texture(textures[nonuniformEXT(fragSlot)], fragUV);
    outColor = sampled * fragTint;
}
