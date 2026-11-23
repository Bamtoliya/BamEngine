// 앰비언트 오클루전 공간 필터와 시간 누적 (DirectX12 Compute)
// 같은 면의 5x5 이웃으로 이번 프레임 값을 고르게 만들고, 이전 결과를 이웃 범위로 잘라 섞는다.
// 출력: r 가시성(lightingPS가 t13으로 읽음), g 카메라 거리, b 누적 횟수.
// 물체별 모션 벡터가 없어 움직이는 물체의 이력은 이웃 범위로만 제한한다.
#include "ambientOcclusion.hlsli"

Texture2D<float> gRawOcclusion : register(t14, space3);
Texture2D<float4> gHistory : register(t15, space3);
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> gOutput : register(u2, space3);

static const int FILTER_RADIUS = 2;

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint width, height;
	gOutput.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	const int3 pixel = int3(dispatchId.xy, 0);
	float3 normal, position;
	if (!ReceivesAmbientOcclusion(pixel, normal, position))
	{
		gOutput[dispatchId.xy] = float4(1.0f, 0.0f, 0.0f, 1.0f);
		return;
	}

	const float cameraDistance = distance(position, gAOParams.CameraPosition);
	// 같은 면으로 볼 평면 거리. 멀수록 위치 정밀도가 낮아 넓힌다.
	const float planeTolerance = 0.02f + 0.01f * cameraDistance;

	// 1. 공간 필터와 이웃 통계
	float weightSum = 0.0f;
	float moment1 = 0.0f;
	float moment2 = 0.0f;
	[unroll]
	for (int y = -FILTER_RADIUS; y <= FILTER_RADIUS; ++y)
	{
		[unroll]
		for (int x = -FILTER_RADIUS; x <= FILTER_RADIUS; ++x)
		{
			const int2 neighbor = int2(dispatchId.xy) + int2(x, y);
			if (neighbor.x < 0 || neighbor.y < 0 || neighbor.x >= (int)width || neighbor.y >= (int)height)
				continue;
			float3 neighborNormal, neighborPosition;
			if (!ReceivesAmbientOcclusion(int3(neighbor, 0), neighborNormal, neighborPosition))
				continue;

			const float planeDistance = abs(dot(normal, neighborPosition - position));
			float weight = saturate(1.0f - planeDistance / planeTolerance);
			weight *= pow(saturate(dot(normal, neighborNormal)), 8.0f);
			if (weight <= 0.0f)
				continue;
			const float value = gRawOcclusion.Load(int3(neighbor, 0));
			weightSum += weight;
			moment1 += value * weight;
			moment2 += value * value * weight;
		}
	}
	// 중심 픽셀은 항상 가중치 1로 들어가 weightSum이 0이 되지 않는다.
	const float mean = moment1 / max(weightSum, 0.0001f);
	const float deviation = sqrt(max(moment2 / max(weightSum, 0.0001f) - mean * mean, 0.0f));

	// 2. 이전 화면에서 같은 면 찾기. 이전 G-Buffer의 지터만큼 옮겨 읽는다.
	float history = mean;
	float count = 0.0f;
	if (gAOParams.HistoryValid != 0u)
	{
		const float4 previousClip = mul(gAOParams.PrevViewProjection, float4(position, 1.0f));
		if (previousClip.w > 0.0001f)
		{
			const float2 previousUV = previousClip.xy / previousClip.w * float2(0.5f, -0.5f) + 0.5f + gAOParams.PrevJitterUV;
			if (all(previousUV >= 0.0f) && all(previousUV < 1.0f))
			{
				const float4 previous = gHistory.Load(int3(int2(previousUV * float2(width, height)), 0));
				const float expectedDistance = distance(position, gAOParams.PrevCameraPosition);
				// 거리가 다르면 다른 면이 드러난 것이므로 버린다.
				if (previous.b > 0.0f && abs(previous.g - expectedDistance) < 0.05f + 0.03f * expectedDistance)
				{
					history = previous.r;
					count = previous.b;
				}
			}
		}
	}

	// 3. 이력을 이웃 범위로 잘라 움직인 물체의 잔상을 줄이고 섞는다.
	const float margin = 2.0f * deviation + 0.05f;
	history = clamp(history, mean - margin, mean + margin);
	count = min(count + 1.0f, max(gAOParams.MaxHistory, 1.0f));
	const float result = saturate(lerp(history, mean, 1.0f / count));
	gOutput[dispatchId.xy] = float4(result, cameraDistance, count, 1.0f);
}
