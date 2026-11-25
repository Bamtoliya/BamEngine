#ifndef TERRAIN_LAYERS_HLSLI
#define TERRAIN_LAYERS_HLSLI

// 지형 픽셀 셰이더 공용 선언: terrainPS.hlsl과 셰이더 그래프 Terrain 도메인 생성 코드가 함께 포함한다.
// 바인딩은 BINDING_LAYOUT.md "지형 바인딩"과 Renderer.cpp의 TerrainObjectData·지형 슬롯 상수와 같아야 한다.

// ── 지형 드로우 상수 (b0, space2) ──
// 앞 96바이트는 terrainVS·terrainShadowDepthVS·motionVectorTerrainVS의 ObjectData와 같다.
struct TerrainObjectData
{
	float4x4 WorldMatrix;
	float4 PickingData;
	float4 HeightfieldData;   // x 최대 높이, yz 칸 간격
	float4 LayerTiling[2];    // 레이어 0~7 반복 길이(m): [i / 4][i % 4]
	float4 LayerTint[8];      // 레이어 색 배율(알베도가 없으면 기본 색)
	float4 LayerPbrParams[8]; // x: Roughness, y: Metallic, z: HasNormalMap(0/1), w: 미사용
};
ConstantBuffer<TerrainObjectData> terrainObject : register(b0, space2);

// ── 텍스처 ──
// t0~t7, space1은 머티리얼(셰이더 그래프) 범위라 지형 데이터를 두지 않는다.
Texture2D terrainLayerNormal[8] : register(t0, space3);   // 레이어 0~7 접선 공간 노멀 (t0~t7, space3)
Texture2D<float> terrainHeightMap : register(t8, space3); // 0~1 높이
Texture2D terrainSplatMap0 : register(t9, space3);        // 레이어 0~3 가중치 (RGBA)
Texture2D terrainSplatMap1 : register(t10, space3);       // 레이어 4~7 가중치 (RGBA)
Texture2D<float> terrainHoleMask : register(t11, space3); // 구멍 마스크 (1=Hole, 0=Solid)
Texture2D terrainLayerAlbedo[8] : register(t8, space1);   // 레이어 0~7 알베도 (t8~t15)

// 샘플러는 s0, space1 하나를 쓴다. 셰이더 그래프가 같은 레지스터에 materialSampler를 이미 선언했으면
// 포함하기 전에 TERRAIN_SAMPLER를 그 이름으로 정의한다(같은 레지스터 두 번 선언 방지).
#ifndef TERRAIN_SAMPLER
SamplerState terrainSampler : register(s0, space1);
#define TERRAIN_SAMPLER terrainSampler
#endif

// 픽셀마다 한 번 구하는 지형 값. TerrainBeginPixel로 채운다.
struct TerrainPixel
{
	float2 WorldXZ;
	float2 WorldDx;   // 분기 안 SampleGrad용 기울기
	float2 WorldDy;
	float4 Weights0;  // 정규화한 레이어 0~3 가중치
	float4 Weights1;  // 정규화한 레이어 4~7 가중치
	bool HasWeights;  // 칠한 가중치가 하나라도 있는가(없으면 레이어 0)
	float Slope;      // 1 평지, 0 수직
	float Height;     // 0~1 높이맵 값
	float Hole;       // 0~1 구멍 마스크(>= 0.5이면 절단)
};

TerrainPixel TerrainBeginPixel(float3 worldPos, float3 normal, float2 terrainUV)
{
	TerrainPixel pixel;
	pixel.WorldXZ = worldPos.xz;
	pixel.WorldDx = ddx(pixel.WorldXZ);
	pixel.WorldDy = ddy(pixel.WorldXZ);

	// 페인트가 합을 255로 맞추지만 양자화 오차를 고려해 다시 정규화한다.
	const float4 w0 = terrainSplatMap0.Sample(TERRAIN_SAMPLER, terrainUV);
	const float4 w1 = terrainSplatMap1.Sample(TERRAIN_SAMPLER, terrainUV);
	const float sum = dot(w0, 1.0f.xxxx) + dot(w1, 1.0f.xxxx);
	pixel.HasWeights = sum > 1e-4f;
	const float invSum = pixel.HasWeights ? 1.0f / sum : 0.0f;
	pixel.Weights0 = w0 * invSum;
	pixel.Weights1 = w1 * invSum;
	if (!pixel.HasWeights) pixel.Weights0 = float4(1.0f, 0.0f, 0.0f, 0.0f);

	pixel.Slope = saturate(normalize(normal).y);

	// 지형 UV(0~1)를 정점 격자로 바꿔 가장 가까운 높이를 읽는다(지형 이동·회전과 무관).
	uint width, height;
	terrainHeightMap.GetDimensions(width, height);
	const int2 texel = int2(saturate(terrainUV) * float2(width - 1, height - 1) + 0.5f);
	pixel.Height = terrainHeightMap.Load(int3(texel, 0));
	pixel.Hole = terrainHoleMask.Sample(TERRAIN_SAMPLER, terrainUV);
	return pixel;
}

bool TerrainIsHole(TerrainPixel pixel)
{
	return pixel.Hole >= 0.5f;
}

