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
    float3 axisX = float3(world[0][0], world[1][0], world[2][0]);
    float3 axisY = float3(world[0][1], world[1][1], world[2][1]);
    float3 axisZ = float3(world[0][2], world[1][2], world[2][2]);

    float3 normalWorld = input.Normal.x * cross(axisY, axisZ) +
                        input.Normal.y * cross(axisZ, axisX) +
                        input.Normal.z * cross(axisX, axisY);

    float determinantSign = dot(axisX, cross(axisY, axisZ)) < 0.0f ? -1.0f : 1.0f;
    output.Normal = normalize(normalWorld * determinantSign);
    output.TexCoord = input.TexCoord;
    output.Tangent = normalize(mul(world, float4(input.Tangent, 0.0f)).xyz);
    output.Color = input.Color;
    output.PickingId = g_Instances[instanceId].PickingData.x;

    return output;
}