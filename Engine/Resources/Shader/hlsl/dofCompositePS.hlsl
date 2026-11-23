#include "depthOfField.hlsli"

// 전체 해상도 CoC로 원래 화면, 뒤쪽 흐림, 앞쪽 흐림을 차례로 섞는다.
Texture2D gScene : register(t0, space3);
Texture2D gPosition : register(t1, space3);
Texture2D gDiffuse : register(t2, space3);
Texture2D gFar : register(t3, space3);
Texture2D gNear : register(t4, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    const int3 pixel = int3(int2(input.Position.xy), 0);
    const float2 uv = (float2(pixel.xy) + 0.5f) * gDof.InvSourceSize;
    const float4 scene = gScene.Load(pixel);
    const float coc = DofComputeCoC(DofViewDepth(gPosition.Load(pixel).xyz, gDiffuse.Load(pixel).a));

    // 반지름 0.5픽셀까지는 원래 화면, 2픽셀부터는 절반 해상도 흐림만 쓴다.
    float3 color = scene.rgb;
    color = lerp(color, gFar.SampleLevel(gLinearClamp, uv, 0.0f).rgb, smoothstep(0.5f, 2.0f, coc));

    // 앞쪽은 흐린 앞 물체가 덮는 정도와 이 픽셀 자신의 앞쪽 CoC 중 큰 쪽으로 섞는다.
    const float4 nearColor = gNear.SampleLevel(gLinearClamp, uv, 0.0f);
    color = lerp(color, nearColor.rgb, max(nearColor.a, smoothstep(0.5f, 2.0f, -coc)));
    return float4(color, scene.a);
}
