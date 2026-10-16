#include "common.hlsli"

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD0;
    float3 Tangent      : TANGENT;
    float3 Binormal    : BINORMAL;
    float4 Color        : COLOR;
};

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}