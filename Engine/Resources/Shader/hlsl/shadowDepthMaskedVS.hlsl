#include "common.hlsli"
#include "instanceData.hlsli"

// Masked 재질의 섀도맵. 픽셀 셰이더의 알파 판정에 필요한 UV와 정점 색을 함께 넘긴다.
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};

VS_OUTPUT main(VS_INPUT input, uint instanceId : SV_InstanceID)
{
    VS_OUTPUT output;
    float4 worldPosition = mul(g_Instances[instanceId].WorldMatrix, float4(input.Position, 1.0f));
    output.Position = mul(commonData.ViewProjection, worldPosition);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    return output;
}
