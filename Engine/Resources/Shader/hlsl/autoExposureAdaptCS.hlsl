#include "autoExposure.hlsli"

// 1x1 R32_FLOAT. 이전 프레임 노출(EV)을 읽고 이번 프레임 노출을 쓴다. 뷰마다 하나다.
RWTexture2D<float> gExposure : register(u2, space3);

groupshared uint s_Bins[AUTO_EXPOSURE_BIN_COUNT];

[numthreads(AUTO_EXPOSURE_BIN_COUNT, 1, 1)]
void main(uint index : SV_GroupIndex)
{
    s_Bins[index] = gHistogram[index];
    gHistogram[index] = 0; // 다음 프레임 측정을 위해 비운다.
    GroupMemoryBarrierWithGroupSync();

    if (index != 0 || gParams.Mode == AUTO_EXPOSURE_MODE_CLEAR)
        return;

    float total = 0.0f;
    for (uint i = 0; i < AUTO_EXPOSURE_BIN_COUNT; ++i)
        total += (float)s_Bins[i];

    // 가장 어두운 비율과 가장 밝은 비율을 뺀 화소들의 평균 log2 밝기
    float low = total * gParams.LowPercent;
    float high = total * gParams.HighPercent;
    float seen = 0.0f;
    float weight = 0.0f;
    float sum = 0.0f;
    for (uint bin = 0; bin < AUTO_EXPOSURE_BIN_COUNT; ++bin)
    {
        float begin = seen;
        seen += (float)s_Bins[bin];
        float used = max(0.0f, min(seen, high) - max(begin, low));
        weight += used;
        sum += used * BinToLogLuminance(bin);
    }
    float averageLogLuminance = weight > 0.0f ? sum / weight : log2(gParams.MiddleGray);

    float target = clamp(log2(gParams.MiddleGray) - averageLogLuminance, min(gParams.MinEV, gParams.MaxEV), max(gParams.MinEV, gParams.MaxEV))
        + gParams.CompensationEV;

    float previous = gExposure[uint2(0, 0)];
    float exposure = target;
    if (gParams.Mode == AUTO_EXPOSURE_MODE_ADAPT && !isnan(previous) && !isinf(previous))
    {
        // 장면이 밝아져 노출을 낮출 때와 어두워져 올릴 때 속도가 다르다.
        float speed = target < previous ? gParams.SpeedToLight : gParams.SpeedToDark;
        exposure = (gParams.DeltaTime > 0.0f && speed > 0.0f)
            ? previous + (target - previous) * (1.0f - exp(-gParams.DeltaTime * speed))
            : previous;
    }
    gExposure[uint2(0, 0)] = exposure;
}
