#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float4 Color : COLOR;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : WORLD_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float4 Color : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
};

struct ObjectData
{
    float4x4 WorldMatrix;
    float4 PickingData;
    float4 HeightfieldData;
};

ConstantBuffer<ObjectData> ubo : register(b0, space2);
Texture2D<float> heightMap : register(t1, space1);

VS_OUTPUT main(VS_INPUT input, uint vertexId : SV_VertexID)
{
    VS_OUTPUT output;

    uint width, height;
    heightMap.GetDimensions(width, height);

    const int2 index = int2(vertexId % width, vertexId / width);
    const int2 lastIndex = int2(width, height) - 1;
    const float maxHeight = ubo.HeightfieldData.x;
    const float2 cellSpacing = ubo.HeightfieldData.yz;

	// 그리드의 정점 번호는 CPU 높이 배열의 인덱스와 같습니다.
    float3 displacedPosition = input.Position;
    displacedPosition.y = heightMap.Load(int3(index, 0)) * maxHeight;

    const float4 worldPosition = mul(ubo.WorldMatrix, float4(displacedPosition, 1.0f));
    output.Position = mul(commonData.ViewProjection, worldPosition);
    output.WorldPos = worldPosition.xyz;

    const int2 left = int2(max(index.x - 1, 0), index.y);
    const int2 right = int2(min(index.x + 1, lastIndex.x), index.y);
    const int2 down = int2(index.x, max(index.y - 1, 0));
    const int2 up = int2(index.x, min(index.y + 1, lastIndex.y));

    const float hL = heightMap.Load(int3(left, 0)) * maxHeight;
    const float hR = heightMap.Load(int3(right, 0)) * maxHeight;
    const float hD = heightMap.Load(int3(down, 0)) * maxHeight;
    const float hU = heightMap.Load(int3(up, 0)) * maxHeight;

    const float distanceX = float(right.x - left.x) * cellSpacing.x;
    const float distanceZ = float(up.y - down.y) * cellSpacing.y;
    const float slopeX = (hR - hL) / distanceX;
    const float slopeZ = (hU - hD) / distanceZ;

    const float3 localTangent = float3(1.0f, slopeX, 0.0f);
    const float3 localBitangent = float3(0.0f, slopeZ, 1.0f);
    const float3 worldTangent = mul(ubo.WorldMatrix, float4(localTangent, 0.0f)).xyz;
    const float3 worldBitangent = mul(ubo.WorldMatrix, float4(localBitangent, 0.0f)).xyz;

    output.Normal = normalize(cross(worldBitangent, worldTangent));
    output.Tangent = normalize(worldTangent);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    output.PickingId = ubo.PickingData.x;
    return output;
}