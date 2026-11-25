#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct ObjectData
{
    float4x4 WorldMatrix;
    float4 PickingData;
    float4 HeightfieldData;
};

ConstantBuffer<ObjectData> ubo : register(b0, space2);
Texture2D<float> heightMap : register(t8, space3); // 머티리얼 범위(t0~t7, space1) 밖. BINDING_LAYOUT.md "지형 바인딩"
Texture2D<float> holeMask : register(t11, space3); // 구멍 마스크 (1=Hole, 0=Solid)

float4 main(VS_INPUT input, uint vertexId : SV_VertexID) : SV_POSITION
{
    uint width, height;
    heightMap.GetDimensions(width, height);

    const int2 lastIndex = int2(width, height) - 1;
    const float2 cellSpacing = max(ubo.HeightfieldData.yz, float2(1e-6f, 1e-6f));
    const float2 gridCoord = input.Position.xz / cellSpacing + 0.5f * float2(lastIndex);
    const int2 index = clamp(int2(round(gridCoord)), int2(0, 0), lastIndex);
    if (holeMask.Load(int3(index, 0)) >= 0.5f)
        return asfloat(0x7fc00000).xxxx;

    float3 position = input.Position;
    position.y = heightMap.Load(int3(index, 0)) * ubo.HeightfieldData.x + input.Position.y;

    const float4 worldPosition = mul(ubo.WorldMatrix, float4(position, 1.0f));
    return mul(commonData.ViewProjection, worldPosition);
}