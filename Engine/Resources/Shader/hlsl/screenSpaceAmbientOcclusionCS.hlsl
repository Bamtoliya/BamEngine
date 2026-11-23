// 화면 공간 앰비언트 오클루전 (GTAO, Jimenez 2016 / XeGTAO 방식, DirectX12 Compute)
// G-Buffer의 월드 위치를 화면에서 따라가며 양쪽 지평선을 찾고, 법선 반구의 코사인 가중 가시성을 적분한다.
// 프레임마다 방향을 돌려 표본 수를 줄이고 ambientOcclusionTemporalCS가 누적한다.
// 화면 밖이나 앞 물체에 가린 차폐는 찾지 못한다.
#include "ambientOcclusion.hlsli"

[[vk::image_format("r32f")]]
RWTexture2D<float> gRawOcclusion : register(u2, space3);

float FastAcos(float x)
{
	const float absX = abs(x);
	float result = -0.156583f * absX + AO_PI * 0.5f;
	result *= sqrt(max(1.0f - absX, 0.0f));
	return x >= 0.0f ? result : AO_PI - result;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint width, height;
	gRawOcclusion.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	const int3 pixel = int3(dispatchId.xy, 0);
	float3 normal, position;
	if (!ReceivesAmbientOcclusion(pixel, normal, position))
	{
		gRawOcclusion[dispatchId.xy] = 1.0f;
		return;
	}

	const float3 toCamera = gAOParams.CameraPosition - position;
	const float cameraDistance = max(length(toCamera), 0.0001f);
	const float3 viewVec = toCamera / cameraDistance;
	const float radiusPixels = min(gAOParams.Radius * gAOParams.PixelsPerMeter / cameraDistance, gAOParams.MaxRadiusPixels);
	// 반경이 한 픽셀보다 작으면 찾을 이웃이 없다.
	if (radiusPixels < 1.0f)
	{
		gRawOcclusion[dispatchId.xy] = 1.0f;
		return;
	}

	// 반경 끝 38%에서 차폐 기여가 0으로 줄어든다.
	const float falloffRange = 0.615f * gAOParams.Radius;
	const float falloffMul = -1.0f / falloffRange;
	const float falloffAdd = (gAOParams.Radius - falloffRange) / falloffRange + 1.0f;

	const float sliceNoise = RandomAO(dispatchId.xy, gAOParams.FrameIndex, 0u);
	const float stepNoise = RandomAO(dispatchId.xy, gAOParams.FrameIndex, 1u);
	const float2 pixelCenter = float2(dispatchId.xy);
	const uint sliceCount = max(gAOParams.SliceCount, 1u);
	const uint stepCount = max(gAOParams.StepCount, 1u);

	float visibility = 0.0f;
	[loop]
	for (uint slice = 0u; slice < sliceCount; ++slice)
	{
		const float phi = (slice + sliceNoise) * AO_PI / sliceCount;
		const float2 omega = float2(cos(phi), sin(phi)); // 화면 픽셀 방향(y 아래)

		// 화면 방향을 월드로 옮기고 시선에 수직인 성분만 남겨 단면을 정한다.
		const float3 directionVec = gAOParams.ScreenRight * omega.x - gAOParams.ScreenUp * omega.y;
		const float3 orthoDirection = normalize(directionVec - dot(directionVec, viewVec) * viewVec);
		const float3 axis = normalize(cross(orthoDirection, viewVec));
		const float3 projectedNormal = normal - axis * dot(normal, axis);
		const float projectedLength = length(projectedNormal);
		if (projectedLength < 0.0001f)
		{
			visibility += 1.0f;
			continue;
		}

		const float signNormal = dot(orthoDirection, projectedNormal) >= 0.0f ? 1.0f : -1.0f;
		const float cosNormal = saturate(dot(projectedNormal, viewVec) / projectedLength);
		const float n = signNormal * FastAcos(cosNormal);
		// 0: +방향, 1: -방향. 지평선의 초기값은 법선 반구의 가장자리다.
		const float lowHorizon0 = cos(n + AO_PI * 0.5f);
		const float lowHorizon1 = cos(n - AO_PI * 0.5f);
		float horizon0 = lowHorizon0;
		float horizon1 = lowHorizon1;

		[loop]
		for (uint stepIndex = 0u; stepIndex < stepCount; ++stepIndex)
		{
			// 가까운 곳에 표본을 모은다. 자기 픽셀을 다시 읽지 않도록 최소 한 픽셀 이상 떨어뜨린다.
			float s = (stepIndex + stepNoise) / stepCount;
			s *= s;
			const float offsetLength = max(s * radiusPixels, stepIndex + 1.0f);
			const float2 offset = omega * offsetLength;

			[unroll]
			for (uint side = 0u; side < 2u; ++side)
			{
				const int2 samplePixel = int2(round(pixelCenter + (side == 0u ? offset : -offset)));
				if (samplePixel.x < 0 || samplePixel.y < 0 || samplePixel.x >= (int)width || samplePixel.y >= (int)height)
					continue;
				const int3 sampleCoord = int3(samplePixel, 0);
				if (!HasSurface(sampleCoord))
					continue;

				const float3 delta = gPosition.Load(sampleCoord).xyz - position;
				const float sampleDistance = length(delta);
				if (sampleDistance < 0.0001f)
					continue;
				const float weight = saturate(sampleDistance * falloffMul + falloffAdd);
				if (side == 0u)
					horizon0 = max(horizon0, lerp(lowHorizon0, dot(delta / sampleDistance, viewVec), weight));
				else
					horizon1 = max(horizon1, lerp(lowHorizon1, dot(delta / sampleDistance, viewVec), weight));
			}
		}

		// 지평선 각을 법선 반구로 자르고 코사인 가중 호를 적분한다.
		float h0 = -FastAcos(horizon1);
		float h1 = FastAcos(horizon0);
		h0 = n + clamp(h0 - n, -AO_PI * 0.5f, AO_PI * 0.5f);
		h1 = n + clamp(h1 - n, -AO_PI * 0.5f, AO_PI * 0.5f);
		const float sinN = sin(n);
		const float arc0 = (cosNormal + 2.0f * h0 * sinN - cos(2.0f * h0 - n)) * 0.25f;
		const float arc1 = (cosNormal + 2.0f * h1 * sinN - cos(2.0f * h1 - n)) * 0.25f;
		visibility += projectedLength * (arc0 + arc1);
	}

	visibility = saturate(visibility / sliceCount);
	gRawOcclusion[dispatchId.xy] = pow(visibility, max(gAOParams.Power, 0.01f));
}
