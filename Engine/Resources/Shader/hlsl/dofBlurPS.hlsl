#include "depthOfField.hlsli"

// 절반 해상도 원판 흐림. NearPass가 0이면 뒤쪽, 1이면 앞쪽만 모은다.
// 앞쪽 결과의 a는 흐린 앞 물체가 이 픽셀을 덮는 정도다.
Texture2D gHalf : register(t0, space3);
Texture2D gDilatedTile : register(t1, space3);

static const uint TAP_COUNT = 48u;
static const float GOLDEN_ANGLE = 2.39996323f;

float FieldCoC(float coc)
{
    return gDof.NearPass != 0u ? max(-coc, 0.0f) : max(coc, 0.0f);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    const int2 pixel = int2(input.Position.xy);
    const float2 uv = (float2(pixel) + 0.5f) * gDof.InvHalfSize;
    const float4 center = gHalf.Load(int3(pixel, 0));

    // 앞쪽은 이웃까지 넓힌 타일 값, 뒤쪽은 자기 CoC를 반지름으로 쓴다. 절반 해상도 픽셀로 바꾼다.
    const float radius = (gDof.NearPass != 0u ? gDilatedTile.Load(int3(pixel / 4, 0)).r : FieldCoC(center.a)) * 0.5f;
    if (radius < 0.5f)
        return float4(center.rgb, gDof.NearPass != 0u ? 0.0f : 1.0f);

    float3 sum = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;
    for (uint i = 0u; i < TAP_COUNT; ++i)
    {
        // 원판 위에 고르게 퍼지는 황금각 나선
        const float tapDistance = sqrt((float(i) + 0.5f) / float(TAP_COUNT)) * radius;
        const float angle = float(i) * GOLDEN_ANGLE;
        const float2 offset = float2(cos(angle), sin(angle)) * tapDistance;
        const float4 tap = gHalf.SampleLevel(gLinearClamp, uv + offset * gDof.InvHalfSize, 0.0f);

        // 표본의 흐림 반지름이 이 픽셀까지 닿을 때만 섞는다. 선명한 표본은 흐린 영역으로 번지지 않는다.
        const float weight = saturate(FieldCoC(tap.a) * 0.5f - tapDistance + 1.0f);
        sum += tap.rgb * weight;
        weightSum += weight;
    }

    const float3 color = weightSum > 0.0001f ? sum / weightSum : center.rgb;
    if (gDof.NearPass != 0u)
        return float4(color, saturate(weightSum / float(TAP_COUNT)));
    return float4(color, 1.0f);
}
