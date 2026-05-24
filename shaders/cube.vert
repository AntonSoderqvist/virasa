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

// Push constants (vertex stage, offset 0, size 80):
//   mat4 mvp                     -- pre-computed proj * view * model
//   uint64_t vertexBufferAddress -- device address of the vertex buffer
//   uint64_t indexBufferAddress  -- device address of the index buffer
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
} pc;

layout(location = 0) out vec3 fragColor;

const vec3 faceColors[6] = vec3[6](
    vec3(0.95, 0.25, 0.25),
    vec3(0.25, 0.85, 0.30),
    vec3(0.25, 0.45, 0.95),
    vec3(0.95, 0.85, 0.25),
    vec3(0.85, 0.30, 0.85),
    vec3(0.25, 0.85, 0.95)
);

void main() {
    IndexBuffer ib = IndexBuffer(pc.indexBufferAddress);
    VertexBuffer vb = VertexBuffer(pc.vertexBufferAddress);

    uint vertIdx = ib.indices[gl_VertexIndex];
    vec3 pos = vb.vertices[vertIdx].position;
    gl_Position = pc.mvp * vec4(pos, 1.0);

    // 6 indices per face (2 triangles); gl_VertexIndex / 6 gives face 0..5
    fragColor = faceColors[clamp(gl_VertexIndex / 6, 0, 5)];
}
