#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNorm;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject {
	vec3 lightPos;
	vec4 lightColor;
	float decayFactor;
	float g;
	vec3 ambientLightColor;
	vec3 eyePos;
} gubo;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    float gamma;
    vec3 specularColor;
    mat4 mvpMat;
    mat4 mMat;
    mat4 nMat;
    vec4 highlight;
} ubo;

layout(set = 1, binding = 1) uniform sampler2D tex;

void main() {
	vec3 Norm = normalize(fragNorm);
	vec3 EyeDir = normalize(gubo.eyePos - fragPos);

	vec3 LightDir = gubo.lightPos - fragPos;
	float LightDistance = length(LightDir);
	LightDir = normalize(LightDir);

	vec3 LightModel = gubo.lightColor.rgb * pow((gubo.g / LightDistance), gubo.decayFactor);

	vec3 albedo = texture(tex, fragUV).rgb;
	vec3 MD = albedo;

	vec3 Diffuse = MD * clamp(dot(LightDir, Norm), 0.0f, 1.0f);

	vec3 MS = ubo.specularColor;
	vec3 Specular = MS * pow(clamp(dot(Norm, normalize(LightDir + EyeDir)), 0.0f, 1.0f), ubo.gamma);

	vec3 MA = albedo;
    vec3 Ambient = MA * gubo.ambientLightColor;

	outColor = vec4(LightModel * (Diffuse + Specular) + Ambient, 1.0f);
}