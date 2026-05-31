#version 450

// Picking clear fragment shader. Paired with fullscreen.vert, it seeds the
// entire R32G32_UINT entity-id target with the invalid-entity sentinel
// uvec2(0xFFFFFFFF, 0) — the encoding of virasa::ecs::Entity::Invalid() —
// before the pick_id pass rasterizes real entity ids over it. This replaces
// a LoadOp::Clear, which cannot express an integer clear through the render
// graph's float-valued ClearColor.
layout(location = 0) out uvec2 outId;

void main() {
    outId = uvec2(0xFFFFFFFFu, 0u);
}
