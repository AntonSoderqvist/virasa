#version 450
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Matches virasa::Vertex { vec3 position; vec3 normal; vec4 tangent; vec2 uv; }
// (48 bytes, scalar layout — offsets: pos=0, normal=12, tangent=24, uv=40)
struct Vertex {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) readonly buffer IndexBuffer {
    uint indices[];
};

// 80-byte ShadowPushConstants (vertex only):
//   mat4     lightMvp               @  0  (64 bytes) = lightViewProj * model
//   uint64_t vertexBufferAddress    @ 64  (8 bytes)
//   uint64_t indexBufferAddress     @ 72  (8 bytes)
//
// lightMvp is pre-multiplied per drawable on the CPU (this light's
// world->light-clip matrix times the drawable's world model matrix), so the
// depth pass transforms model-space vertices straight to light clip space.
layout(push_constant) uniform ShadowPushConstants {
    mat4 lightMvp;
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
} pc;

void main() {
    IndexBuffer ib = IndexBuffer(pc.indexBufferAddress);
    VertexBuffer vb = VertexBuffer(pc.vertexBufferAddress);

    uint vertIdx = ib.indices[gl_VertexIndex];
    Vertex v = vb.vertices[vertIdx];

    gl_Position = pc.lightMvp * vec4(v.position, 1.0);
}
