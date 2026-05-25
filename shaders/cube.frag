#version 450
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Matches virasa::PBRFactors (64 bytes, scalar layout).
struct PBRFactors {
    vec4  baseColorFactor;     // @  0
    vec3  emissiveFactor;      // @ 16
    float metallicFactor;      // @ 28
    float roughnessFactor;     // @ 32
    float normalScale;         // @ 36
    float occlusionStrength;   // @ 40
    float alphaCutoff;         // @ 44
    uint  alphaModeBits;       // @ 48
    float _pad0;               // @ 52
    float _pad1;               // @ 56
    float _pad2;               // @ 60
};

// Matches virasa::VisualMaterialGPU (96 bytes, scalar layout).
struct VisualMaterialGPU {
    PBRFactors factors;                // 64 bytes
    uint baseColorTexture;             // @ 64
    uint normalTexture;                // @ 68
    uint metallicRoughnessTexture;     // @ 72
    uint occlusionTexture;             // @ 76
    uint emissiveTexture;              // @ 80
    uint _pad3;                        // @ 84
    uint _pad4;                        // @ 88
    uint _pad5;                        // @ 92
};

layout(buffer_reference, scalar) readonly buffer MaterialTable {
    VisualMaterialGPU materials[];
};

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    uint64_t materialBufferAddress;
    uint64_t lightBufferAddress;
    uint materialId;
    uint lightCount;
} pc;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    MaterialTable mt = MaterialTable(pc.materialBufferAddress);
    VisualMaterialGPU mat = mt.materials[pc.materialId];

    // materialId is push-constant => dynamically uniform; plain indexing is valid.
    vec4 sampled = texture(textures[mat.baseColorTexture], fragUV);
    vec3 albedo = sampled.rgb * mat.factors.baseColorFactor.rgb;

    outColor = vec4(albedo, 1.0);
}
