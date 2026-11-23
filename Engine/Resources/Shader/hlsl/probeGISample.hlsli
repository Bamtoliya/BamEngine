// 조명 패스에서 프로브 GI 조도를 읽는다 (DDGI 방식, DirectX12 전용).
// 둘러싼 8개 프로브의 조도를 3선형 가중치에 다음 두 가중치를 곱해 섞는다.
// - 법선이 프로브 쪽을 향하는 정도: 면 뒤쪽 프로브의 영향을 줄인다.
// - 체비쇼프 가시성: 거리 아틀라스의 평균·제곱 평균보다 멀리 있는 점은 프로브에서 가려졌다고 보고 줄인다(빛샘 방지).
// 아직 한 번도 갱신하지 않은 프로브(조도 a = 0)는 쓰지 않는다.
#ifndef BAM_PROBE_GI_SAMPLE_HLSLI
#define BAM_PROBE_GI_SAMPLE_HLSLI

#include "probeGI.hlsli"

// DeferredSceneRenderer.cpp의 ProbeGILightingData. LightingPassData 끝에 붙는다(64바이트).
struct ProbeGILightingData
{
	float3 GridOrigin;
	uint Enabled;          // 1이면 이번 프레임 아틀라스를 읽는다
	float3 GridSpacing;
	float Intensity;
	uint3 GridCounts;
	float BlendDistance;   // 상자 밖으로 이 거리(m)만큼 기존 환경광과 섞는다
	float NormalBias;      // 표본 위치를 법선 방향으로 띄우는 거리(m)
	float ViewBias;        // 표본 위치를 카메라 쪽으로 띄우는 거리(m)
	uint DebugView;        // 1이면 간접 확산광만 출력한다
	uint _Pad;
};

// 상자 안은 1, 상자 밖 blendDistance(m)에서 0. ProbeGrid::VolumeBlendWeight와 같다.
float ProbeVolumeWeight(ProbeGILightingData gi, float3 position)
{
	const float3 extent = gi.GridSpacing * (float3)(gi.GridCounts - 1u);
	const float3 outside = max(gi.GridOrigin - position, position - (gi.GridOrigin + extent));
	const float farthest = max(outside.x, max(outside.y, outside.z));
	if (farthest <= 0.0f)
		return 1.0f;
	if (gi.BlendDistance <= 0.0f)
		return 0.0f;
	return saturate(1.0f - farthest / gi.BlendDistance);
}

// 프로브 칸 안에서 direction 방향을 읽을 아틀라스 UV
float2 ProbeAtlasUV(uint probeIndex, uint3 counts, uint texels, float3 direction)
{
	const float2 atlasSize = float2(counts.x * counts.y, counts.z) * (float)(texels + 2u);
	const float2 texel = (float2)ProbeTileOrigin(probeIndex, counts, texels) + 1.0f + (EncodeOctahedral(direction) * 0.5f + 0.5f) * (float)texels;
	return texel / atlasSize;
}

// position의 면이 normal 방향 반구에서 받는 간접 확산광(코사인 가중 평균 밝기). viewDirection은 면에서 카메라를 향한다.
float3 SampleProbeIrradiance(ProbeGILightingData gi, float3 position, float3 normal, float3 viewDirection,
	Texture2D<float4> irradianceAtlas, Texture2D<float4> distanceAtlas, SamplerState atlasSampler)
{
	const float3 biasedPosition = position + normal * gi.NormalBias + viewDirection * gi.ViewBias;
	const float3 local = (biasedPosition - gi.GridOrigin) / gi.GridSpacing;
	const float3 baseCell = clamp(floor(local), 0.0f, (float3)(gi.GridCounts - 2u));
	const float3 alpha = saturate(local - baseCell);
	const uint3 base = (uint3)baseCell;

	float3 sum = float3(0.0f, 0.0f, 0.0f);
	float totalWeight = 0.0f;
	for (uint corner = 0u; corner < 8u; ++corner)
	{
		const uint3 offset = uint3(corner & 1u, (corner >> 1u) & 1u, (corner >> 2u) & 1u);
		const uint3 coord = min(base + offset, gi.GridCounts - 1u);
		const uint probeIndex = coord.x + coord.y * gi.GridCounts.x + coord.z * gi.GridCounts.x * gi.GridCounts.y;
		const float3 probePosition = gi.GridOrigin + gi.GridSpacing * (float3)coord;

		// 면 뒤쪽 프로브를 줄인다. 완전히 없애면 얇은 물체에서 끊기므로 0.2를 남긴다.
		const float3 toProbe = normalize(probePosition - position);
		const float wrap = (dot(toProbe, normal) + 1.0f) * 0.5f;
		float weight = wrap * wrap + 0.2f;

		// 체비쇼프 가시성
		const float3 probeToPoint = biasedPosition - probePosition;
		const float pointDistance = length(probeToPoint);
		const float2 moments = distanceAtlas.SampleLevel(atlasSampler,
			ProbeAtlasUV(probeIndex, gi.GridCounts, PROBE_DISTANCE_TEXELS, probeToPoint / max(pointDistance, 1e-4f)), 0.0f).rg;
		if (pointDistance > moments.x)
		{
			const float variance = abs(moments.y - moments.x * moments.x);
			const float difference = pointDistance - moments.x;
			const float chebyshev = variance / max(variance + difference * difference, 1e-6f);
			weight *= max(chebyshev * chebyshev * chebyshev, 0.05f);
		}

		// 아주 작은 가중치는 더 눌러 가려진 프로브가 조금씩 새는 것을 줄인다.
		weight = max(weight, 1e-6f);
		const float crushThreshold = 0.2f;
		if (weight < crushThreshold)
			weight *= weight * weight / (crushThreshold * crushThreshold);

		const float3 trilinear = lerp(1.0f - alpha, alpha, (float3)offset);
		const float4 irradiance = irradianceAtlas.SampleLevel(atlasSampler, ProbeAtlasUV(probeIndex, gi.GridCounts, PROBE_IRRADIANCE_TEXELS, normal), 0.0f);
		// a는 한 번 이상 갱신한 프로브만 1이다.
		weight *= trilinear.x * trilinear.y * trilinear.z * saturate(irradiance.a);
		sum += irradiance.rgb * weight;
		totalWeight += weight;
	}
	return totalWeight > 1e-6f ? sum / totalWeight : float3(0.0f, 0.0f, 0.0f);
}

#endif
