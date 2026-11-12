#include "common.hlsli"

struct VS_INPUT
{
    float3 Position     : POSITION;
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD0;
    float3 Tangent      : TANGENT;
    float3 Binormal    : BINORMAL;
    float4 Color        : COLOR;
};

struct VS_OUTPUT
{
    float4 Position     : SV_POSITION;
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD0;
    float3 Tangent      : TANGENT;
    float3 Binormal    : BINORMAL;
    float4 Color        : COLOR;
};

struct InstanceData
{
    float4x4 WorldMatrix;
    float4 PickingData;
};

StructuredBuffer<InstanceData> g_Instances : register(t0, space2);

VS_OUTPUT main(VS_INPUT input, uint instanceId : SV_InstanceID)
{
    VS_OUTPUT output;
    float4x4 world = g_Instances[instanceId].WorldMatrix;
    float4 worldPosition = mul(world, float4(input.Position, 1.0f));

    output.Position = mul(commonData.ViewProjection, worldPosition);
    output.Normal = normalize(mul(world, float4(input.Normal, 0.0f)).xyz);
    output.TexCoord = input.TexCoord;
    output.Tangent = mul(world, float4(input.Tangent, 0.0f)).xyz;
    output.Binormal = mul(world, float4(input.Binormal, 0.0f)).xyz;
    output.Color = input.Color;
    return output;
}