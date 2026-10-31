#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
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
    output.WorldPos = worldPosition.xyz;

    // 비균일/음수 스케일 역전치 법선 보정 (외적을 통한 여인수 기저 계산)
    float3 axisX = world[0].xyz;
    float3 axisY = world[1].xyz;
    float3 axisZ = world[2].xyz;
    float3 normalWorld = input.Normal.x * cross(axisY, axisZ) +
                         input.Normal.y * cross(axisZ, axisX) +
                         input.Normal.z * cross(axisX, axisY);

    output.Normal = normalize(normalWorld);
    output.TexCoord = input.TexCoord;
    output.Tangent = normalize(mul(world, float4(input.Tangent, 0.0f)).xyz);
    output.Color = input.Color;
    output.PickingId = g_Instances[instanceId].PickingData.x;

    return output;
}