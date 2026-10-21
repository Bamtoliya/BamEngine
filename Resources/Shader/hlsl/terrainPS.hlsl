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

// ── 머티리얼 텍스처 슬롯 (space1) ──
Texture2D diffuseMap : register(t0, space1);       // Slot 0: Layer 0 잔디
Texture2D heightMap : register(t1, space1);        // Slot 1: 지형 높이맵
Texture2D rockMap : register(t2, space1);          // Slot 2: Layer 1 암석
Texture2D dirtMap : register(t3, space1);          // Slot 3: Layer 2 흙
Texture2D splatMap : register(t4, space1);         // Slot 4: 4채널 스플랫 맵 (RGBA)
SamplerState terrainSampler : register(s0, space1);

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT output;

	// 1. 월드 법선 정규화 및 경사도(Slope: 1=수평 평지, 0=수직 절벽) 계산
	float3 N = normalize(input.Normal);
	float slope = saturate(N.y);

	// 2. 월드 XZ 좌표 기반 텍스처 타일링 UV
	float2 uvTiled = input.WorldPos.xz * 0.2f;

	// 3. 머티리얼 텍스처 샘플링
	float4 grassTex = diffuseMap.Sample(terrainSampler, uvTiled) * float4(0.35f, 0.65f, 0.25f, 1.0f);
	float4 rockTex  = rockMap.Sample(terrainSampler, uvTiled)    * float4(0.48f, 0.45f, 0.42f, 1.0f);
	float4 dirtTex  = dirtMap.Sample(terrainSampler, uvTiled)    * float4(0.55f, 0.42f, 0.28f, 1.0f);

	// 4. 스플랫 맵(Splatmap) 가중치 샘플링 (전체 지형 UV 기준)
	float4 splat = splatMap.Sample(terrainSampler, input.TexCoord);

	// 5. 사용자가 칠한 레이어 가중치 합성
	float4 paintedColor = grassTex * splat.r + rockTex * splat.g + dirtTex * splat.b;

	// 6. 경사도(Slope) 기반 자동 암석 블렌딩 (가파른 절벽은 칠한 것과 무관하게 암석화)
	float cliffWeight = 1.0f - smoothstep(0.55f, 0.78f, slope);
	float4 finalColor = lerp(paintedColor, rockTex, cliffWeight) * input.Color;

	// 7. GBuffer 출력
	uint defaultMeshFlags = FLAG_RECEIVE_SHADOWS | FLAG_RECEIVE_DECALS | FLAG_APPLY_SSAO;

	output.Diffuse = finalColor;
	output.Normal = float4(N * 0.5f + 0.5f, PackShadingData(SHADING_MODEL_DEFAULT_LIT, defaultMeshFlags));
	output.PBR = float4(lerp(0.85f, 0.65f, cliffWeight), 0.0f, 0.5f, 1.0f);
	output.Emission = float4(0.0f, 0.0f, 0.0f, 0.0f);
	output.Position = float4(input.WorldPos, input.PickingId);

	return output;
}