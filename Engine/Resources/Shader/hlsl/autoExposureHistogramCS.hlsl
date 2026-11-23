#include "autoExposure.hlsli"

// 톤매핑 전 HDR 장면 색
Texture2D gSource : register(t0, space3);

groupshared uint s_Bins[AUTO_EXPOSURE_BIN_COUNT];

[numthreads(16, 16, 1)]
void main(uint3 pixel : SV_DispatchThreadID, uint index : SV_GroupIndex)
{
    if (index < AUTO_EXPOSURE_BIN_COUNT)
        s_Bins[index] = 0;
    GroupMemoryBarrierWithGroupSync();

    // 그룹 안에서 먼저 세고 전역 버퍼에는 구간마다 한 번만 더해 원자 연산 충돌을 줄인다.
    if (all(pixel.xy < gParams.SourceSize))
    {
        float3 color = gSource.Load(int3(pixel.xy, 0)).rgb;
        InterlockedAdd(s_Bins[LuminanceToBin(ExposureLuminance(color))], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    if (index < AUTO_EXPOSURE_BIN_COUNT && s_Bins[index] != 0)
        InterlockedAdd(gHistogram[index], s_Bins[index]);
}
