// 프로브 GI 아틀라스 섞기 (cs_6_5, DirectX12 전용). 포함하기 전에 PROBE_TEXELS와 PROBE_BLEND_DISTANCE를 정한다.
// 스레드 그룹 하나가 프로브 칸 하나를 맡는다. 안쪽 텍셀마다 광선 결과를 방향 가중치로 평균 내 이전 값과 섞고,
// 같은 그룹 안에서 테두리 텍셀을 채워 다른 Dispatch 없이 쌍선형 샘플이 이웃 칸으로 새지 않게 한다.
// - 조도(PROBE_BLEND_DISTANCE 0): rgb = 코사인 가중 평균 밝기. 뒷면에 닿은 광선은 뺀다.
// - 거리(PROBE_BLEND_DISTANCE 1): rg = 거리, 거리 제곱의 평균(체비쇼프 가시성 검사에 쓴다, 3단계).
#include "raytracing.hlsli"
#include "probeGI.hlsli"

RWTexture2D<float4> gAtlas : register(u2, space3);
ConstantBuffer<ProbeGIUpdateParams> gParams : register(b1, space3);

groupshared float4 gTexels[PROBE_TEXELS * PROBE_TEXELS];

float4 BlendTexel(uint row, uint2 local, float4 previous)
{
	const float3 texelDirection = ProbeTexelDirection(local, PROBE_TEXELS);
	Texture2D<float4> rayData = gBindlessTextures[gParams.RayDataTexture];
	float4 sum = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float totalWeight = 0.0f;
	for (uint rayIndex = 0u; rayIndex < gParams.RaysPerProbe; ++rayIndex)
	{
		const float weight = max(dot(texelDirection, ProbeRayDirection(rayIndex, gParams)), 0.0f);
		if (weight <= 0.0f)
			continue;
		// HLSL에서 sample은 보간 한정자라 이름으로 쓰지 않는다.
		const float4 rayResult = rayData.Load(int3(rayIndex, row, 0));
#if PROBE_BLEND_DISTANCE
		const float distanceWeight = pow(weight, gParams.DistanceExponent);
		const float rayDistance = min(abs(rayResult.a), gParams.MaxProbeDistance);
		sum.rg += float2(rayDistance, rayDistance * rayDistance) * distanceWeight;
		totalWeight += distanceWeight;
#else
		if (rayResult.a < 0.0f)
			continue;
		sum.rgb += rayResult.rgb * weight;
		totalWeight += weight;
#endif
	}
	// 쓸 수 있는 광선이 없으면(모두 뒷면) 이전 값을 그대로 둔다.
	if (totalWeight <= 1e-5f)
		return previous;

	float4 result = sum / totalWeight;
	result.a = 1.0f;
	const float hysteresis = row < gParams.FreshCount ? 0.0f : gParams.Hysteresis;
	return lerp(result, previous, hysteresis);
}

[numthreads(PROBE_TEXELS, PROBE_TEXELS, 1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
	// 비울 때는 행이 곧 프로브 번호다.
	const uint row = groupId.x;
	const uint probeIndex = gParams.ClearAll != 0u ? row : (gParams.FirstProbe + row) % gParams.ProbeCount;
	const uint2 origin = ProbeTileOrigin(probeIndex, gParams.GridCounts, PROBE_TEXELS);
	const uint2 local = groupThreadId.xy + 1u;

	float4 value = float4(0.0f, 0.0f, 0.0f, 0.0f);
	if (gParams.ClearAll == 0u)
		value = BlendTexel(row, local, gAtlas[origin + local]);
	gAtlas[origin + local] = value;
	gTexels[groupThreadId.y * PROBE_TEXELS + groupThreadId.x] = value;

	GroupMemoryBarrierWithGroupSync();

	if (groupIndex < 4u * PROBE_TEXELS + 4u)
	{
		const uint2 border = ProbeBorderTexel(groupIndex, PROBE_TEXELS);
		const uint2 source = ProbeBorderSource(border, PROBE_TEXELS) - 1u;
		gAtlas[origin + border] = gTexels[source.y * PROBE_TEXELS + source.x];
	}
}
