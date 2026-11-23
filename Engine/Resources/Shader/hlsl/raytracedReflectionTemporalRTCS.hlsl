// 레이트레이싱 반사의 시간 누적 (cs_6_5, DirectX12 전용)
// 이전 프레임에서 같은 면이 보이던 위치를 찾아 누적 결과를 이어 쓴다. TAA의 모션 벡터가 있으면 움직인 면(정적·스킨드 메시,
// 지형, 스플라인 메시)은 그 이동량으로 이전 위치를 찾고 거리 비교는 생략한다(이전 월드 위치를 모른다). 없으면 움직인 면은
// 거리와 법선이 맞지 않아 이력을 버린다. 남은 잔상은 이번 프레임 주변 픽셀의 범위로 자른다.
#include "common.hlsli"
#include "raytracing.hlsli"
#include "reflection.hlsli"

Texture2D gNormal : register(t1, space3);
Texture2D gPBR : register(t2, space3);
Texture2D gPosition : register(t4, space3);
RWTexture2D<float4> gAccumulated : register(u2, space3);
RWTexture2D<float4> gGeometry : register(u3, space3);
ConstantBuffer<ReflectionDenoiseParams> gParams : register(b1, space3);

float4 LoadBindless(uint index, int2 pixel)
{
	return gBindlessTextures[index].Load(int3(pixel, 0));
}

// 이번 프레임의 추적 결과(밝기를 누른 값)
float4 LoadCurrent(int2 pixel)
{
	const float4 value = LoadBindless(gParams.InputTexture, pixel);
	return float4(CompressRadiance(value.rgb), value.a);
}

// 이전 화면의 uv 주변 4픽셀 중 같은 면인 것만 섞는다. 없으면 false.
// expectedDistance가 0 이하면 거리를 비교하지 않는다(움직인 면은 이전 카메라 거리를 알 수 없다).
bool SampleHistory(float2 uv, uint2 size, float expectedDistance, float distanceTolerance, float3 normal, out float4 history, out float historyLength)
{
	history = float4(0.0f, 0.0f, 0.0f, 0.0f);
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
			const bool distanceMismatch = expectedDistance > 0.0f && abs(geometry.r - expectedDistance) > expectedDistance * distanceTolerance;
			if (geometry.a < 0.5f || distanceMismatch || dot(DecodeNormal(geometry.gb), normal) < 0.9f)
				continue;
			const float weight = (x == 0 ? 1.0f - f.x : f.x) * (y == 0 ? 1.0f - f.y : f.y);
			history += LoadBindless(gParams.HistoryTexture, tap) * weight;
			historyLength += geometry.a * weight;
			total += weight;
		}
	}
	if (total < 0.05f)
		return false;
	history /= total;
	historyLength /= total;
	return true;
}

// 이번 화면과 이전 화면의 투영 차이만큼 픽셀에서 옮긴다. TAA가 켜지면 G-Buffer가 픽셀 중심에서 조금씩 어긋난 위치를 담으므로
// 이전 화면의 투영을 그대로 쓰면 멈춘 화면에서도 이력이 매 프레임 흐려진다.
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

