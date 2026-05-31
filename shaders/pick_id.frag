#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Picking id fragment shader. Writes the owning entity's generational
// handle into the R32G32_UINT entity-id target as uvec2(index, generation),
// per the encoding pinned by
// virasa.renderer.Types::entity_id_target_encoding_round_trips_through_uint2.
// The push-constant block must match pick_id.vert byte-for-byte; only the
// trailing entityIndex/entityGeneration are read here.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    uint entityIndex;
    uint entityGeneration;
} pc;

layout(location = 0) out uvec2 outId;

void main() {
    outId = uvec2(pc.entityIndex, pc.entityGeneration);
}
