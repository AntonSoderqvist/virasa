#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Outline composite pass. Drawn as a full-screen triangle over the scene
// color (LoadOp Load, source-over blend). For each pixel outside every
// highlighted silhouette, looks up the nearest silhouette seed from the
// final jump-flood image and, when that seed is within widthPixels, emits the
// border color (the mask RGB at the seed) with the seed's mask alpha as
// opacity. Pixels inside a silhouette, or farther than widthPixels from any
// seed, are discarded so the scene color shows through (border, not fill).
//
// The jump-flood and mask images are sampled through binding-0 combined
// image samplers registered with a nearest, clamp-to-edge sampler, so the
// stored seed coordinates are read without interpolation.
layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PC {
    uint  extentX;
    uint  extentY;
    uint  jfaSlot;      // binding-0 slot of the final jump-flood image
    uint  maskSlot;     // binding-0 slot of the outline mask
    float widthPixels;  // border half-width in pixels
    float pad0;
    float pad1;
    float pad2;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 ext = vec2(pc.extentX, pc.extentY);
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec2 uv = (vec2(p) + 0.5) / ext;

    vec4 m = texture(textures[nonuniformEXT(pc.maskSlot)], uv);
    if (m.a > 0.0) {
        discard; // inside a highlighted silhouette: leave the scene untouched
    }

    vec2 seed = texture(textures[nonuniformEXT(pc.jfaSlot)], uv).rg;
    if (seed.x < 0.0) {
        discard; // no seed reached this pixel
    }

    float dist = distance(vec2(p), seed);
    if (dist <= 0.0 || dist > pc.widthPixels) {
        discard; // outside the border band
    }

    vec2 seedUv = (seed + 0.5) / ext;
    vec4 seedMask = texture(textures[nonuniformEXT(pc.maskSlot)], seedUv);
    outColor = vec4(seedMask.rgb, seedMask.a);
}
