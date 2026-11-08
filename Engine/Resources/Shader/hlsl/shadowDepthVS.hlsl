#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct InstanceData
{
    float4x4 WorldMatrix;
    float4 PickingData;
};

StructuredBuffer<InstanceData> g_Instances : register(t0, space2);

float4 main(VS_INPUT input, uint instanceId : SV_InstanceID) : SV_POSITION
{
    float4x4 world = g_Instances[instanceId].WorldMatrix;
    float4 worldPosition = mul(world, float4(input.Position, 1.0f));
    return mul(commonData.ViewProjection, worldPosition);
}