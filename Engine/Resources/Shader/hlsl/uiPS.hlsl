#include "uiCommon.hlsli"

#if defined(__spirv__) && !defined(BAM_UI_WGSL)
// SDL GPU reflects combined fragment samplers at consecutive bindings in set 2.
[[vk::combinedImageSampler]][[vk::binding(0, 2)]] Texture2D texSampler;
[[vk::combinedImageSampler]][[vk::binding(0, 2)]] SamplerState textureSampler;
[[vk::combinedImageSampler]][[vk::binding(1, 2)]] Texture2D alphaMask0;
[[vk::combinedImageSampler]][[vk::binding(1, 2)]] SamplerState alphaSampler0;
[[vk::combinedImageSampler]][[vk::binding(2, 2)]] Texture2D alphaMask1;
[[vk::combinedImageSampler]][[vk::binding(2, 2)]] SamplerState alphaSampler1;
#else
// Tint's SPIR-V reader needs the images and shared sampler at distinct bindings.
#ifdef __spirv__
[[vk::binding(3, 2)]]
#endif
SamplerState textureSampler : register(s0, space1);
#ifdef __spirv__
[[vk::binding(0, 2)]]
#endif
Texture2D texSampler : register(t0, space1);
#ifdef __spirv__
[[vk::binding(1, 2)]]
#endif
Texture2D alphaMask0 : register(t1, space1);
#ifdef __spirv__
[[vk::binding(2, 2)]]
#endif
Texture2D alphaMask1 : register(t2, space1);
#define alphaSampler0 textureSampler
#define alphaSampler1 textureSampler
#endif

bool IsInsideMask(float2 local)
{
    return all(local >= 0.0f) && all(local <= 1.0f);
}

float2 AlphaMaskUv(float2 local, float4 uvTransform, float4 uvClamp)
{
    return clamp(uvTransform.xy + local * uvTransform.zw, uvClamp.xy, uvClamp.zw);
}

float4 main(UIVertexOutput input) : SV_TARGET
{
    const float4 radiusSquared = float4(dot(input.Ellipse01.xy, input.Ellipse01.xy),
        dot(input.Ellipse01.zw, input.Ellipse01.zw), dot(input.Ellipse23.xy, input.Ellipse23.xy),
        dot(input.Ellipse23.zw, input.Ellipse23.zw));
    float4 color = texSampler.Sample(textureSampler, input.TexCoord) * input.Color;
    clip(1.0f - radiusSquared);
    if (input.AlphaMask0Uv.z > 0.0f)
    {
        float2 local = input.AlphaMaskLocal.xy;
        float2 uv = AlphaMaskUv(local, input.AlphaMask0Uv, input.AlphaMask0Clamp);
        color.a *= IsInsideMask(local) ? alphaMask0.SampleLevel(alphaSampler0, uv, 0.0f).a : 0.0f;
    }
    if (input.AlphaMask1Uv.z > 0.0f)
    {
        float2 local = input.AlphaMaskLocal.zw;
        float2 uv = AlphaMaskUv(local, input.AlphaMask1Uv, input.AlphaMask1Clamp);
        color.a *= IsInsideMask(local) ? alphaMask1.SampleLevel(alphaSampler1, uv, 0.0f).a : 0.0f;
    }
    return color;
}
