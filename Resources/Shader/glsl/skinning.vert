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

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragTangent;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
} ubo;

// --- 추가된 Bone Matrix Uniform Buffer ---
layout(set = 0, binding = 0) readonly buffer BoneSSBO {
    mat4 boneMatrices[];
} bones;

void main() {
    
    // (1) Bone 가중치를 통한 Skin Matrix 조합
    mat4 skinMatrix = 
        inWeights.x * bones.boneMatrices[inBoneIDs.x] +
        inWeights.y * bones.boneMatrices[inBoneIDs.y] +
        inWeights.z * bones.boneMatrices[inBoneIDs.z] +
        inWeights.w * bones.boneMatrices[inBoneIDs.w];

    // (2) 로컬 버텍스에 SkinMatrix 적용하여 부모 월드(로컬/모델스페이스) 변환 생성
    vec4 localPosition = skinMatrix * vec4(inPosition, 1.0);
    vec3 localNormal   = mat3(skinMatrix) * inNormal;
    vec3 localTangent  = mat3(skinMatrix) * inTangent;
    
    // (3) 월드 및 뷰프로젝션 최종 변환
    gl_Position = commonVertdata.viewProjection * ubo.worldMatrix * localPosition;
    
    fragTexCoord = inTexCoord;
    
    // TBN 관련 행렬 변환
    fragNormal   = mat3(ubo.worldMatrix) * localNormal;
    fragTangent  = mat3(ubo.worldMatrix) * localTangent;
}
