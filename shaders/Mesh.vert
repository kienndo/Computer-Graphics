#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, set = 1) uniform UniformBufferObject {
    float gamma;
    vec3 specularColor;
    mat4 mvpMat;
    mat4 mMat;
    mat4 nMat;
    vec4 visibilityFlag;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec2 fragUV;

void main() {

    if (ubo.visibilityFlag.w < 0.5) {
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
            return;
        }

	gl_Position = ubo.mvpMat * vec4(inPosition, 1.0);
	fragPos = (ubo.mMat * vec4(inPosition, 1.0)).xyz;
	fragNorm = (ubo.nMat * vec4(inNorm, 0.0)).xyz;
	fragUV = inUV;
}