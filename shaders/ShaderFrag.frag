#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = vec3(0.4, 0.8, 1.0); // light blue
    outColor = vec4(color * max(dot(normalize(fragNormal), vec3(0,0,1)), 0.3), 1.0);
}
