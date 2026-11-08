#include "tonemapping.hlsli"

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PostProcessParams
{
    float Exposure;
    float Gamma;
    float _pad0;
    float _pad1;
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<PostProcessParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 color = inputTexture.Sample(inputSampler, input.TexCoord).rgb * gParams.Exposure;
    color = ACESFilmic(color);
    color = pow(color, 1.0f / gParams.Gamma);
    return float4(color, 1.0f);
}