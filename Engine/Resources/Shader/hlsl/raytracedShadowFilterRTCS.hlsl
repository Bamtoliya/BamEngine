// 레이트레이싱 그림자의 공간 필터 (cs_6_5, DirectX12 전용)
// 누적 결과를 같은 면 안에서 반그림자 반지름만큼 흐린다. 반그림자가 없는 채널(광원 크기 0, 그림자 밖이나 안)은 흐리지 않는다.
// 결과를 누적에 되먹이지 않아 시간이 지나도 더 번지지 않는다.
#include "common.hlsli"
#include "raytracing.hlsli"
#include "reflection.hlsli"
#include "raytracedShadow.hlsli"

Texture2D gPosition : register(t4, space3);
RWTexture2D<float4> gFiltered : register(u2, space3);
ConstantBuffer<RaytracedShadowDenoiseParams> gParams : register(b1, space3);

static const uint FILTER_TAPS = 12u;
// 픽셀 단위 최대 반지름. 넘는 반그림자는 이 폭으로 흐린다.
static const float MAX_FILTER_RADIUS = 24.0f;

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
	if (geometry.a < 0.5f)
	{
		gFiltered[pixel] = center;
		return;
	}

	// 반그림자 반지름(m)을 픽셀로 바꾼다. 누적이 짧을수록 표본이 적어 조금 더 넓게 흐린다.
	const float pixelSize = max(gParams.PixelSizeConstant + geometry.r * gParams.PixelSizePerMeter, 1e-5f);
	const float4 penumbra = gBindlessTextures[gParams.PenumbraTexture].Load(int3(pixel, 0));
	const float unstable = lerp(1.5f, 1.0f, saturate(geometry.a / 8.0f));
	const float4 radius = min(penumbra / pixelSize * unstable, MAX_FILTER_RADIUS);
	const float maxRadius = max(max(radius.x, radius.y), max(radius.z, radius.w));
	if (maxRadius < 0.5f)
	{
		gFiltered[pixel] = center;
		return;
	}

	const float3 normal = DecodeNormal(geometry.gb);
	const float3 position = gPosition.Load(int3(pixel, 0)).xyz;
	// 채널마다 반지름이 달라 표본은 가장 넓은 채널에 맞춰 놓고, 채널마다 거리에 따른 무게를 따로 준다.
	const float4 sigma = max(radius * 0.5f, 1e-3f);
	const float4 active = (float4)(radius >= 0.5f);
	float4 sum = center;
	float4 total = float4(1.0f, 1.0f, 1.0f, 1.0f);
	// 프레임마다 돌리면 누적이 끝난 결과도 계속 흔들리므로 픽셀마다 한 번만 정한다.
	const float rotation = HashPixel(dispatchId.xy).x * 6.2831853f;
	for (uint i = 0u; i < FILTER_TAPS; ++i)
	{
		// 황금각으로 원 안에 고르게 놓는다.
		const float angle = rotation + (float)i * 2.3999632f;
		const float tapRadius = maxRadius * sqrt(((float)i + 0.5f) / (float)FILTER_TAPS);
		const int2 tap = pixel + int2(round(float2(cos(angle), sin(angle)) * tapRadius));
		if (any(tap < 0) || any(tap >= int2(size)) || all(tap == pixel))
			continue;
		const float4 tapGeometry = gBindlessTextures[gParams.GeometryTexture].Load(int3(tap, 0));
		if (tapGeometry.a < 0.5f)
			continue;

		const float3 tapPosition = gPosition.Load(int3(tap, 0)).xyz;
		const float planeDistance = abs(dot(normal, tapPosition - position)) / max(geometry.r, 0.1f);
		const float surfaceWeight = pow(saturate(dot(normal, DecodeNormal(tapGeometry.gb))), 32.0f) * exp(-planeDistance * 200.0f);
		const float tapDistance = length(float2(tap - pixel));
		const float4 weight = surfaceWeight * active * exp(-(tapDistance * tapDistance) / (2.0f * sigma * sigma));
		sum += gBindlessTextures[gParams.InputTexture].Load(int3(tap, 0)) * weight;
		total += weight;
	}
	gFiltered[pixel] = sum / total;
}