// 이번 프레임의 모션 벡터가 이 픽셀 면의 이동을 담고 있으면 true. 카메라만 움직인 면은 NO_MOTION이다.
bool LoadSurfaceMotion(int2 pixel, out float2 motion)
{
	motion = float2(0.0f, 0.0f);
	if (gParams.MotionTexture == REFLECTION_INVALID_TEXTURE)
		return false;
	const float2 value = LoadBindless(gParams.MotionTexture, pixel).xy;
	if (!all(abs(value) < REFLECTION_NO_MOTION * 0.5f))
		return false;
	motion = value;
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
	const float2 pixelUV = (float2(dispatchId.xy) + 0.5f) / float2(size);
	const float hitDistance = LoadBindless(gParams.HitDistanceTexture, pixel).r;
	if (hitDistance < 0.0f)
	{
		gAccumulated[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
		gGeometry[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
		return;
	}

	const float3 normal = normalize(gNormal.Load(int3(pixel, 0)).rgb * 2.0f - 1.0f);
	const float3 position = gPosition.Load(int3(pixel, 0)).xyz;
	const float roughness = max(gPBR.Load(int3(pixel, 0)).r, 0.04f);
	const float cameraDistance = length(position - commonData.CameraPos);
	const float4 current = LoadCurrent(pixel);

	float4 result = current;
	float length1 = 1.0f;
	// 거울 방향 하나로 정해지는 면은 누적할 노이즈가 없고, 누적하면 움직이는 반사가 늦는다.
	if (gParams.HistoryState != REFLECTION_HISTORY_NONE && roughness >= gParams.MirrorRoughness)
	{
		float4 history = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float historyLength = 0.0f;
		bool found = false;
		float2 uv;

		// 면 자체가 움직였으면(모션 벡터가 있으면) 월드 위치가 고정이라는 가정이 깨진다. 모션 벡터에는 카메라 이동도 들어 있다.
		float2 surfaceMotion;
		const bool surfaceMoved = LoadSurfaceMotion(pixel, surfaceMotion);

		// 매끈한 면의 반사는 면이 아니라 닿은 물체를 따라 움직인다. 거울 뒤의 가상 위치로 이전 화면을 찾는다.
		const float smoothness = saturate((0.35f - roughness) / 0.2f);
		if (!surfaceMoved && smoothness > 0.5f && hitDistance < REFLECTION_MISS_DISTANCE)
		{
			const float3 viewDirection = (position - commonData.CameraPos) / max(cameraDistance, 1e-4f);
			if (ProjectPrevious(position + viewDirection * hitDistance, pixelUV, uv))
				found = SampleHistory(uv, size, length(position - gParams.PrevCameraPosition), 0.15f, normal, history, historyLength);
		}
		if (surfaceMoved)
		{
			uv = pixelUV + surfaceMotion;
			found = SampleHistory(uv, size, 0.0f, 0.0f, normal, history, historyLength);
		}
		else if (!found && ProjectPrevious(position, pixelUV, uv))
		{
			found = SampleHistory(uv, size, length(position - gParams.PrevCameraPosition), 0.02f, normal, history, historyLength);
		}

		if (found)
		{
			float4 mean = float4(0.0f, 0.0f, 0.0f, 0.0f);
			float4 squared = float4(0.0f, 0.0f, 0.0f, 0.0f);
			float count = 0.0f;
			for (int y = -2; y <= 2; ++y)
			{
				for (int x = -2; x <= 2; ++x)
				{
					const int2 tap = clamp(pixel + int2(x, y), int2(0, 0), int2(size) - 1);
					if (LoadBindless(gParams.HitDistanceTexture, tap).r < 0.0f)
						continue;
					const float4 value = LoadCurrent(tap);
					mean += value;
					squared += value * value;
					count += 1.0f;
				}
			}
			mean /= max(count, 1.0f);
			const float4 deviation = sqrt(max(squared / max(count, 1.0f) - mean * mean, 0.0f));
			const float4 range = deviation * 2.0f + 0.01f;
			// 주변 픽셀도 표본이 하나뿐이라 범위 자체가 흔들린다. 카메라가 멈췄을 때 그대로 자르면 노이즈가 계속 다시 들어오므로
			// 조금씩만 당긴다. 반사된 물체가 움직여 생긴 잔상은 이것으로 서서히 사라진다.
			// 면이 움직이면 카메라가 멈춰도 반사가 바뀌므로 움직이는 카메라와 같이 짧게 쌓고 범위로 자른다.
			const bool isStatic = gParams.HistoryState == REFLECTION_HISTORY_STATIC && !surfaceMoved;
			history = lerp(history, clamp(history, mean - range, mean + range), isStatic ? 0.08f : 1.0f);

			const float roughAmount = saturate((roughness - gParams.MirrorRoughness) / max(0.4f - gParams.MirrorRoughness, 0.01f));
			const float maxHistory = isStatic ? lerp(24.0f, gParams.MaxHistory, roughAmount) : lerp(6.0f, gParams.MaxHistory * 0.4f, roughAmount);
			length1 = min(historyLength + 1.0f, maxHistory);
			result = lerp(history, current, 1.0f / length1);
		}
	}

	gAccumulated[pixel] = result;
	gGeometry[pixel] = float4(cameraDistance, EncodeNormal(normal), length1);
}
