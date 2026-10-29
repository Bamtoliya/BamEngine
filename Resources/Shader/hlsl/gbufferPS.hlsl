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

// ── 머티리얼 텍스처 슬롯 (Space 1) ──
Texture2D diffuseMap  : register(t0, space1); // Base Color (Albedo)
Texture2D normalMap   : register(t1, space1); // Tangent Space Normal Map
Texture2D pbrMap      : register(t2, space1); // R: Roughness, G: Metallic, B: Cavity, A: AO
Texture2D emissiveMap : register(t3, space1); // Emission RGB

SamplerState materialSampler : register(s0, space1);

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 diffuse = diffuseMap.Sample(materialSampler, input.TexCoord) * input.Color;
	if (diffuse.a < 0.01f)
		discard;

	// 1. 탄젠트 공간 법선 계산 (TBN)
	float3 N = normalize(input.Normal);
	float3 T = normalize(input.Tangent - dot(input.Tangent, N) * N);
	float3 B = cross(N, T);
	float3x3 TBN = float3x3(T, B, N);

	float4 normalSample = normalMap.Sample(materialSampler, input.TexCoord);
	float3 tangentNormal = normalSample.xyz * 2.0f - 1.0f;
	// 노멀 맵 텍스처가 유효한지 검사 (푸른색 Z성분이 주도적인 경우에만 탄젠트 변환 적용, 아니면 정점 노멀 유지)
	bool isNormalValid = (normalSample.z > 0.45f && abs(length(tangentNormal) - 1.0f) < 0.4f);
	float3 worldNormal = isNormalValid ? normalize(mul(tangentNormal, TBN)) : N;

	// 2. PBR 속성 샘플링 (R: Roughness, G: Metallic, A: AO)
	float4 pbrSample = pbrMap.Sample(materialSampler, input.TexCoord);
	float roughness = clamp(pbrSample.r, 0.04f, 1.0f);
	float metallic = saturate(pbrSample.g);
	float cavity = (pbrSample.b > 0.01f) ? pbrSample.b : 0.5f;
	float ao = (pbrSample.a > 0.01f) ? pbrSample.a : 1.0f;

	// 3. 발광(Emission) 샘플링
	float4 emissionSample = emissiveMap.Sample(materialSampler, input.TexCoord);

	uint defaultMeshFlags = FLAG_RECEIVE_SHADOWS | FLAG_RECEIVE_DECALS | FLAG_APPLY_SSAO;

	output.Diffuse = diffuse;
	output.Normal = float4(worldNormal * 0.5f + 0.5f, PackShadingData(SHADING_MODEL_DEFAULT_LIT, defaultMeshFlags));
	output.PBR = float4(roughness, metallic, cavity, ao);
	output.Emission = float4(emissionSample.rgb, 1.0f);
	output.Position = float4(input.WorldPos, input.PickingId);

	return output;
}