// 레이트레이싱 반사의 공간 필터 (cs_6_5, DirectX12 전용)
// 누적 결과를 같은 면 안에서만 섞는다. 거칠수록, 닿은 물체가 멀수록, 누적이 짧을수록 넓게 섞는다.
#include "common.hlsli"
#include "raytracing.hlsli"
#include "reflection.hlsli"

Texture2D gNormal : register(t1, space3);
Texture2D gPBR : register(t2, space3);
Texture2D gPosition : register(t4, space3);
RWTexture2D<float4> gFiltered : register(u2, space3);
ConstantBuffer<ReflectionDenoiseParams> gParams : register(b1, space3);

static const uint FILTER_TAPS = 8u;
static const float MAX_FILTER_RADIUS = 14.0f;

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint2 size;
	gFiltered.GetDimensions(size.x, size.y);
	if (any(dispatchId.xy >= size))
		return;

	const int2 pixel = int2(dispatchId.xy);
	const float4 center = gBindlessTextures[gParams.InputTexture].Load(int3(pixel, 0));
	const float4 geometry = gBindlessTextures[gParams.GeometryTexture].Load(int3(pixel, 0));
	const float roughness = max(gPBR.Load(int3(pixel, 0)).r, 0.04f);
	if (geometry.a < 0.5f || roughness < gParams.MirrorRoughness)
	{
		gFiltered[pixel] = float4(ExpandRadiance(center.rgb), center.a);
		return;
	}

	const float3 normal = normalize(gNormal.Load(int3(pixel, 0)).rgb * 2.0f - 1.0f);
	const float3 position = gPosition.Load(int3(pixel, 0)).xyz;
	const float hitDistance = gBindlessTextures[gParams.HitDistanceTexture].Load(int3(pixel, 0)).r;
	const float roughAmount = saturate((roughness - gParams.MirrorRoughness) / max(gParams.RoughnessFadeEnd - gParams.MirrorRoughness, 0.01f));
	// 가까이 닿은 반사는 거칠어도 또렷하다.
	const float spread = saturate(hitDistance / (hitDistance + geometry.r * 0.5f));
	const float unstable = lerp(1.6f, 1.0f, saturate(geometry.a / 8.0f));
	const float radius = MAX_FILTER_RADIUS * roughAmount * lerp(0.35f, 1.0f, spread) * unstable;

	float4 sum = center;
	float total = 1.0f;
	// 프레임마다 돌리면 누적이 끝난 결과도 계속 흔들리므로 픽셀마다 한 번만 정한다.
	const float rotation = HashPixel(dispatchId.xy).x * 6.2831853f;
	for (uint i = 0u; i < FILTER_TAPS; ++i)
	{
		// 황금각으로 원 안에 고르게 놓는다.
		const float angle = rotation + (float)i * 2.3999632f;
		const float tapRadius = radius * sqrt(((float)i + 0.5f) / (float)FILTER_TAPS);
		const int2 tap = pixel + int2(round(float2(cos(angle), sin(angle)) * tapRadius));
		if (any(tap < 0) || any(tap >= int2(size)) || all(tap == pixel))
			continue;
		const float4 tapGeometry = gBindlessTextures[gParams.GeometryTexture].Load(int3(tap, 0));
		if (tapGeometry.a < 0.5f)
			continue;

		const float3 tapNormal = DecodeNormal(tapGeometry.gb);
		const float3 tapPosition = gPosition.Load(int3(tap, 0)).xyz;
		const float tapRoughness = max(gPBR.Load(int3(tap, 0)).r, 0.04f);
		const float planeDistance = abs(dot(normal, tapPosition - position)) / max(geometry.r, 0.1f);
		const float weight = pow(saturate(dot(normal, tapNormal)), 32.0f) * exp(-planeDistance * 200.0f) * exp(-abs(tapRoughness - roughness) * 12.0f);
		sum += gBindlessTextures[gParams.InputTexture].Load(int3(tap, 0)) * weight;
		total += weight;
	}
	const float4 filtered = sum / total;
	gFiltered[pixel] = float4(ExpandRadiance(filtered.rgb), filtered.a);
}
