#include "uiCommon.hlsli"

Texture2D texSampler : register(t0, space1);
SamplerState textureSampler : register(s0, space1);

float4 main(UIVertexOutput input) : SV_TARGET
{
    return texSampler.Sample(textureSampler, input.TexCoord) * input.Color;
}
