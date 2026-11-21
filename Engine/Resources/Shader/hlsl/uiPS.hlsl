#include "uiCommon.hlsli"

Texture2D texSampler : register(t0, space1);
SamplerState textureSampler : register(s0, space1);

float4 main(UIVertexOutput input) : SV_TARGET
{
    const float4 radiusSquared = float4(dot(input.Ellipse01.xy, input.Ellipse01.xy),
        dot(input.Ellipse01.zw, input.Ellipse01.zw), dot(input.Ellipse23.xy, input.Ellipse23.xy),
        dot(input.Ellipse23.zw, input.Ellipse23.zw));
    const float4 color = texSampler.Sample(textureSampler, input.TexCoord) * input.Color;
    clip(1.0f - radiusSquared);
    return color;
}
