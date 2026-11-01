#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    uint4 BoneIDs : BONEIDS;
    float4 Weights : WEIGHTS;
};

struct ObjectData
{
    float4x4 WorldMatrix;
    float4 PickingData;
    float4 HeightfieldData;
};

StructuredBuffer<float4x4> boneMatrices : register(t0, space2);
ConstantBuffer<ObjectData> ubo : register(b0, space2);

float4 main(VS_INPUT input) : SV_POSITION
{
    float weightSum = input.Weights.x + input.Weights.y + input.Weights.z + input.Weights.w;
    float4x4 skinMatrix;
    if (weightSum > 0.0001f)
    {
        skinMatrix =
            input.Weights.x * boneMatrices[input.BoneIDs.x] +
            input.Weights.y * boneMatrices[input.BoneIDs.y] +
            input.Weights.z * boneMatrices[input.BoneIDs.z] +
            input.Weights.w * boneMatrices[input.BoneIDs.w];
    }
    else
    {
        skinMatrix = float4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    float4 localPosition = mul(skinMatrix, float4(input.Position, 1.0f));
    float4 worldPosition = mul(ubo.WorldMatrix, localPosition);
    return mul(commonData.ViewProjection, worldPosition);
}