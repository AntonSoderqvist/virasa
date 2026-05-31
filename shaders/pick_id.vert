#version 450
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Picking id vertex shader. Transforms a mesh vertex (fetched by
// buffer-device-address, exactly like cube.vert) into clip space so the
// paired pick_id.frag can write the owning entity id into the
// R32G32_UINT entity-id target. Drawn non-indexed with
// vkCmdDraw(indexCount): gl_VertexIndex selects the index, which selects
// the vertex.
//
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

// 88-byte PickPushConstants block (vertex+fragment):
//   mat4     mvp                  @  0  (64 bytes)
//   uint64_t vertexBufferAddress  @ 64  (8 bytes)
//   uint64_t indexBufferAddress   @ 72  (8 bytes)
//   uint32_t entityIndex          @ 80  (4 bytes)
//   uint32_t entityGeneration     @ 84  (4 bytes)
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    uint entityIndex;
    uint entityGeneration;
} pc;

void main() {
    IndexBuffer ib = IndexBuffer(pc.indexBufferAddress);
    VertexBuffer vb = VertexBuffer(pc.vertexBufferAddress);

    uint vertIdx = ib.indices[gl_VertexIndex];
    Vertex v = vb.vertices[vertIdx];

    gl_Position = pc.mvp * vec4(v.position, 1.0);
}
