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
    return mul(mul(float4(input.Position, 1.0f), ubo.WorldMatrix), commonData.ViewProjection);
}