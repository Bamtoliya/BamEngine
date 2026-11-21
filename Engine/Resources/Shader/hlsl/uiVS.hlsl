#include "common.hlsli"
#include "uiCommon.hlsli"

struct ObjectData
{
    float4x4 WorldMatrix;
    float4 UvRect;
    float2 UvTiling;
    float2 UvOffset;
    float4 Color;
    float4 EllipseMaskPlanes[8];
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
    float4 world = mul(ubo.WorldMatrix, float4(position, 0.0f, 1.0f));
    output.Position = mul(commonData.ViewProjection, world);
    float3 canvas = float3(world.xy, 1.0f);
    output.Ellipse01 = float4(dot(canvas, ubo.EllipseMaskPlanes[0].xyz), dot(canvas, ubo.EllipseMaskPlanes[1].xyz),
        dot(canvas, ubo.EllipseMaskPlanes[2].xyz), dot(canvas, ubo.EllipseMaskPlanes[3].xyz));
    output.Ellipse23 = float4(dot(canvas, ubo.EllipseMaskPlanes[4].xyz), dot(canvas, ubo.EllipseMaskPlanes[5].xyz),
        dot(canvas, ubo.EllipseMaskPlanes[6].xyz), dot(canvas, ubo.EllipseMaskPlanes[7].xyz));
    output.TexCoord = lerp(ubo.UvRect.xy, ubo.UvRect.zw, uvs[vertexId]) * ubo.UvTiling + ubo.UvOffset;
    output.Color = ubo.Color;
    return output;
}
