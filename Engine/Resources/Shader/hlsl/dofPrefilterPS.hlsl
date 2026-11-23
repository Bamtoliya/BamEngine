#include "depthOfField.hlsli"

// 절반 해상도 색(rgb)과 CoC(a). 출력 1픽셀이 전체 해상도 2×2를 덮는다.
Texture2D gScene : register(t0, space3);
Texture2D gPosition : register(t1, space3);
Texture2D gDiffuse : register(t2, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    const int2 basePixel = int2(input.Position.xy) * 2;
    const int2 maxPixel = int2(gDof.SourceSize) - 1;

    float3 color = float3(0.0f, 0.0f, 0.0f);
    // 가장 앞쪽(작은) CoC를 고른다. 앞 물체 경계를 넓게 잡고, 선명한 물체가 뒤쪽 흐림에 섞이지 않게 한다.
    float coc = gDof.MaxRadius;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        const int3 pixel = int3(min(basePixel + int2(i & 1, i >> 1), maxPixel), 0);
        color += gScene.Load(pixel).rgb;
        const float z = DofViewDepth(gPosition.Load(pixel).xyz, gDiffuse.Load(pixel).a);
        coc = min(coc, DofComputeCoC(z));
    }
    return float4(color * 0.25f, coc);
}
