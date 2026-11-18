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
    float4 baseColor;
    float3 emissiveColor;
    float ao;
};

ConstantBuffer<MaterialConstants> gMaterial : register(b0, space3);

// Pass Texture: SceneColorCopy (Space 3, t0 / RHI Slot 16)
Texture2D g_SceneColor : register(t0, space3);
SamplerState g_SceneColorSampler : register(s0, space3);

// Material Texture: Normal Map (Space 1, t1)
Texture2D g_NormalMap : register(t1, space1);
SamplerState g_MaterialSampler : register(s0, space1);

// Pass Texture: IBL Prefiltered Environment Cube (Space 3, t11 / RHI Slot 27)
TextureCube g_PrefilteredEnvMap : register(t11, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    // 1. 화면 해상도 조회 및 원본 스크린 UV 계산
    float width, height;
    g_SceneColor.GetDimensions(width, height);
    float2 screenUV = input.Position.xy / float2(width, height);

    // 2. 표면 법선 계산
    float3 N = normalize(input.Normal);
    if ((gMaterial.materialFlags & 0x02u) != 0u) // FLAG_HAS_NORMAL_MAP
    {
        float3 normalSample = g_NormalMap.Sample(g_MaterialSampler, input.TexCoord).xyz * 2.0f - 1.0f;
        float3 T = normalize(input.Tangent);
        float3 B = cross(N, T);
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(normalSample, TBN));
    }

    // 3. 시선 벡터 및 뷰 공간 법선 기반 굴절 오프셋
    float3 V = normalize(commonData.CameraPos - input.WorldPos);
    float3 viewNormal = mul((float3x3)commonData.View, N);

    // 굴절 왜곡 강도 계산 (거칠기에 따라 왜곡 범위 조절)
    float refractionStrength = 0.05f * (1.0f - saturate(gMaterial.roughness) * 0.5f);
    float2 refractedUV = screenUV + viewNormal.xy * refractionStrength;

    // 화면 밖 경계 벗어남 방지 (안전 폴백)
    if (refractedUV.x < 0.0f || refractedUV.x > 1.0f || refractedUV.y < 0.0f || refractedUV.y > 1.0f)
    {
        refractedUV = screenUV;
    }

    // 4. 굴절된 배경 HDR 씬 컬러 샘플링
    float3 sceneColor = g_SceneColor.Sample(g_SceneColorSampler, refractedUV).rgb;

    // 5. 유리 틴트 컬러 (baseColor.rgb) 적용
    float3 tintColor = gMaterial.baseColor.rgb;
    if (dot(tintColor, tintColor) < 0.001f)
        tintColor = float3(1.0f, 1.0f, 1.0f);
    float3 glassColor = sceneColor * tintColor;

    // 6. 프레넬 반사율 계산 (Schlick Fresnel, 유리 IOR ~1.52 -> F0 = 0.04)
    float NdotV = saturate(dot(N, V));
    float F0 = 0.04f;
    float fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

    // 7. 환경맵(IBL) 스페큘러 반사
    float3 R = reflect(-V, N);
    float3 envReflection = g_PrefilteredEnvMap.SampleLevel(g_SceneColorSampler, R, gMaterial.roughness * 4.0f).rgb;

    // 8. 최종 합성 (굴절된 배경 + 프레넬 환경 반사)
    float3 finalColor = lerp(glassColor, envReflection, fresnel);

    return float4(finalColor, 1.0f);
}
