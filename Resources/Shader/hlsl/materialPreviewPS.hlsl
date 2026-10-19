#include "common.hlsli"

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float3 WorldPos     : WORLD_POSITION;
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD0;
    float3 Tangent      : TANGENT;
    float4 Color        : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
};

Texture2D diffuseMap : register(t0, space1);
SamplerState diffuseSampler : register(s0, space1);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 albedo = diffuseMap.Sample(diffuseSampler, input.TexCoord) * input.Color;
    float3 N = normalize(input.Normal);
    float3 V = normalize(commonData.CameraPos.xyz - input.WorldPos);

    // Studio 3-Point Lighting
    // 1. Key Light (우측 상단 메인 라이트)
    float3 keyDir = normalize(float3(0.5f, 1.0f, -0.6f));
    float NdotL1 = max(0.0f, dot(N, keyDir));
    float3 H1 = normalize(keyDir + V);
    float spec1 = pow(max(0.0f, dot(N, H1)), 32.0f) * 0.45f;

    // 2. Fill Light (좌측 하단 보조광: 그림자 완화)
    float3 fillDir = normalize(float3(-0.6f, -0.2f, -0.4f));
    float NdotL2 = max(0.0f, dot(N, fillDir)) * 0.25f;

    // 3. Rim Light & Ambient (외곽 실루엣 강조 및 환경광)
    float rim = pow(1.0f - max(0.0f, dot(N, V)), 3.0f) * 0.3f;
    float3 ambient = float3(0.2f, 0.22f, 0.26f);

    float3 lighting = (NdotL1 * 0.8f + NdotL2 + ambient + rim);
    float3 finalColor = albedo.rgb * lighting + spec1;

    return float4(finalColor, albedo.a);
}