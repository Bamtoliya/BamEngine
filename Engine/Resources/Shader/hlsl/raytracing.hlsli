// 번호로 읽는 텍스처·버퍼와 TLAS 인스턴스 표 (DXR 인라인 RayQuery, DirectX12 전용)
#ifndef BAM_RAYTRACING_HLSLI
#define BAM_RAYTRACING_HLSLI

#include "shading_flags.hlsli"

// RaytracingInstanceData.h와 일치해야 한다.
static const uint RAYTRACING_MASK_REFLECTION = 2u;
static const uint RAYTRACING_MASK_REFLECTION_SHADOW = 4u;
static const uint INVALID_BINDLESS_INDEX = 0xFFFFFFFFu;

struct RaytracedInstance
{
	uint PositionBuffer;
	uint MaterialBuffer;
	uint IndexBuffer;
	uint IndexStride;
	uint PositionStride;
	uint MaterialStride;
	uint MaterialFlags;
	uint BaseColorTexture;
	uint PbrTexture;
	uint EmissiveTexture;
	float Roughness;
	float Metallic;
	float4 BaseColor;
	float3 EmissiveColor;
	float AmbientOcclusion;
};
static const uint RAYTRACED_INSTANCE_STRIDE = 80u;

// VertexMaterial: 법선 0, UV 12, 정점 색 44
static const uint VERTEX_NORMAL_OFFSET = 0u;
static const uint VERTEX_UV_OFFSET = 12u;
static const uint VERTEX_COLOR_OFFSET = 44u;

Texture2D<float4> gBindlessTextures[] : register(t0, space4);
SamplerState gBindlessSampler : register(s0, space4);
ByteAddressBuffer gBindlessBuffers[] : register(t0, space5);

RaytracedInstance LoadRaytracedInstance(uint tableIndex, uint instanceIndex)
{
	ByteAddressBuffer table = gBindlessBuffers[tableIndex];
	const uint address = instanceIndex * RAYTRACED_INSTANCE_STRIDE;
	const uint4 a = table.Load4(address);
	const uint4 b = table.Load4(address + 16u);
	const uint4 c = table.Load4(address + 32u);
	const uint4 e = table.Load4(address + 64u);

	RaytracedInstance instance;
	instance.PositionBuffer = a.x;
	instance.MaterialBuffer = a.y;
	instance.IndexBuffer = a.z;
	instance.IndexStride = a.w;
	instance.PositionStride = b.x;
	instance.MaterialStride = b.y;
	instance.MaterialFlags = b.z;
	instance.BaseColorTexture = b.w;
	instance.PbrTexture = c.x;
	instance.EmissiveTexture = c.y;
	instance.Roughness = asfloat(c.z);
	instance.Metallic = asfloat(c.w);
	instance.BaseColor = asfloat(table.Load4(address + 48u));
	instance.EmissiveColor = asfloat(e.xyz);
	instance.AmbientOcclusion = asfloat(e.w);
	return instance;
}

uint3 LoadTriangleIndices(RaytracedInstance instance, uint primitiveIndex)
{
	const uint first = primitiveIndex * 3u;
	if (instance.IndexStride == 0u)
		return uint3(first, first + 1u, first + 2u);

	ByteAddressBuffer indices = gBindlessBuffers[NonUniformResourceIndex(instance.IndexBuffer)];
	if (instance.IndexStride == 4u)
		return indices.Load3(first * 4u);

	// 16비트 인덱스 3개는 4바이트 경계에서 시작하지 않을 수 있다.
	const uint address = first * 2u;
	const uint2 words = indices.Load2(address & ~3u);
	if ((address & 2u) == 0u)
		return uint3(words.x & 0xFFFFu, words.x >> 16u, words.y & 0xFFFFu);
	return uint3(words.x >> 16u, words.y & 0xFFFFu, words.y >> 16u);
}

float2 LoadVertexUV(RaytracedInstance instance, uint vertex)
{
	return asfloat(gBindlessBuffers[NonUniformResourceIndex(instance.MaterialBuffer)].Load2(vertex * instance.MaterialStride + VERTEX_UV_OFFSET));
}

float2 InterpolateUV(RaytracedInstance instance, uint3 indices, float3 weights)
{
	return LoadVertexUV(instance, indices.x) * weights.x + LoadVertexUV(instance, indices.y) * weights.y + LoadVertexUV(instance, indices.z) * weights.z;
}

float3 GetBarycentricWeights(float2 barycentrics)
{
	return float3(1.0f - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
}

// Masked 재질의 알파 검사. 광선마다 여러 번 부르므로 낮은 밉을 쓴다.
bool PassesAlphaTest(RaytracedInstance instance, uint primitiveIndex, float2 barycentrics)
{
	float alpha = instance.BaseColor.a;
	if (instance.BaseColorTexture != INVALID_BINDLESS_INDEX)
	{
		const float2 uv = InterpolateUV(instance, LoadTriangleIndices(instance, primitiveIndex), GetBarycentricWeights(barycentrics));
		alpha *= gBindlessTextures[NonUniformResourceIndex(instance.BaseColorTexture)].SampleLevel(gBindlessSampler, uv, 1.0f).a;
	}
	return alpha >= GetAlphaDiscardThreshold(instance.MaterialFlags);
}

#endif
