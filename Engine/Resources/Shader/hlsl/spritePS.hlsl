#include "common.hlsli"

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float2 TexCoord     : TEXCOORD0;
    float4 Color        : COLOR;
};

struct PS_OUTPUT
{
    float4 Color : SV_TARGET;
};

Texture2D uTexture : register(t0, space1);
SamplerState samplerState : register(s0, space1);

float4 main(PS_INPUT input) : SV_TARGET
{
    return input.Color * uTexture.Sample(samplerState, input.TexCoord);
}