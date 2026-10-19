#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct ObjectData
{
    float4x4 WorldMatrix;
};

ConstantBuffer<ObjectData> ubo : register(b0, space2);

float4 main(VS_INPUT input) : SV_POSITION
{
    float4 worldPosition = mul(ubo.WorldMatrix, float4(input.Position, 1.0f));
    return mul(commonData.ViewProjection, worldPosition);
}