#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
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

struct SpriteInstanceData
{
    float4x4 WorldMatrix;
    float4 Color;
    float4 UVTransform;
    float4 PickingData;
};

StructuredBuffer<SpriteInstanceData> g_SpriteInstances : register(t0, space2);

VS_OUTPUT main(VS_INPUT input, uint instanceId : SV_InstanceID)
{
    VS_OUTPUT output;

    SpriteInstanceData inst = g_SpriteInstances[instanceId];
    float4 worldPosition = mul(inst.WorldMatrix, float4(input.Position, 1.0f));

    output.Position = mul(commonData.ViewProjection, worldPosition);
    output.WorldPos = worldPosition.xyz;
    output.Normal = normalize(mul((float3x3) inst.WorldMatrix, input.Normal));
    output.TexCoord = inst.UVTransform.xy + input.TexCoord * inst.UVTransform.zw;
    output.Tangent = normalize(mul((float3x3) inst.WorldMatrix, float3(1.0f, 0.0f, 0.0f)));
    output.Color = input.Color * inst.Color;
    output.PickingId = inst.PickingData.x;

    return output;
}