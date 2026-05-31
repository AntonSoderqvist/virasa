#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Outline-mask fragment shader. Paired with cube.vert (which transforms a
// mesh by the 192-byte forward PushConstants). Rendered without depth state
// (x-ray) into the R8G8B8A8_UNORM outline mask: RGB carries the per-drawable
// border color (highlight.xyz) and alpha carries the highlight intensity
// (highlight.w), which the jump-flood seed pass treats as silhouette
// coverage (alpha > 0) and the composite pass uses as the border opacity.
//
// The push-constant block must match cube.vert's layout byte-for-byte so the
// two stages agree; only the trailing highlight Vec4 is read here.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    uint64_t materialBufferAddress;
    uint64_t lightBufferAddress;
    uint64_t shadowBufferAddress;
    uint materialId;
    uint lightCount;
    vec4 highlight;
} pc;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragWorldNormal;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(pc.highlight.rgb, clamp(pc.highlight.w, 0.0, 1.0));
}
