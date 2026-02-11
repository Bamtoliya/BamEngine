#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 fragTexCoord;

layout(set = 1, binding = 0) uniform UBO {
    mat4 worldMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
} ubo;

void main() {
    gl_Position = ubo.projMatrix * ubo.viewMatrix * ubo.worldMatrix * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}