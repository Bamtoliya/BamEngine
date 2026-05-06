#version 450

#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 6) in uvec4 inBoneIDs;
layout(location = 7) in vec4  inWeights;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
} ubo;

layout(set = 0, binding = 0) readonly buffer BoneSSBO {
    mat4 boneMatrices[];
} bones;

void main()
{
    mat4 skinMatrix =
        inWeights.x * bones.boneMatrices[inBoneIDs.x] +
        inWeights.y * bones.boneMatrices[inBoneIDs.y] +
        inWeights.z * bones.boneMatrices[inBoneIDs.z] +
        inWeights.w * bones.boneMatrices[inBoneIDs.w];

    vec4 worldPos = ubo.worldMatrix * (skinMatrix * vec4(inPosition, 1.0));
    gl_Position   = commonVertdata.viewProjection * worldPos;
}