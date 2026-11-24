#include "shading_flags.hlsli"
#include "terrainLayers.hlsli"

// 기본 지형 표면. 셰이더 그래프 Terrain 도메인이 같은 함수(terrainLayers.hlsli)로 같은 결과를 만들 수 있다.

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

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT output;
	const float3 N = normalize(input.Normal);
	const TerrainPixel pixel = TerrainBeginPixel(input.WorldPos, N, input.TexCoord);

	// 칠한 레이어 합성. 가파른 절벽(경사 0.55~0.78 사이에서 전환)은 칠한 것과 무관하게 레이어 1(암석)로 덮는다.
	const float4 paintedColor = TerrainBlendLayers(pixel);
	const float cliffWeight = 1.0f - smoothstep(0.55f, 0.78f, pixel.Slope);
	const float4 cliffColor = cliffWeight > 0.0f ? TerrainSampleLayer(pixel, 1) : paintedColor;
	const float4 finalColor = lerp(paintedColor, cliffColor, cliffWeight) * input.Color;

	const uint defaultMeshFlags = FLAG_RECEIVE_SHADOWS | FLAG_RECEIVE_DECALS | FLAG_APPLY_SSAO;
	output.Diffuse = finalColor;
	output.Normal = float4(N * 0.5f + 0.5f, PackShadingData(SHADING_MODEL_DEFAULT_LIT, defaultMeshFlags));
	output.PBR = float4(lerp(0.85f, 0.65f, cliffWeight), 0.0f, 0.5f, 1.0f);
	output.Emission = float4(0.0f, 0.0f, 0.0f, 0.0f);
	output.Position = float4(input.WorldPos, input.PickingId);
	return output;
}
