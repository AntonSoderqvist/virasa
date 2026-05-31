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

// Matches virasa::LightGPU (64 bytes, scalar layout).
//   0: Directional, 1: Point, 2: Spot
struct LightGPU {
    vec3  position;       // @  0
    uint  type;           // @ 12
    vec3  direction;      // @ 16
    float range;          // @ 28
    vec3  color;          // @ 32  (linear RGB * intensity)
    float innerConeCos;   // @ 44
    float outerConeCos;   // @ 48
    float _pad0;          // @ 52
    float _pad1;          // @ 56
    float _pad2;          // @ 60
};

layout(buffer_reference, scalar) readonly buffer LightTable {
    LightGPU lights[];
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
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragWorldNormal;
layout(location = 0) out vec4 outColor;

void main() {
    MaterialTable mt = MaterialTable(pc.materialBufferAddress);
    VisualMaterialGPU mat = mt.materials[pc.materialId];

    vec4 sampled = texture(textures[mat.baseColorTexture], fragUV);
    vec4 baseColor = sampled * mat.factors.baseColorFactor;
    vec3 albedo = baseColor.rgb;

    // Alpha mode (mirrors virasa::AlphaMode: 0 Opaque, 1 Mask, 2 Blend).
    // Mask discards fragments below the cutoff so the surface reads as a
    // hard-edged cutout; a surviving Mask fragment is fully opaque since the
    // Opaque/Mask pipeline keeps depth writes on and blending off. Blend
    // keeps the base color alpha for source-over compositing in the
    // depth-write-disabled blend pipeline. Opaque forces alpha to 1.
    uint alphaMode = mat.factors.alphaModeBits;
    if (alphaMode == 1u && baseColor.a < mat.factors.alphaCutoff) {
        discard;
    }
    float outAlpha = (alphaMode == 2u) ? baseColor.a : 1.0;

    vec3 N = normalize(fragWorldNormal);

    // Small ambient term so unlit faces aren't pure black.
    vec3 lit = albedo * 0.03;

    if (pc.lightBufferAddress != 0ul && pc.lightCount > 0u) {
        LightTable lt = LightTable(pc.lightBufferAddress);
        for (uint i = 0u; i < pc.lightCount; ++i) {
            LightGPU L = lt.lights[i];

            vec3  Ldir;
            float atten = 1.0;

            if (L.type == 0u) {
                // Directional: L.direction points FROM the light. Vector to light is -direction.
                Ldir = normalize(-L.direction);
            } else {
                vec3 toLight = L.position - fragWorldPos;
                float dist = length(toLight);
                Ldir = toLight / max(dist, 1e-5);

                // Smooth range falloff (range == 0 means unbounded).
                if (L.range > 0.0) {
                    float x = clamp(1.0 - (dist / L.range), 0.0, 1.0);
                    atten = x * x;
                } else {
                    atten = 1.0 / max(dist * dist, 1e-4);
                }

                if (L.type == 2u) {
                    // Spot: L.direction is cone axis (from light outward).
                    float cd = dot(normalize(-L.direction), Ldir);
                    float spot = clamp(
                        (cd - L.outerConeCos) /
                        max(L.innerConeCos - L.outerConeCos, 1e-5),
                        0.0, 1.0);
                    atten *= spot;
                }
            }

            float NdotL = max(dot(N, Ldir), 0.0);
            lit += albedo * L.color * NdotL * atten;
        }
    }

    lit += mat.factors.emissiveFactor;
    outColor = vec4(lit, outAlpha);
}
