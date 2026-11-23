// 레이트레이싱 그림자의 시간 누적 (cs_6_5, DirectX12 전용)
// 광원 크기가 있는 채널만 이전 프레임에서 같은 면이 보이던 위치의 누적 결과를 이어 쓴다. 크기가 0인 채널은 표본이 흔들리지
// 않으므로 이번 결과를 그대로 쓴다. 움직인 물체의 그림자가 남지 않도록 이력은 이번 프레임 주변 픽셀의 범위로 자른다.
#include "common.hlsli"
#include "light.hlsli"
#include "raytracing.hlsli"
#include "reflection.hlsli"
#include "raytracedShadow.hlsli"

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPosition : register(t4, space3);
RWTexture2D<float4> gAccumulated : register(u2, space3);
RWTexture2D<float4> gPenumbraOut : register(u3, space3);
RWTexture2D<float4> gGeometry : register(u4, space3);
ConstantBuffer<RaytracedShadowDenoiseParams> gParams : register(b1, space3);

float4 LoadBindless(uint index, int2 pixel)
{
	return gBindlessTextures[index].Load(int3(pixel, 0));
}

// 이전 화면의 uv 주변 4픽셀 중 같은 면인 것만 섞는다. 없으면 false.
bool SampleHistory(float2 uv, uint2 size, float expectedDistance, float3 normal, out float4 history, out float4 penumbra, out float historyLength)
{
	history = float4(0.0f, 0.0f, 0.0f, 0.0f);
	penumbra = float4(0.0f, 0.0f, 0.0f, 0.0f);
	historyLength = 0.0f;
	if (any(uv < 0.0f) || any(uv > 1.0f))
		return false;

	const float2 position = uv * float2(size) - 0.5f;
	const int2 base = int2(floor(position));
	const float2 f = position - float2(base);
	float total = 0.0f;
	for (int y = 0; y <= 1; ++y)
	{
		for (int x = 0; x <= 1; ++x)
		{
			const int2 tap = base + int2(x, y);
			if (any(tap < 0) || any(tap >= int2(size)))
				continue;
			const float4 geometry = LoadBindless(gParams.GeometryTexture, tap);
			// 추적하지 않은 픽셀은 누적 횟수가 0이다.
			if (geometry.a < 0.5f || abs(geometry.r - expectedDistance) > expectedDistance * 0.02f || dot(DecodeNormal(geometry.gb), normal) < 0.9f)
				continue;
			const float weight = (x == 0 ? 1.0f - f.x : f.x) * (y == 0 ? 1.0f - f.y : f.y);
			history += LoadBindless(gParams.HistoryTexture, tap) * weight;
			penumbra += LoadBindless(gParams.PenumbraHistoryTexture, tap) * weight;
			historyLength += geometry.a * weight;
			total += weight;
		}
	}
	if (total < 0.05f)
		return false;
	history /= total;
	penumbra /= total;
	historyLength /= total;
	return true;
}

