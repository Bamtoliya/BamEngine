#include "common.hlsli"

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : WORLD_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float4 Color : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
};

Texture2D texSampler : register(t0, space1);
SamplerState textureSampler : register(s0, space1);

Texture2D gSceneNormal : register(t1, space3);
Texture2D gScenePosition : register(t4, space3);

float4 main(PS_INPUT input) : SV_TARGET0
{
    float4 texColor = texSampler.Sample(textureSampler, input.TexCoord) * input.Color;

    // 1. 3D 메쉬 프레넬(Fresnel) & 림 발광(Rim Glow) 부스팅
    float3 V = normalize(commonData.CameraPos - input.WorldPos);
    float3 N = normalize(input.Normal);
    float NdotV = saturate(abs(dot(N, V)));
    
    // 시선 외곽 림(Rim) 발광 부스팅
    float fresnelRim = pow(1.0f - NdotV, 2.0f);
    texColor.rgb += texColor.rgb * (fresnelRim * 2.0f);
    // [개선]: 정면 알파를 0.55로 깎지 않고 원본 알파를 보존하여 허공에서도 흙갈색으로 가라앉지 않도록 방어
    float facingAlpha = lerp(0.90f, 1.0f, fresnelRim);
    texColor.a *= facingAlpha;

    // 2. 쿼드 파티클의 UV 가장자리 단색 텍스처 하드 에지 방지
    float2 uvEdgeDist = min(input.TexCoord, 1.0f - input.TexCoord);
    float uvEdgeFade = smoothstep(0.0f, 0.04f, min(uvEdgeDist.x, uvEdgeDist.y));
    texColor.a *= uvEdgeFade;

    if (texColor.a < 0.005f)
        discard;

    // 3. 정밀 Soft Particle (Depth Fade): 접촉 교차선(15cm) 국소 페더링
    int3 pixelCoord = int3(int2(input.Position.xy), 0);
    float4 sceneNormalSample = gSceneNormal.Load(pixelCoord);
    if (dot(sceneNormalSample.rgb, sceneNormalSample.rgb) > 0.01f)
    {
        float3 sceneWorldPos = gScenePosition.Load(pixelCoord).xyz;
        float sceneViewZ = mul(commonData.View, float4(sceneWorldPos, 1.0f)).z;
        float particleViewZ = mul(commonData.View, float4(input.WorldPos, 1.0f)).z;
        float depthDiff = sceneViewZ - particleViewZ;

        // 접촉면 15cm(0.15m) 근처의 교차 띠만 부드럽게 페이드아웃
        const float softFadeDistance = 0.15f;
        float depthFade = smoothstep(0.0f, softFadeDistance, depthDiff);

        // 지면 데칼 등 표면과 나란한(Coplanar) 쿼드는 바닥면에 의해 소실되지 않도록 보호
        float3 sceneWorldNormal = normalize(sceneNormalSample.rgb * 2.0f - 1.0f);
        float coplanarFactor = smoothstep(0.92f, 0.98f, abs(dot(N, sceneWorldNormal)));
        depthFade = lerp(depthFade, 1.0f, coplanarFactor);

        texColor *= depthFade;
        if (texColor.a < 0.005f)
            discard;
    }

    return texColor;
}