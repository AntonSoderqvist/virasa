#version 450

// Depth-only shadow pass: there is no color attachment, so the fragment
// shader produces no outputs. Rasterization still writes gl_FragCoord.z to
// the depth attachment, which is exactly the shadow map this pass renders.
void main() {
}
