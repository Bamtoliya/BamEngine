#version 450

#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 fragTexCoord;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
} ubo;

void main() {
    gl_Position = commondata.viewProjection * ubo.worldMatrix * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}