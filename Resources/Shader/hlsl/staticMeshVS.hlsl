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


struct ObjectData
{
    matrix WorldMatrix;
};

ConstantBuffer<ObjectData> ubo : register(b0, space2);

VS_OUTPUT main(VS_INPUT input) 
{
    VS_OUTPUT output;

    matrix MVP = mul(ubo.WorldMatrix, commonData.ViewProjection);

    output.Position = mul(float4(input.Position, 1.0f), MVP);
    output.Normal = mul(float4(input.Normal, 0.0f), ubo.WorldMatrix).xyz;
    output.TexCoord = input.TexCoord;
    output.Tangent = mul(float4(input.Tangent, 0.0f), ubo.WorldMatrix).xyz;
    output.Binormal = mul(float4(input.Binormal, 0.0f), ubo.WorldMatrix).xyz;
    output.Color = input.Color;

    return output;
}