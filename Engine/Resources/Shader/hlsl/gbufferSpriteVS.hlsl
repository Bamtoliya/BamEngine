#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR;
};

struct VS_OUTPUT
{
    float4 Position                 : SV_POSITION;
    float3 WorldPos                 : WORLD_POSITION;
    float3 Normal                   : NORMAL;
    float2 TexCoord                 : TEXCOORD0;
    float3 Tangent                  : TANGENT;
    float4 Color                    : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
};

struct ObjectData
{
    float4x4 WorldMatrix;
    float4 Color;
    float4 UVTransform;
    float4 PickingData;
};

ConstantBuffer<ObjectData> ubo : register(b0, space2);

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 worldPosition = mul(ubo.WorldMatrix, float4(input.Position, 1.0f));

    output.Position = mul(commonData.ViewProjection, worldPosition);
    output.WorldPos = worldPosition.xyz;
    output.Normal = normalize(mul((float3x3)ubo.WorldMatrix, input.Normal));
    output.TexCoord = ubo.UVTransform.xy + input.TexCoord * ubo.UVTransform.zw;
    output.Tangent = normalize(mul((float3x3)ubo.WorldMatrix, float3(1.0f, 0.0f, 0.0f)));
    output.Color = input.Color * ubo.Color;
    output.PickingId = ubo.PickingData.x;

    return output;
}