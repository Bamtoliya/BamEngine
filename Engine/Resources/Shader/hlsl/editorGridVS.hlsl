#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct ObjectData
{
    float4x4 WorldMatrix;
};

ConstantBuffer<ObjectData> ubo : register(b0, space2);

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 worldPosition = mul(float4(input.Position, 1.0f), ubo.WorldMatrix);
    output.Position = mul(worldPosition, commonData.ViewProjection);
    output.TexCoord = input.TexCoord;
    return output;
}