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

        // 2. 표면 법선 및 마이크로 잔물결(Micro Ripple) 합성
    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent - dot(input.Tangent, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);

    float time = commonData.Time;
    float microWaveSpeed = 0.8f;

    // [대처 핵심] 로컬 input.TexCoord 대신 월드 수평 좌표(WorldPos.xz) 사용
    // 엔티티 Scale을 키워도 1미터당 잔물결 크기가 고정되어 자연스럽게 타일링 반복됨
    float2 worldUV = input.WorldPos.xz;
    float2 microUV = worldUV * 1.5f; // 1미터당 약 1.5회 잔물결 반복

    float3 microNormalTangent;
    if ((gMaterial.materialFlags & 0x02u) != 0u) // FLAG_HAS_NORMAL_MAP
    {
        float2 scrollUV1 = microUV + float2(0.04f, 0.03f) * (time * microWaveSpeed);
        float2 scrollUV2 = microUV * 1.5f + float2(-0.03f, 0.045f) * (time * microWaveSpeed);
        float3 n1 = g_NormalMap.Sample(g_MaterialSampler, scrollUV1).xyz * 2.0f - 1.0f;
        float3 n2 = g_NormalMap.Sample(g_MaterialSampler, scrollUV2).xyz * 2.0f - 1.0f;
        microNormalTangent = normalize(float3(n1.xy + n2.xy, n1.z * n2.z));
    }
    else
    {
        float t = time * microWaveSpeed * 1.5f;
        float p1 = dot(microUV, float2(1.0f, 0.2f)) * 8.0f + t;
        float p2 = dot(microUV, float2(-0.4f, 0.9f)) * 11.0f - t * 1.2f;
        float dHdx = cos(p1) * 0.04f - sin(p2) * 0.035f;
        float dHdy = cos(p1) * 0.015f + cos(p2) * 0.04f;
        microNormalTangent = normalize(float3(-dHdx, -dHdy, 1.0f));
    }

    float3 worldNormal = normalize(mul(microNormalTangent, TBN));

    // 시선 벡터 (카메라 -> 수면)
    float3 V = normalize(commonData.CameraPos - input.WorldPos);
    if (dot(worldNormal, V) < 0.0f)
        worldNormal = -worldNormal;

    // 3. Depth Fade: 파도 표면과 뒤쪽 불투명 지형/물체 간의 수심(Optical Depth) 계산
    float4 bgPosSample = g_ScenePosition.Sample(g_PassSampler, screenUV);
    bool hasBackground = (dot(bgPosSample.xyz, bgPosSample.xyz) > 0.001f);

    float waterDist = length(input.WorldPos - commonData.CameraPos);
    float bgDist = hasBackground ? length(bgPosSample.xyz - commonData.CameraPos) : (waterDist + 50.0f);

    // 깊이 역전(바닥 관통 시) 안전장치
    bool isOccludedByForeground = hasBackground && (bgDist < waterDist - 0.02f);
    float waterDepth = isOccludedByForeground ? 0.0f : max(0.0f, bgDist - waterDist);

    float fadeDistance = (gMaterial.ao > 0.01f) ? (gMaterial.ao * 3.5f) : 3.0f;
    float depthFactor = saturate(waterDepth / fadeDistance);

    // 4. 뷰 공간 법선 기반 굴절 오프셋
    float3 viewNormal = mul((float3x3)commonData.View, worldNormal);
    float refractionStrength = 0.02f * (1.0f - depthFactor * 0.35f);
    float2 uvOffset = float2(viewNormal.x, -viewNormal.y) * refractionStrength;
    float2 refractedUV = clamp(screenUV + uvOffset, float2(0.001f, 0.001f), float2(0.999f, 0.999f));

    // 물 앞쪽 물체의 고스트 아티팩트 방지
    float4 refrBgPos = g_ScenePosition.Sample(g_PassSampler, refractedUV);
    if (dot(refrBgPos.xyz, refrBgPos.xyz) > 0.001f)
    {
        float refrDist = length(refrBgPos.xyz - commonData.CameraPos);
        if (refrDist < waterDist - 0.02f)
            refractedUV = screenUV;
    }

    // 5. 굴절된 배경 씬 컬러 샘플링
    float3 sceneColor = g_SceneColor.Sample(g_PassSampler, refractedUV).rgb;

    if (isOccludedByForeground)
    {
        return float4(sceneColor, 1.0f);
    }

    // 6. 맑은 투과 및 수심별 색상 그라데이션 (Beer-Lambert 모델)
    float3 shallowTint = gMaterial.baseColor.rgb;
    if (dot(shallowTint, shallowTint) < 0.001f)
        shallowTint = float3(0.85f, 0.96f, 0.98f);

    float3 deepColor = gMaterial.emissiveColor;
    if (dot(deepColor, deepColor) < 0.001f)
        deepColor = float3(0.015f, 0.10f, 0.28f);

    float3 waterBodyColor = lerp(sceneColor * shallowTint, deepColor, depthFactor);

     // 7. 파도 마루(Wave Crest Whitecap) 거품 노이즈도 월드 좌표 기반으로 변경
    float crestFoam = saturate((1.0f - N.y) * 3.5f - 0.6f);
    float foamNoise = saturate(sin(worldUV.x * 4.0f + time * 3.0f) * sin(worldUV.y * 4.0f - time * 2.5f) * 0.5f + 0.5f);
    float totalFoam = crestFoam * foamNoise * 0.6f;

    // 해안선 초미세 림 거품 (4cm 이하 접촉선)
    float shorelineDist = 0.05f;
    if (waterDepth < shorelineDist && waterDepth > 0.001f)
    {
        float shorelineT = saturate(1.0f - waterDepth / shorelineDist);
        totalFoam = max(totalFoam, shorelineT * 0.45f);
    }

    if (totalFoam > 0.01f)
    {
        float3 foamColor = float3(0.95f, 0.98f, 1.0f);
        waterBodyColor = lerp(waterBodyColor, foamColor, totalFoam);
    }

    // 8. Schlick Fresnel 반사율 계산 (물 IOR 1.333 -> F0 = 0.02)
    float NdotV = saturate(dot(worldNormal, V));
    float F0 = 0.02f;
    float fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

    // 9. 하늘 반사광 및 태양광 스페큘러 (파도 굴곡을 따라 맺히는 Sun Sparkle)
    float3 skyReflection = lerp(float3(0.35f, 0.48f, 0.65f), float3(0.68f, 0.80f, 0.94f), saturate(worldNormal.y));

    float3 sunDir = normalize(float3(0.4f, 0.85f, 0.35f));
    float3 H = normalize(sunDir + V);
    float NdotH = saturate(dot(worldNormal, H));
    float sunSpecular = pow(NdotH, 256.0f) * 0.8f;
    float3 sunHighlight = float3(1.0f, 0.96f, 0.88f) * sunSpecular;

    // 10. 최종 색상 합성
    float3 finalColor = lerp(waterBodyColor, skyReflection, fresnel * 0.4f) + sunHighlight;

    return float4(finalColor, 1.0f);
}
