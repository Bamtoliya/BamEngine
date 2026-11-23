#ifndef AUTO_EXPOSURE_HLSLI
#define AUTO_EXPOSURE_HLSLI

// Engine/Source/Render/PostProcess/Public/AutoExposure.h와 같은 식을 쓴다. 한쪽을 바꾸면 다른 쪽도 고친다.
#define AUTO_EXPOSURE_BIN_COUNT 64

#define AUTO_EXPOSURE_MODE_CLEAR 0 // 히스토그램만 비운다.
#define AUTO_EXPOSURE_MODE_RESET 1 // 이전 노출 없이 목표 노출을 바로 쓴다.
#define AUTO_EXPOSURE_MODE_ADAPT 2 // 이전 노출에서 목표로 옮긴다.

// AutoExposureEffect.cpp의 AutoExposureParams와 같은 배치 (b0, space3)
struct AutoExposureParams
{
    uint2 SourceSize;
    float MinLogLuminance;
    float LogLuminanceRange;

    float LowPercent;
    float HighPercent;
    float MiddleGray;
    float MinEV;

    float MaxEV;
    float CompensationEV;
    float DeltaTime;
    float SpeedToLight;

    float SpeedToDark;
    uint Mode;
    float2 _pad;
};

ConstantBuffer<AutoExposureParams> gParams : register(b0, space3);
// 구간별 화소 수. 적응 패스가 읽은 뒤 0으로 비운다.
RWStructuredBuffer<uint> gHistogram : register(u0, space3);

float ExposureLuminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

uint LuminanceToBin(float luminance)
{
    // 0·음수·NaN은 가장 어두운 구간으로 보낸다.
    if (!(luminance > 0.0f))
        return 0;
    float t = saturate((log2(luminance) - gParams.MinLogLuminance) / gParams.LogLuminanceRange);
    return min((uint)(t * AUTO_EXPOSURE_BIN_COUNT), AUTO_EXPOSURE_BIN_COUNT - 1);
}

float BinToLogLuminance(uint bin)
{
    return gParams.MinLogLuminance + ((float)bin + 0.5f) / AUTO_EXPOSURE_BIN_COUNT * gParams.LogLuminanceRange;
}

#endif
