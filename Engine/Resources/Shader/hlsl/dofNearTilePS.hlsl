#include "depthOfField.hlsli"

// 타일 1픽셀(전체 해상도 8×8, 절반 해상도 4×4)마다 앞쪽 CoC의 최댓값
Texture2D gHalf : register(t0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    const int2 basePixel = int2(input.Position.xy) * 4;
    const int2 maxPixel = int2(gDof.HalfSize) - 1;

    float nearCoC = 0.0f;
    [unroll]
    for (int y = 0; y < 4; ++y)
    {
        [unroll]
        for (int x = 0; x < 4; ++x)
            nearCoC = max(nearCoC, -gHalf.Load(int3(min(basePixel + int2(x, y), maxPixel), 0)).a);
    }
    return float4(nearCoC, 0.0f, 0.0f, 0.0f);
}
