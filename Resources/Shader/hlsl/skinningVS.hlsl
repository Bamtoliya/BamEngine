#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    uint4 BoneIDs : BLENDINDICES;
    float4 Weights : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float3 Tangent : TEXCOORD2;
};

struct ObjectData
{
    float4x4 WorldMatrix;
};

StructuredBuffer<float4x4> boneMatrices : register(t0, space2);
ConstantBuffer<ObjectData> ubo : register(b0, space2);

VS_OUTPUT main(VS_INPUT input)
{
    float4x4 skinMatrix =
        input.Weights.x * boneMatrices[input.BoneIDs.x] +
        input.Weights.y * boneMatrices[input.BoneIDs.y] +
        input.Weights.z * boneMatrices[input.BoneIDs.z] +
        input.Weights.w * boneMatrices[input.BoneIDs.w];
    float4 localPosition = mul(float4(input.Position, 1.0f), skinMatrix);
    float3 localNormal = mul(float4(input.Normal, 0.0f), skinMatrix).xyz;
    float3 localTangent = mul(float4(input.Tangent, 0.0f), skinMatrix).xyz;

    VS_OUTPUT output;
    float4 worldPosition = mul(localPosition, ubo.WorldMatrix);
    output.Position = mul(worldPosition, commonData.ViewProjection);
    output.TexCoord = input.TexCoord;
    output.Normal = mul(float4(localNormal, 0.0f), ubo.WorldMatrix).xyz;
    output.Tangent = mul(float4(localTangent, 0.0f), ubo.WorldMatrix).xyz;
    return output;
}