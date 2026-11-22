#include "common.hlsli"

// Masked 재질의 스킨드 섀도맵. 픽셀 셰이더의 알파 판정에 필요한 UV와 정점 색을 함께 넘긴다.
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
    uint4 BoneIDs : BONEIDS;
    float4 Weights : WEIGHTS;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};

struct ObjectData
{
    float4x4 WorldMatrix;
    float4 PickingData;
    float4 HeightfieldData;
};

StructuredBuffer<float4x4> boneMatrices : register(t0, space2);
ConstantBuffer<ObjectData> ubo : register(b0, space2);

VS_OUTPUT main(VS_INPUT input)
{
    float weightSum = input.Weights.x + input.Weights.y + input.Weights.z + input.Weights.w;
    float4x4 skinMatrix;
    uint4 boneIDs = min(input.BoneIDs, uint4(255, 255, 255, 255));
    if (weightSum > 0.0001f)
    {
        skinMatrix =
            input.Weights.x * boneMatrices[boneIDs.x] +
            input.Weights.y * boneMatrices[boneIDs.y] +
            input.Weights.z * boneMatrices[boneIDs.z] +
            input.Weights.w * boneMatrices[boneIDs.w];
    }
    else
    {
        skinMatrix = float4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    VS_OUTPUT output;
    float4 worldPosition = mul(ubo.WorldMatrix, mul(skinMatrix, float4(input.Position, 1.0f)));
    output.Position = mul(commonData.ViewProjection, worldPosition);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    return output;
}
