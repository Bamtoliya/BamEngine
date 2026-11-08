#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float4 Color : COLOR;
    uint4 BoneIDs : BONEIDS;
    float4 Weights : WEIGHTS;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : WORLD_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float4 Color : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
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
    // 본 인덱스 상한선 방어 (바인딩된 Identity 본 버퍼 256개 기준 최대 255로 안전 클램핑)
    uint4 safeBoneIDs = min(input.BoneIDs, uint4(255, 255, 255, 255));
    if (weightSum > 0.0001f)
    {
        skinMatrix =
            input.Weights.x * boneMatrices[safeBoneIDs.x] +
            input.Weights.y * boneMatrices[safeBoneIDs.y] +
            input.Weights.z * boneMatrices[safeBoneIDs.z] +
            input.Weights.w * boneMatrices[safeBoneIDs.w];
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
    float3 localNormal = mul((float3x3) skinMatrix, input.Normal);
    float3 localTangent = mul((float3x3) skinMatrix, input.Tangent);
    float4 worldPosition = mul(ubo.WorldMatrix, localPosition);

    VS_OUTPUT output;
    output.Position = mul(commonData.ViewProjection, worldPosition);
    output.WorldPos = worldPosition.xyz;
    output.Normal = normalize(mul((float3x3) ubo.WorldMatrix, localNormal));
    output.TexCoord = input.TexCoord;
    output.Tangent = normalize(mul((float3x3) ubo.WorldMatrix, localTangent));
    output.Color = input.Color;
    output.PickingId = ubo.PickingData.x;
    return output;
}