#version 450

// Generates a single full-screen triangle from gl_VertexIndex with no
// vertex input. Draw with vkCmdDraw(3, 1, 0, 0). The triangle's three
// clip-space corners (-1,-1), (3,-1), (-1,3) cover the whole viewport;
// the composite fragment shader derives pixel coordinates from
// gl_FragCoord, so no varyings are emitted.
void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
