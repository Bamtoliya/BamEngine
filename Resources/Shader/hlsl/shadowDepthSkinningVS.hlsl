#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    uint4 BoneIDs : BLENDINDICES;
    float4 Weights : BLENDWEIGHT;
};

struct ObjectData
{
    float4x4 WorldMatrix;
};

StructuredBuffer<float4x4> boneMatrices : register(t0, space2);
ConstantBuffer<ObjectData> ubo : register(b0, space2);

float4 main(VS_INPUT input) : SV_POSITION
{
    float4x4 skinMatrix =
        input.Weights.x * boneMatrices[input.BoneIDs.x] +
        input.Weights.y * boneMatrices[input.BoneIDs.y] +
        input.Weights.z * boneMatrices[input.BoneIDs.z] +
        input.Weights.w * boneMatrices[input.BoneIDs.w];
    float4 localPosition = mul(float4(input.Position, 1.0f), skinMatrix);
    return mul(mul(localPosition, ubo.WorldMatrix), commonData.ViewProjection);
}