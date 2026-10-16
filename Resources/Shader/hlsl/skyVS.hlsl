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
    float3 Direction    : TEXCOORD1;
};

struct ObjectData
{
    float4x4 WorldMatrix;
};

ConstantBuffer<ObjectData> ubo : register(b0, space2);

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4x4 rotOnlyView = commonData.View;
    rotOnlyView[3][0] = 0.0f;
    rotOnlyView[3][1] = 0.0f;
    rotOnlyView[3][2] = 0.0f;

    float4 worldPosition = mul(float4(input.Position, 1.0f), ubo.WorldMatrix);
    float4 viewPosition = mul(worldPosition, rotOnlyView);
    float4 clipPos = mul(viewPosition, commonData.Projection);

    output.Position = float4(clipPos.x, clipPos.y, clipPos.w, clipPos.w);
    output.Direction = input.Position;
    return output;
}
    