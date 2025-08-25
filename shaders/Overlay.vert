// Overlay.vert
#version 450

layout(location = 0) in vec2 inPos;   // must exist to consume attribute 0
layout(location = 1) in vec2 inUV;    // must exist to consume attribute 1

layout(location = 0) out vec2 vUV;

void main() {
    vUV = inUV;
    gl_Position = vec4(inPos, 0.0, 1.0);  // NDC already (since you built a screen quad)
}
