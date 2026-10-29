#include "common.hlsli"
#include "light.hlsli"

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256

Texture2D gPosition : register(t4, space3);
ByteAddressBuffer gLightBuffer : register(t6, space3);

// 출력 UAV 버퍼
RWStructuredBuffer<uint2> g_TileLightGrid : register(u0, space3);      // x: startIndex, y: lightCount
RWStructuredBuffer<uint>  g_TileLightIndexList : register(u1, space3); // 선별된 광원 인덱스 배열

static const uint LIGHT_BUFFER_HEADER_SIZE = 16u;
static const uint LIGHT_BUFFER_STRIDE = 80u;

Light LoadLight(uint lightIndex)
{
    uint address = LIGHT_BUFFER_HEADER_SIZE + lightIndex * LIGHT_BUFFER_STRIDE;
    Light light;

    light.Position = asfloat(gLightBuffer.Load3(address));
    light.Intensity = asfloat(gLightBuffer.Load(address + 12u));
    light.Direction = asfloat(gLightBuffer.Load3(address + 16u));
    light.Range = asfloat(gLightBuffer.Load(address + 28u));
    light.Color = asfloat(gLightBuffer.Load3(address + 32u));
    light.PackedFlags = gLightBuffer.Load(address + 44u);
    light.AttenuationCoeff = asfloat(gLightBuffer.Load3(address + 48u));
    light.ShadowStrength = asfloat(gLightBuffer.Load(address + 60u));
    light.SpotInnerCos = asfloat(gLightBuffer.Load(address + 64u));
    light.SpotOuterCos = asfloat(gLightBuffer.Load(address + 68u));
    light.SpotFalloff = asfloat(gLightBuffer.Load(address + 72u));
    light.Pad1 = asfloat(gLightBuffer.Load(address + 76u));

    return light;
}

groupshared uint s_MinZ;
groupshared uint s_MaxZ;
groupshared uint s_TileLightCount;
groupshared uint s_TileLightIndices[MAX_LIGHTS_PER_TILE];

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    // 1. 타일 공유 변수 초기화
    if (groupIndex == 0)
    {
        s_MinZ = 0x7F7FFFFF; // float 최대치 (비트 표현)
        s_MaxZ = 0;
        s_TileLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // 2. 픽셀별 뷰 공간 깊이 추출 및 타일 minZ/maxZ 계산
    uint width, height;
    gPosition.GetDimensions(width, height);
    int2 pixelCoord = int2(groupID.xy * TILE_SIZE + groupThreadID.xy);

    if (pixelCoord.x < (int)width && pixelCoord.y < (int)height)
    {
        float3 worldPos = gPosition.Load(int3(pixelCoord, 0)).rgb;
        if (length(worldPos) > 0.0001f)
        {
            float viewZ = mul(commonData.View, float4(worldPos, 1.0f)).z;
            if (viewZ > 0.0f)
            {
                uint zInt = asuint(viewZ);
                InterlockedMin(s_MinZ, zInt);
                InterlockedMax(s_MaxZ, zInt);
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    float minTileZ = asfloat(s_MinZ);
    float maxTileZ = asfloat(s_MaxZ);

    // 3. 광원 순회 및 타일 절두체 구면 교차 검사
    uint numLights = gLightBuffer.Load(0u);

    for (uint lightIndex = groupIndex; lightIndex < numLights; lightIndex += (TILE_SIZE * TILE_SIZE))
    {
        Light light = LoadLight(lightIndex);
        if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_USE_IN_DEFERRED))
            continue;

        uint lightType = GetLightType(light.PackedFlags);

        // Directional Light 및 Sky Light는 전역 광원이므로 항상 타일에 포함
        if (lightType == LIGHT_TYPE_DIRECTIONAL || lightType == LIGHT_TYPE_SKY)
        {
            uint slot;
            InterlockedAdd(s_TileLightCount, 1, slot);
            if (slot < MAX_LIGHTS_PER_TILE)
                s_TileLightIndices[slot] = lightIndex;
            continue;
        }

        // Point Light 및 Spot Light: 뷰 공간 거리 검사
        float3 lightViewPos = mul(commonData.View, float4(light.Position, 1.0f)).xyz;
        float lightRange = max(light.Range, 0.1f);

        // Z축 범위 검사 (minTileZ ~ maxTileZ)
        if ((lightViewPos.z + lightRange >= minTileZ) && (lightViewPos.z - lightRange <= maxTileZ))
        {
            uint slot;
            InterlockedAdd(s_TileLightCount, 1, slot);
            if (slot < MAX_LIGHTS_PER_TILE)
            {
                s_TileLightIndices[slot] = lightIndex;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // 4. 전역 버퍼에 타일 결과 기록 (스레드 0번이 일괄 기록)
    if (groupIndex == 0)
    {
        uint tilesX = (width + TILE_SIZE - 1) / TILE_SIZE;
        uint tileIndex = groupID.y * tilesX + groupID.x;
        uint count = min(s_TileLightCount, (uint)MAX_LIGHTS_PER_TILE);
        uint startIndex = tileIndex * MAX_LIGHTS_PER_TILE;

        g_TileLightGrid[tileIndex] = uint2(startIndex, count);

        for (uint i = 0; i < count; ++i)
        {
            g_TileLightIndexList[startIndex + i] = s_TileLightIndices[i];
        }
    }
}