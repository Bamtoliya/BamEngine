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
Texture2D<float> heightMap : register(t1, space1);

float4 main(VS_INPUT input, uint vertexId : SV_VertexID) : SV_POSITION
{
    uint width, height;
    heightMap.GetDimensions(width, height);

    const int2 index = int2(vertexId % width, vertexId / width);

    float3 position = input.Position;
    position.y = heightMap.Load(int3(index, 0)) * ubo.HeightfieldData.x;

    const float4 worldPosition = mul(ubo.WorldMatrix, float4(position, 1.0f));
    return mul(commonData.ViewProjection, worldPosition);
}