#version 450

#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragTangent;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
} ubo;

void main() {
    gl_Position = commondata.viewProjection * ubo.worldMatrix * vec4(inPosition, 1.0);
    
    fragTexCoord = inTexCoord;
    fragNormal = mat3(ubo.worldMatrix) * inNormal;
}