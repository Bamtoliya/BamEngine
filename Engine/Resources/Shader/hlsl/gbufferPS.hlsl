#include "shading_flags.hlsli"

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

struct PS_OUTPUT
{
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 PBR : SV_TARGET2;
	float4 Emission : SV_TARGET3;
	float4 Position : SV_TARGET4;
};

// ── 머티리얼 상수 버퍼 (Space 3, b0 / RHI Slot 2) ──
struct MaterialConstants
{
	uint   materialFlags;
	float  roughness;
	float  metallic;
	float  cavity;
	float4 baseColor;
	float3 emissiveColor;
	float  ao;
};

ConstantBuffer<MaterialConstants> gMaterial : register(b0, space3);

// ── 머티리얼 텍스처 슬롯 (Space 1) ──
Texture2D diffuseMap  : register(t0, space1); // Base Color (Albedo)
Texture2D normalMap   : register(t1, space1); // Tangent Space Normal Map
Texture2D pbrMap      : register(t2, space1); // R: Roughness, G: Metallic, B: Cavity, A: AO
Texture2D emissiveMap : register(t3, space1); // Emission RGB

SamplerState materialSampler : register(s0, space1);

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT output;

	// 1. Albedo (Diffuse)
	float4 diffuse = gMaterial.baseColor * input.Color;
	[branch]
	if (gMaterial.materialFlags & FLAG_HAS_DIFFUSE_MAP)
	{
		diffuse *= diffuseMap.Sample(materialSampler, input.TexCoord);
	}
	if (diffuse.a < 0.01f)
		discard;

	// 2. 법선 계산 (Normal)
	float3 N = normalize(input.Normal);
	float3 worldNormal = N;
	[branch]
	if (gMaterial.materialFlags & FLAG_HAS_NORMAL_MAP)
	{
		float3 T = normalize(input.Tangent - dot(input.Tangent, N) * N);
		float3 B = cross(N, T);
		float3x3 TBN = float3x3(T, B, N);

		float4 normalSample = normalMap.Sample(materialSampler, input.TexCoord);
		float3 tangentNormal = normalSample.xyz * 2.0f - 1.0f;
		bool isNormalValid = (normalSample.z > 0.45f && abs(length(tangentNormal) - 1.0f) < 0.4f);
		if (isNormalValid)
			worldNormal = normalize(mul(tangentNormal, TBN));
	}

	// 3. PBR 속성 (Roughness, Metallic, Cavity, AO)
	float roughness = clamp(gMaterial.roughness, 0.04f, 1.0f);
	float metallic = saturate(gMaterial.metallic);
	float cavity = clamp(gMaterial.cavity, 0.0f, 1.0f);
	float ao = clamp(gMaterial.ao, 0.0f, 1.0f);
	[branch]
	if (gMaterial.materialFlags & FLAG_HAS_PBR_MAP)
	{
		float4 pbrSample = pbrMap.Sample(materialSampler, input.TexCoord);
		roughness = clamp(pbrSample.r, 0.04f, 1.0f);
		metallic = saturate(pbrSample.g);
		cavity = (pbrSample.b > 0.01f) ? pbrSample.b : cavity;
		ao = (pbrSample.a > 0.01f) ? pbrSample.a : ao;
	}

	// 4. 발광 (Emission)
	float3 emission = gMaterial.emissiveColor;
	[branch]
	if (gMaterial.materialFlags & FLAG_HAS_EMISSIVE_MAP)
	{
		float4 emissionSample = emissiveMap.Sample(materialSampler, input.TexCoord);
		emission = emissionSample.rgb;
	}

	uint defaultMeshFlags = FLAG_RECEIVE_SHADOWS | FLAG_RECEIVE_DECALS | FLAG_APPLY_SSAO;

	output.Diffuse = diffuse;
	output.Normal = float4(worldNormal * 0.5f + 0.5f, PackShadingData(SHADING_MODEL_DEFAULT_LIT, defaultMeshFlags));
	output.PBR = float4(roughness, metallic, cavity, ao);
	output.Emission = float4(emission, 1.0f);
	output.Position = float4(input.WorldPos, input.PickingId);

	return output;
}