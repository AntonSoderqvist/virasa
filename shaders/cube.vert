#version 450
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Matches virasa::Vertex { vec3 position; vec3 normal; vec2 uv; }
struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) readonly buffer IndexBuffer {
    uint indices[];
};

// 96-byte push constant block (vertex+fragment):
//   mat4     mvp                    @  0  (64 bytes)
//   uint64_t vertexBufferAddress    @ 64  (8 bytes)
//   uint64_t indexBufferAddress     @ 72  (8 bytes)
//   uint64_t materialBufferAddress  @ 80  (8 bytes)
//   uint32_t materialId             @ 88  (4 bytes)
//   uint32_t _pad                   @ 92  (4 bytes)
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

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec3 fragWorldNormal;

void main() {
    IndexBuffer ib = IndexBuffer(pc.indexBufferAddress);
    VertexBuffer vb = VertexBuffer(pc.vertexBufferAddress);

    uint vertIdx = ib.indices[gl_VertexIndex];
    Vertex v = vb.vertices[vertIdx];

    vec4 worldPos = pc.model * vec4(v.position, 1.0);
    gl_Position = pc.mvp * vec4(v.position, 1.0);
    fragUV = v.uv;
    fragWorldPos = worldPos.xyz;
    // Assumes uniform scale; for non-uniform scale, use a normal matrix.
    fragWorldNormal = normalize(mat3(pc.model) * v.normal);
}
