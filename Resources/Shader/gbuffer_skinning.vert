#version 450

#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in vec4 inColor;

layout(location = 6) in uvec4 inBoneIDs;
layout(location = 7) in vec4 inWeights;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec4 outColor;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
} ubo;

layout(set = 0, binding = 0) readonly buffer BoneSSBO {
    mat4 boneMatrices[];
} bones;

void main() {
    mat4 skinMatrix =
        inWeights.x * bones.boneMatrices[inBoneIDs.x] +
        inWeights.y * bones.boneMatrices[inBoneIDs.y] +
        inWeights.z * bones.boneMatrices[inBoneIDs.z] +
        inWeights.w * bones.boneMatrices[inBoneIDs.w];

    vec4 localPosition = skinMatrix * vec4(inPosition, 1.0);
    vec3 localNormal   = mat3(skinMatrix) * inNormal;
    vec3 localTangent  = mat3(skinMatrix) * inTangent;

    vec4 worldPos = ubo.worldMatrix * localPosition;
    outWorldPos   = worldPos.xyz;
    outNormal     = normalize(mat3(ubo.worldMatrix) * localNormal);
    outTexCoord   = inTexCoord;
    outTangent    = normalize(mat3(ubo.worldMatrix) * localTangent);
    outColor      = inColor;
    gl_Position   = commonVertdata.viewProjection * worldPos;
}
