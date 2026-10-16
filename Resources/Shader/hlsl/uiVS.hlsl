#include "common.hlsli"
#include "uiCommon.hlsli"

struct ObjectData
{
    float4x4 WorldMatrix;
    float4 UvRect;
    float2 UvTiling;
    float2 UvOffset;
    float4 Color;
};

ConstantBuffer<ObjectData> ubo : register(b0, space2);

UIVertexOutput main(uint vertexId : SV_VertexID)
{
    static const float2 positions[6] = {
        float2(0.0f, 1.0f), float2(1.0f, 1.0f), float2(1.0f, 0.0f),
        float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(0.0f, 0.0f)
    };
    static const float2 uvs[6] = {
        float2(0.0f, 1.0f), float2(1.0f, 1.0f), float2(1.0f, 0.0f),
        float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(0.0f, 0.0f)
    };

    UIVertexOutput output;
    float2 position = positions[vertexId];
    output.Position = mul(commonData.ViewProjection, mul(ubo.WorldMatrix, float4(position, 0.0f, 1.0f)));
    output.TexCoord = lerp(ubo.UvRect.xy, ubo.UvRect.zw, uvs[vertexId]) * ubo.UvTiling + ubo.UvOffset;
    output.Color = ubo.Color;
    return output;
}
