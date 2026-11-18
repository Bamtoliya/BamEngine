#include "common.hlsli"

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : WORLD_POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent  : TANGENT;
    float4 Color    : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
};

// MaterialConstants (Space 3, b0 / RHI Slot 2)
struct MaterialConstants
{
    uint materialFlags;
    float roughness;
    float metallic;
    float cavity;
    float4 baseColor;     // 얕은 물 틴트 (Shallow Water Tint)
    float3 emissiveColor; // 깊은 물 색상 (Deep Water Color)
    float ao;             // Depth Fade 거리 스케일 (Water Depth Falloff)
};

ConstantBuffer<MaterialConstants> gMaterial : register(b0, space3);

// Pass Textures (Space 3)
// Slot 16 (t0, space3): SceneColorCopy (불투명 씬 컬러 복사 버퍼)
Texture2D g_SceneColor : register(t0, space3);
SamplerState g_PassSampler : register(s0, space3);

// Slot 20 (t4, space3): GBuffer_Position (불투명 물체의 월드 위치 xyz)
Texture2D g_ScenePosition : register(t4, space3);

// Material Texture: Water Ripple Normal Map (Space 1, t1)
Texture2D g_NormalMap : register(t1, space1);
SamplerState g_MaterialSampler : register(s0, space1);

