#include "common.hlsli"
#include "shading_flags.hlsli"

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

// Material Texture: Diffuse (Space 1, t0) - 알파 마스크로만 사용 (점액 흔적 등 블롭 모양 반투명 표현)
Texture2D g_DiffuseMap : register(t0, space1);

// Material Texture: Normal Map (Space 1, t1)
Texture2D g_NormalMap : register(t1, space1);
SamplerState g_MaterialSampler : register(s0, space1);

float4 main(PS_INPUT input) : SV_TARGET
{
    // 1. 화면 해상도 조회 및 화면 UV 계산
    float width, height;
    g_SceneColor.GetDimensions(width, height);
    float2 screenUV = input.Position.xy / float2(width, height);

    // 2. 표면 법선 계산 및 양면 렌더링 대응
    float3 N = normalize(input.Normal);
    if ((gMaterial.materialFlags & 0x02u) != 0u) // FLAG_HAS_NORMAL_MAP
    {
        float3 normalSample = g_NormalMap.Sample(g_MaterialSampler, input.TexCoord).xyz * 2.0f - 1.0f;
        float3 T = normalize(input.Tangent);
        float3 B = cross(N, T);
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(normalSample, TBN));
    }

    // 시선 벡터 (카메라 위치 - 픽셀 월드 위치)
    float3 V = normalize(commonData.CameraPos - input.WorldPos);

    // 양면(Two-sided) 메쉬 대응: 뒷면 법선을 카메라 시선 방향으로 정렬
    if (dot(N, V) < 0.0f)
    {
        N = -N;
    }

    // 3. 뷰 공간 법선 기반 굴절 오프셋
    // (화면 UV 공간은 +Y가 아래쪽이므로 View Y를 반전)
    float3 viewNormal = mul((float3x3)commonData.View, N);
    float refractionStrength = 0.04f * (1.0f - saturate(gMaterial.roughness) * 0.5f);
    float2 offset = float2(viewNormal.x, -viewNormal.y) * refractionStrength;
    float2 refractedUV = screenUV + offset;

    // 화면 밖 경계 벗어남 방지 (안전 클램핑)
    refractedUV = clamp(refractedUV, float2(0.001f, 0.001f), float2(0.999f, 0.999f));

    // 4. 굴절된 배경 HDR 씬 컬러 샘플링
    float3 sceneColor = g_SceneColor.Sample(g_SceneColorSampler, refractedUV).rgb;

    // 5. 유리 틴트 컬러 (baseColor.rgb) 적용
    float3 tintColor = gMaterial.baseColor.rgb;
    if (dot(tintColor, tintColor) < 0.001f)
        tintColor = float3(1.0f, 1.0f, 1.0f);
    float3 glassColor = sceneColor * tintColor;

    // 6. Schlick Fresnel 반사율 계산 (유리 IOR ~1.52 -> F0 = 0.04)
    float NdotV = saturate(dot(N, V));
    float F0 = 0.04f;
    float fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

    // 7. 은은한 가장자리 프레넬 스페큘러 하이라이트 합성
    // (미바인딩 IBL 텍스처로 인해 검게 타는 현상을 원천 방지하고 맑은 유리 반사감 부여)
    float3 rimHighlight = float3(1.0f, 1.0f, 1.0f) * 0.35f;
    float3 finalColor = glassColor + rimHighlight * fresnel;

    float outAlpha = (gMaterial.baseColor.a > 0.001f) ? saturate(gMaterial.baseColor.a) : 1.0f;
    if (gMaterial.materialFlags & FLAG_HAS_DIFFUSE_MAP)
        outAlpha *= g_DiffuseMap.Sample(g_MaterialSampler, input.TexCoord).a;
    return float4(finalColor, outAlpha);
}