// 이번 화면과 이전 화면의 투영 차이만큼 픽셀에서 옮긴다. TAA가 켜지면 G-Buffer가 픽셀 중심에서 조금씩 어긋난 위치를 담으므로
// 이전 화면의 투영을 그대로 쓰면 멈춘 화면에서도 이력이 매 프레임 흐려진다(raytracedReflectionTemporalRTCS와 같다).
bool ProjectPrevious(float3 position, float2 pixelUV, out float2 uv)
{
	const float4 current = mul(commonData.ViewProjection, float4(position, 1.0f));
	const float4 previous = mul(gParams.PrevViewProjection, float4(position, 1.0f));
	uv = float2(-1.0f, -1.0f);
	if (current.w <= 0.0f || previous.w <= 0.0f)
		return false;
	uv = pixelUV + (previous.xy / previous.w - current.xy / current.w) * float2(0.5f, -0.5f);
	return true;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint2 size;
	gAccumulated.GetDimensions(size.x, size.y);
	if (any(dispatchId.xy >= size))
		return;

	const int2 pixel = int2(dispatchId.xy);
	const float4 current = LoadBindless(gParams.InputTexture, pixel);
	const float4 normalSample = gNormal.Load(int3(pixel, 0));
	if (!IsShadowReceiver(gDiffuse.Load(int3(pixel, 0)), normalSample))
	{
		gAccumulated[pixel] = current;
		gPenumbraOut[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
		gGeometry[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
		return;
	}

	// 광원 크기가 있는 채널만 누적한다.
	float4 soft = float4(0.0f, 0.0f, 0.0f, 0.0f);
	for (uint channel = 0u; channel < MAX_RAYTRACED_SHADOW_LIGHTS; ++channel)
	{
		const uint lightIndex = gParams.Lights[channel];
		if (lightIndex != INVALID_LIGHT && LoadLight(lightIndex).SourceSize > 0.0f)
			soft[channel] = 1.0f;
	}

	const float3 normal = normalize(normalSample.rgb * 2.0f - 1.0f);
	const float3 position = gPosition.Load(int3(pixel, 0)).xyz;
	const float cameraDistance = length(position - commonData.CameraPos);
	const float2 pixelUV = (float2(dispatchId.xy) + 0.5f) / float2(size);
	const float4 currentPenumbra = LoadBindless(gParams.PenumbraTexture, pixel);

	float4 result = current;
	float4 penumbra = currentPenumbra;
	float length1 = 1.0f;
	float4 history, historyPenumbra;
	float historyLength;
	float2 uv;
	if (gParams.HistoryState != SHADOW_HISTORY_NONE && any(soft > 0.0f) && ProjectPrevious(position, pixelUV, uv) &&
		SampleHistory(uv, size, length(position - gParams.PrevCameraPosition), normal, history, historyPenumbra, historyLength))
	{
		// 이번 프레임 주변 5x5의 평균과 편차. 완전히 밝거나 어두운 곳은 편차가 0이라 움직인 물체의 그림자가 바로 바뀌고,
		// 반그림자 안은 표본이 0과 1로 흔들려 편차가 크므로 누적한 값이 그대로 남는다.
		float4 mean = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float4 squared = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float count = 0.0f;
		for (int y = -2; y <= 2; ++y)
		{
			for (int x = -2; x <= 2; ++x)
			{
				const int2 tap = clamp(pixel + int2(x, y), int2(0, 0), int2(size) - 1);
				if (gDiffuse.Load(int3(tap, 0)).a < 0.01f)
					continue;
				const float4 value = LoadBindless(gParams.InputTexture, tap);
				mean += value;
				squared += value * value;
				count += 1.0f;
			}
		}
		mean /= max(count, 1.0f);
		const float4 deviation = sqrt(max(squared / max(count, 1.0f) - mean * mean, 0.0f));
		const float4 range = deviation * 1.5f + 0.05f;
		history = clamp(history, mean - range, mean + range);

		const bool isStatic = gParams.HistoryState == SHADOW_HISTORY_STATIC;
		length1 = min(historyLength + 1.0f, isStatic ? gParams.MaxHistory : gParams.MaxHistory * 0.375f);
		result = lerp(history, current, 1.0f / length1);

		// 반그림자 반지름은 가려진 표본에서만 나온다. 이번에 가려지지 않았으면 이전 값을 조금씩 줄여 쓴다.
		const float4 hasHistory = (float4)(historyPenumbra > 0.0f);
		const float4 hasCurrent = (float4)(currentPenumbra > 0.0f);
		const float4 previousPenumbra = lerp(currentPenumbra, historyPenumbra, hasHistory);
		penumbra = lerp(historyPenumbra * 0.9f, lerp(previousPenumbra, currentPenumbra, max(1.0f / length1, 0.1f)), hasCurrent);
	}

	gAccumulated[pixel] = lerp(current, result, soft);
	gPenumbraOut[pixel] = penumbra * soft;
	gGeometry[pixel] = float4(cameraDistance, EncodeNormal(normal), length1);
}
