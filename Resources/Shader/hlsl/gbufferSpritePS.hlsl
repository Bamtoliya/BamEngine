#include "shading_flags.hlsli"

struct PS_INPUT
{
    float4 Position                 : SV_POSITION;
    float3 WorldPos                 : WORLD_POSITION;
    float3 Normal                   : NORMAL;
    float2 TexCoord                 : TEXCOORD0;
    float3 Tangent                  : TANGENT;
    float4 Color                    : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
};

struct PS_OUTPUT
{
    float4 Diffuse  : SV_TARGET0;
    float4 Normal   : SV_TARGET1;
    float4 PBR      : SV_TARGET2;
    float4 Emission : SV_TARGET3;
    float4 Position : SV_TARGET4;
};

Texture2D texSampler : register(t0, space1);
SamplerState textureSampler : register(s0, space1);

PS_OUTPUT main(PS_INPUT input)
{
    float4 texColor = texSampler.Sample(textureSampler, input.TexCoord) * input.Color;

    clip(texColor.a - 0.01f);
    if (texColor.a < 0.01f) discard;

    PS_OUTPUT output;
    output.Diffuse = texColor;
    // 2D 스프라이트는 Z축을 바라보는 기본 노멀(0, 0, 1 -> 0.5, 0.5, 1.0)과 Unlit 플래그(1.0f) 저장
    output.Normal = float4(0.5f, 0.5f, 1.0f, PackShadingData(SHADING_MODEL_UNLIT, 0u));
    output.PBR = float4(1.0f, 0.0f, 0.0f, 1.0f);
    output.Emission = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.Position = float4(input.WorldPos, input.PickingId);
    return output;
}