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

	if (TerrainIsHole(pixel))
		clip(-1);

	// 칠한 레이어 PBR 합성. 가파른 절벽(경사 0.55~0.78 사이에서 전환)은 칠한 것과 무관하게 레이어 1(암석)로 덮는다.
	float4 paintedColor;
	float3 paintedNormalTS;
	float paintedRoughness;
	float paintedMetallic;
	TerrainBlendLayersPBR(pixel, paintedColor, paintedNormalTS, paintedRoughness, paintedMetallic);

	const float cliffWeight = 1.0f - smoothstep(0.55f, 0.78f, pixel.Slope);
	float4 finalColor = paintedColor;
	float3 finalNormalTS = paintedNormalTS;
	float finalRoughness = paintedRoughness;
	float finalMetallic = paintedMetallic;
	[branch] if (cliffWeight > 0.0f)
	{
		finalColor = lerp(paintedColor, TerrainSampleLayer(pixel, 1), cliffWeight);
		finalNormalTS = normalize(lerp(paintedNormalTS, TerrainSampleLayerNormalTS(pixel, 1), cliffWeight));
		finalRoughness = lerp(paintedRoughness, TerrainLayerRoughness(1), cliffWeight);
		finalMetallic = lerp(paintedMetallic, TerrainLayerMetallic(1), cliffWeight);
	}
	finalColor *= input.Color;

	const float3 T = normalize(input.Tangent - dot(input.Tangent, N) * N);
	const float3 B = cross(N, T);
	const float3x3 TBN = float3x3(T, B, N);
	const float3 worldNormal = normalize(mul(finalNormalTS, TBN));

	const uint defaultMeshFlags = FLAG_RECEIVE_SHADOWS | FLAG_RECEIVE_DECALS | FLAG_APPLY_SSAO;
	output.Diffuse = finalColor;
	output.Normal = float4(worldNormal * 0.5f + 0.5f, PackShadingData(SHADING_MODEL_DEFAULT_LIT, defaultMeshFlags));
	output.PBR = float4(clamp(finalRoughness, 0.04f, 1.0f), saturate(finalMetallic), 0.5f, 1.0f);
	output.Emission = float4(0.0f, 0.0f, 0.0f, 0.0f);
	output.Position = float4(input.WorldPos, input.PickingId);
	return output;
}