float TerrainLayerRoughness(uint layer)
{
	return clamp(terrainObject.LayerPbrParams[layer].x, 0.04f, 1.0f);
}

float TerrainLayerMetallic(uint layer)
{
	return saturate(terrainObject.LayerPbrParams[layer].y);
}

bool TerrainLayerHasNormal(uint layer)
{
	return terrainObject.LayerPbrParams[layer].z > 0.5f;
}

float TerrainLayerWeight(TerrainPixel pixel, uint layer)
{
	return layer < 4 ? pixel.Weights0[layer] : pixel.Weights1[layer - 4];
}

float TerrainLayerTiling(uint layer)
{
	return max(terrainObject.LayerTiling[layer / 4][layer % 4], 0.01f);
}

// 레이어 하나의 알베도 × 색 배율. 월드 XZ 기준 반복이며 기울기를 넘겨 분기 안에서도 밉맵이 맞다.
float4 TerrainSampleLayer(TerrainPixel pixel, uint layer)
{
	const float invTiling = 1.0f / TerrainLayerTiling(layer);
	const float2 uv = pixel.WorldXZ * invTiling;
	const float2 dx = pixel.WorldDx * invTiling;
	const float2 dy = pixel.WorldDy * invTiling;
	float4 albedo = float4(1.0f, 1.0f, 1.0f, 1.0f);
	// 리소스 배열의 동적 인덱스를 피하려고 분기한다(호출부의 layer가 상수면 하나로 접힌다).
	switch (layer)
	{
	case 0: albedo = terrainLayerAlbedo[0].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 1: albedo = terrainLayerAlbedo[1].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 2: albedo = terrainLayerAlbedo[2].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 3: albedo = terrainLayerAlbedo[3].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 4: albedo = terrainLayerAlbedo[4].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 5: albedo = terrainLayerAlbedo[5].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 6: albedo = terrainLayerAlbedo[6].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	default: albedo = terrainLayerAlbedo[7].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	}
	return albedo * terrainObject.LayerTint[layer];
}

// 레이어 하나의 접선 공간 노멀. 노멀 텍스처가 없거나 값이 비정상이면 기본 접선 노멀(0, 0, 1)을 반환한다.
float3 TerrainSampleLayerNormalTS(TerrainPixel pixel, uint layer)
{
	[branch] if (!TerrainLayerHasNormal(layer))
		return float3(0.0f, 0.0f, 1.0f);

	const float invTiling = 1.0f / TerrainLayerTiling(layer);
	const float2 uv = pixel.WorldXZ * invTiling;
	const float2 dx = pixel.WorldDx * invTiling;
	const float2 dy = pixel.WorldDy * invTiling;
	float4 normalSample = float4(0.5f, 0.5f, 1.0f, 1.0f);
	switch (layer)
	{
	case 0: normalSample = terrainLayerNormal[0].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 1: normalSample = terrainLayerNormal[1].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 2: normalSample = terrainLayerNormal[2].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 3: normalSample = terrainLayerNormal[3].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 4: normalSample = terrainLayerNormal[4].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 5: normalSample = terrainLayerNormal[5].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	case 6: normalSample = terrainLayerNormal[6].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	default: normalSample = terrainLayerNormal[7].SampleGrad(TERRAIN_SAMPLER, uv, dx, dy); break;
	}
	const float3 tangentNormal = normalSample.xyz * 2.0f - 1.0f;
	const bool isNormalValid = (normalSample.z > 0.45f && abs(length(tangentNormal) - 1.0f) < 0.4f);
	return isNormalValid ? normalize(tangentNormal) : float3(0.0f, 0.0f, 1.0f);
}

// 가중치가 있는 레이어만 읽어 섞는다.
float4 TerrainBlendLayers(TerrainPixel pixel)
{
	float4 color = 0.0f.xxxx;
	[unroll] for (uint layer = 0; layer < 8; ++layer)
	{
		const float weight = TerrainLayerWeight(pixel, layer);
		[branch] if (weight > 0.0f)
			color += TerrainSampleLayer(pixel, layer) * weight;
	}
	return color;
}

// 가중치가 있는 레이어의 알베도·접선 노멀·거칠기·금속성을 한 번에 섞는다.
void TerrainBlendLayersPBR(TerrainPixel pixel, out float4 outAlbedo, out float3 outTangentNormal, out float outRoughness, out float outMetallic)
{
	outAlbedo = 0.0f.xxxx;
	outTangentNormal = 0.0f.xxx;
	outRoughness = 0.0f;
	outMetallic = 0.0f;
	[unroll] for (uint layer = 0; layer < 8; ++layer)
	{
		const float weight = TerrainLayerWeight(pixel, layer);
		[branch] if (weight > 0.0f)
		{
			outAlbedo += TerrainSampleLayer(pixel, layer) * weight;
			outTangentNormal += TerrainSampleLayerNormalTS(pixel, layer) * weight;
			outRoughness += TerrainLayerRoughness(layer) * weight;
			outMetallic += TerrainLayerMetallic(layer) * weight;
		}
	}
	const float normalLenSq = dot(outTangentNormal, outTangentNormal);
	outTangentNormal = normalLenSq > 1e-6f ? outTangentNormal * rsqrt(normalLenSq) : float3(0.0f, 0.0f, 1.0f);
}

#endif
