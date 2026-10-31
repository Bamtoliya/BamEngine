#include "common.hlsli"
#include "light.hlsli"

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256

Texture2D gPosition : register(t4, space3);
ByteAddressBuffer gLightBuffer : register(t6, space3);

RWStructuredBuffer<uint2> g_TileLightGrid : register(u0, space3); // x: startIndex, y: lightCount
RWStructuredBuffer<uint> g_TileLightIndexList : register(u1, space3); // 선별된 광원 인덱스 배열

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
groupshared float4 s_FrustumPlanes[4]; // Left, Right, Top, Bottom (원점을 지나므로 xyz만 사용)

// NDC 좌표를 View-space 방향 벡터로 역투영 (Z=1 평면)
float3 UnprojectNDC(float2 ndc)
{
    float4 clip = float4(ndc, 1.0f, 1.0f);
    float4 view = mul(commonData.InvProjection, clip);
    return view.xyz / view.w;
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    uint width, height;
    gPosition.GetDimensions(width, height);

    // 1. 타일 공유 변수 및 절두체 평면 초기화 (스레드 0번 수행)
    if (groupIndex == 0)
    {
        s_MinZ = 0x7F7FFFFF; // float 최대치 (비트 표현)
        s_MaxZ = 0;
        s_TileLightCount = 0;

        // 타일의 픽셀 경계 계산
        float pxMinX = float(groupID.x * TILE_SIZE);
        float pxMaxX = min(float((groupID.x + 1) * TILE_SIZE), float(width));
        float pxMinY = float(groupID.y * TILE_SIZE);
        float pxMaxY = min(float((groupID.y + 1) * TILE_SIZE), float(height));

        // 뷰포트 좌표를 NDC [-1, 1] 범위로 변환 (DirectX: Y축 상단이 +1, 하단이 -1)
        float2 ndcMin = float2((pxMinX / float(width)) * 2.0f - 1.0f, 1.0f - (pxMaxY / float(height)) * 2.0f);
        float2 ndcMax = float2((pxMaxX / float(width)) * 2.0f - 1.0f, 1.0f - (pxMinY / float(height)) * 2.0f);

        // 타일의 4개 코너 뷰 공간 방향 벡터 (Left-Handed: Z+ 전방)
        float3 v00 = UnprojectNDC(float2(ndcMin.x, ndcMax.y)); // 좌상단
        float3 v10 = UnprojectNDC(float2(ndcMax.x, ndcMax.y)); // 우상단
        float3 v01 = UnprojectNDC(float2(ndcMin.x, ndcMin.y)); // 좌하단
        float3 v11 = UnprojectNDC(float2(ndcMax.x, ndcMin.y)); // 우하단

        // 타일 내부를 향하는 4개 평면 법선 계산 (원점 (0,0,0)을 지남)
        s_FrustumPlanes[0] = float4(normalize(cross(v00, v01)), 0.0f); // Left Plane
        s_FrustumPlanes[1] = float4(normalize(cross(v11, v10)), 0.0f); // Right Plane
        s_FrustumPlanes[2] = float4(normalize(cross(v10, v00)), 0.0f); // Top Plane
        s_FrustumPlanes[3] = float4(normalize(cross(v01, v11)), 0.0f); // Bottom Plane
    }
    GroupMemoryBarrierWithGroupSync();

    // 2. 픽셀별 뷰 공간 깊이 추출 및 타일 minZ/maxZ 계산
    int2 pixelCoord = int2(groupID.xy * TILE_SIZE + groupThreadID.xy);
    if (pixelCoord.x < (int) width && pixelCoord.y < (int) height)
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
    bool hasGeometry = (minTileZ <= maxTileZ);

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

        // 지오메트리가 없는 타일은 국소 광원을 수집하지 않음
        if (!hasGeometry)
            continue;

        // Point Light 및 Spot Light: 뷰 공간 절두체 평면 및 거리 검사
        float3 lightViewPos = mul(commonData.View, float4(light.Position, 1.0f)).xyz;
        float lightRange = max(light.Range, 0.1f);

        // Z축 범위 검사
        if ((lightViewPos.z + lightRange < minTileZ) || (lightViewPos.z - lightRange > maxTileZ))
            continue;

        // 좌/우/상/하 4개 절두체 평면 검사
        bool inFrustum = true;
        [unroll]
        for (int p = 0; p < 4; ++p)
        {
            if (dot(s_FrustumPlanes[p].xyz, lightViewPos) < -lightRange)
            {
                inFrustum = false;
                break;
            }
        }

        if (inFrustum)
        {
            uint slot;
            InterlockedAdd(s_TileLightCount, 1, slot);
            if (slot < MAX_LIGHTS_PER_TILE)
                s_TileLightIndices[slot] = lightIndex;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // 4. 전역 버퍼에 타일 결과 기록 (스레드 0번이 일괄 기록)
    if (groupIndex == 0)
    {
        uint tilesX = (width + TILE_SIZE - 1) / TILE_SIZE;
        uint tileIndex = groupID.y * tilesX + groupID.x;
        uint count = min(s_TileLightCount, (uint) MAX_LIGHTS_PER_TILE);
        uint startIndex = tileIndex * MAX_LIGHTS_PER_TILE;

        g_TileLightGrid[tileIndex] = uint2(startIndex, count);

        for (uint i = 0; i < count; ++i)
        {
            g_TileLightIndexList[startIndex + i] = s_TileLightIndices[i];
        }
    }
}