float4 main(PS_INPUT input) : SV_TARGET
{
    // 1. 화면 해상도 조회 및 화면 UV 계산
    float width, height;
    g_SceneColor.GetDimensions(width, height);
    float2 screenUV = input.Position.xy / float2(width, height);

    // 2. 시간 기반 유기적 잔물결 계산 (3방향 비대칭 파동 합성)
    float time = commonData.Time;
    float waveSpeed = 0.5f;
    float waveScale = 2.0f;

    float2 uv1 = input.TexCoord * waveScale;
    float2 uv2 = input.TexCoord * (waveScale * 1.4f);

    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent - dot(input.Tangent, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);

    float3 waveNormalTangent;
    if ((gMaterial.materialFlags & 0x02u) != 0u) // FLAG_HAS_NORMAL_MAP
    {
        float2 scrollUV1 = uv1 + float2(0.035f, 0.02f) * (time * waveSpeed);
        float2 scrollUV2 = uv2 + float2(-0.025f, 0.03f) * (time * waveSpeed);
        float3 n1 = g_NormalMap.Sample(g_MaterialSampler, scrollUV1).xyz * 2.0f - 1.0f;
        float3 n2 = g_NormalMap.Sample(g_MaterialSampler, scrollUV2).xyz * 2.0f - 1.0f;
        waveNormalTangent = normalize(float3(n1.xy + n2.xy, n1.z * n2.z));
    }
    else
    {
        // 3개의 비대칭 각도(0도, 55도, 115도) 파동 벡터 합성으로 바둑판 격자 아티팩트 제거
        float2 dir1 = float2(1.0f, 0.0f);
        float2 dir2 = float2(-0.57f, 0.82f);
        float2 dir3 = float2(-0.42f, -0.91f);

        float t = time * waveSpeed * 1.2f;
        float p1 = dot(uv1, dir1) * 6.0f + t * 1.0f;
        float p2 = dot(uv1, dir2) * 8.5f - t * 1.25f;
        float p3 = dot(uv2, dir3) * 11.0f + t * 0.85f;

        float dHdx = cos(p1) * dir1.x * 0.05f + cos(p2) * dir2.x * 0.04f + cos(p3) * dir3.x * 0.035f;
        float dHdy = cos(p1) * dir1.y * 0.05f + cos(p2) * dir2.y * 0.04f + cos(p3) * dir3.y * 0.035f;
        waveNormalTangent = normalize(float3(-dHdx, -dHdy, 1.0f));
    }

    float3 worldNormal = normalize(mul(waveNormalTangent, TBN));

    // 시선 벡터 (카메라 -> 수면)
    float3 V = normalize(commonData.CameraPos - input.WorldPos);
    if (dot(worldNormal, V) < 0.0f)
        worldNormal = -worldNormal;

    // 3. Depth Fade: 수면과 뒤쪽 불투명 지형/물체 간의 수심(Optical Depth) 계산
    float4 bgPosSample = g_ScenePosition.Sample(g_PassSampler, screenUV);
    bool hasBackground = (dot(bgPosSample.xyz, bgPosSample.xyz) > 0.001f);

    float waterDist = length(input.WorldPos - commonData.CameraPos);
    float bgDist = hasBackground ? length(bgPosSample.xyz - commonData.CameraPos) : (waterDist + 50.0f);

    // 수심 계산 및 깊이 역전(바닥 관통 시) 안전장치:
    // 만약 불투명 물체가 수면보다 앞에 있는 경우(bgDist < waterDist)는 물체에 가려져야 하는 비정상 영역
    bool isOccludedByForeground = hasBackground && (bgDist < waterDist - 0.02f);
    float waterDepth = isOccludedByForeground ? 0.0f : max(0.0f, bgDist - waterDist);

    // 수심 페이드 거리 (기본 약 3.0m, ao 파라미터로 조절 가능)
    float fadeDistance = (gMaterial.ao > 0.01f) ? (gMaterial.ao * 3.5f) : 3.0f;
    float depthFactor = saturate(waterDepth / fadeDistance);

    // 4. 뷰 공간 법선 기반 굴절 오프셋
    float3 viewNormal = mul((float3x3)commonData.View, worldNormal);
    float refractionStrength = 0.018f * (1.0f - depthFactor * 0.4f);
    float2 uvOffset = float2(viewNormal.x, -viewNormal.y) * refractionStrength;
    float2 refractedUV = clamp(screenUV + uvOffset, float2(0.001f, 0.001f), float2(0.999f, 0.999f));

    // 물 앞쪽 물체의 고스트 아티팩트 방지: 굴절 샘플이 수면보다 앞쪽이면 왜곡 취소
    float4 refrBgPos = g_ScenePosition.Sample(g_PassSampler, refractedUV);
    if (dot(refrBgPos.xyz, refrBgPos.xyz) > 0.001f)
    {
        float refrDist = length(refrBgPos.xyz - commonData.CameraPos);
        if (refrDist < waterDist - 0.02f)
            refractedUV = screenUV;
    }

    // 5. 굴절된 배경 씬 컬러 샘플링
    float3 sceneColor = g_SceneColor.Sample(g_PassSampler, refractedUV).rgb;

    // 만약 바닥 평면 아래로 파묻힌 관통 영역(bgDist < waterDist)이라면:
    // 하얗게 칠하지 않고 배경 씬 컬러를 그대로 통과시켜 하얗게 새는 현상을 원천 방어
    if (isOccludedByForeground)
    {
        return float4(sceneColor, 1.0f);
    }

    // 6. 맑은 투과 및 수심별 색상 그라데이션 (Beer-Lambert 흡수 모델)
    float3 shallowTint = gMaterial.baseColor.rgb;
    if (dot(shallowTint, shallowTint) < 0.001f)
        shallowTint = float3(0.85f, 0.96f, 0.98f); // 얕은 곳에서는 배경 원색(캐릭터 옷 등)을 90% 이상 맑게 보존

    float3 deepColor = gMaterial.emissiveColor;
    if (dot(deepColor, deepColor) < 0.001f)
        deepColor = float3(0.02f, 0.12f, 0.32f);   // 깊은 곳으로 갈수록 짙어지는 바다빛 네이비

    // 얕은 물에서는 씬 컬러의 디테일과 고유 색상을 온전히 유지하고, 깊어질수록 deepColor로 흡수
    float3 waterBodyColor = lerp(sceneColor * shallowTint, deepColor, depthFactor);

    // 7. 해안선 접촉 거품 (상향 수면 윗면에서만 얇게 발생)
    float isTopSurface = saturate((N.y - 0.4f) * 2.5f);
    float foamDist = 0.04f; // 4cm 초미세 림 거품
    if (isTopSurface > 0.05f && waterDepth < foamDist && waterDepth > 0.001f)
    {
        float foamT = saturate(1.0f - waterDepth / foamDist);
        float foamNoise = saturate(sin(uv1.x * 20.0f + uv1.y * 20.0f + time * 2.0f) * 0.5f + 0.5f);
        float3 foamColor = float3(0.92f, 0.96f, 1.0f);
        waterBodyColor = lerp(waterBodyColor, foamColor, foamT * foamNoise * 0.4f * isTopSurface);
    }

    // 8. Schlick Fresnel 반사율 (물 IOR 1.333 -> F0 = 0.02)
    float NdotV = saturate(dot(worldNormal, V));
    float F0 = 0.02f;
    float fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

    // 9. 하늘 반사광 및 태양광 스페큘러 (차분한 조도 밸런스)
    float3 skyReflection = lerp(float3(0.35f, 0.48f, 0.65f), float3(0.65f, 0.78f, 0.92f), saturate(worldNormal.y));

    float3 sunDir = normalize(float3(0.4f, 0.85f, 0.35f));
    float3 H = normalize(sunDir + V);
    float NdotH = saturate(dot(worldNormal, H));
    float sunSpecular = pow(NdotH, 256.0f) * 0.6f * isTopSurface;
    float3 sunHighlight = float3(1.0f, 0.96f, 0.88f) * sunSpecular;

    // 10. 최종 색상 합성 (투과된 씬 컬러 90% 보존 + 은은한 프레넬 반사)
    float3 finalColor = lerp(waterBodyColor, skyReflection, fresnel * 0.35f) + sunHighlight;

    return float4(finalColor, 1.0f);
}
