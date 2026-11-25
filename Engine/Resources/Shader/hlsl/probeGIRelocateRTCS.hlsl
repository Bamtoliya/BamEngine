// 프로브 GI 벽 속 프로브 옮기기와 끄기 (cs_6_5, DirectX12 전용, 4단계)
// 이번 프레임에 추적한 프로브마다 광선 결과를 요약해 상태 텍스처(rgb 이동량, a 사용 여부)를 고친다.
// - 뒷면에 닿은 광선이 BACKFACE_RATIO_THRESHOLD보다 많으면 벽 속으로 보고, 가장 가까운 뒷면을 지나 밖으로 옮기며 이번에는 끈다.
// - 아니면 켜고, 앞면에 너무 붙어 있으면 반대쪽으로 뗀다.
// - 이동량은 축마다 간격의 MAX_PROBE_OFFSET_RATIO까지다.
// 규칙은 ProbeGrid::RelocateProbe와 같다. 바뀐 위치는 다음 갱신의 광선부터 쓴다.
#include "raytracing.hlsli"
#include "probeGI.hlsli"

static const float BACKFACE_RATIO_THRESHOLD = 0.25f;
static const float BACKFACE_REACTIVATE_THRESHOLD = 0.15f;
static const float MIN_FRONTFACE_DISTANCE_RATIO = 0.2f;
static const float FRONTFACE_PUSH_DAMPING = 0.5f;
static const float MAX_PROBE_OFFSET_RATIO = 0.45f;
// 추적 패스는 뒷면 거리를 -0.2배로 줄여 기록한다.
static const float BACKFACE_DISTANCE_SCALE = 5.0f;

RWTexture2D<float4> gProbeData : register(u2, space3);
ConstantBuffer<ProbeGIUpdateParams> gParams : register(b1, space3);

[numthreads(64, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	// 비울 때는 스레드 번호가 곧 프로브 번호다. 격자 위치에서 켠 상태로 시작한다.
	if (gParams.ClearAll != 0u)
	{
		if (dispatchId.x < gParams.ProbeCount)
			gProbeData[ProbeDataTexel(dispatchId.x, gParams.GridCounts)] = float4(0.0f, 0.0f, 0.0f, 1.0f);
		return;
	}

	const uint row = dispatchId.x;
	if (row >= gParams.UpdateCount)
		return;
	const uint probeIndex = (gParams.FirstProbe + row) % gParams.ProbeCount;
	const uint2 texel = ProbeDataTexel(probeIndex, gParams.GridCounts);

	Texture2D<float4> rayData = gBindlessTextures[gParams.RayDataTexture];
	uint backfaceCount = 0u;
	float closestBackface = 1e27f;
	float3 backfaceDirection = float3(0.0f, 0.0f, 0.0f);
	float closestFrontface = 1e27f;
	float3 frontfaceDirection = float3(0.0f, 0.0f, 0.0f);
	for (uint rayIndex = 0u; rayIndex < gParams.RaysPerProbe; ++rayIndex)
	{
		const float hitDistance = rayData.Load(int3(rayIndex, row, 0)).a;
		if (hitDistance < 0.0f)
		{
			++backfaceCount;
			const float backfaceDistance = -hitDistance * BACKFACE_DISTANCE_SCALE;
			if (backfaceDistance < closestBackface)
			{
				closestBackface = backfaceDistance;
				backfaceDirection = ProbeRayDirection(rayIndex, gParams);
			}
		}
		else if (hitDistance < closestFrontface)
		{
			closestFrontface = hitDistance;
			frontfaceDirection = ProbeRayDirection(rayIndex, gParams);
		}
	}

	const float minFrontface = min(gParams.GridSpacing.x, min(gParams.GridSpacing.y, gParams.GridSpacing.z)) * MIN_FRONTFACE_DISTANCE_RATIO;
	const float4 currentState = gProbeData[texel];
	const bool prevActive = currentState.a > 0.0f;
	const float backfaceRatio = (float)backfaceCount / max((float)gParams.RaysPerProbe, 1.0f);
	const bool inside = backfaceRatio > (prevActive ? BACKFACE_RATIO_THRESHOLD : BACKFACE_REACTIVATE_THRESHOLD);
	float3 offset = currentState.xyz;
	if (inside)
		offset += backfaceDirection * (closestBackface + minFrontface * 0.5f);
	else if (closestFrontface < minFrontface)
		offset -= frontfaceDirection * ((minFrontface - closestFrontface) * FRONTFACE_PUSH_DAMPING);
	const float3 limit = gParams.GridSpacing * MAX_PROBE_OFFSET_RATIO;
	gProbeData[texel] = float4(clamp(offset, -limit, limit), inside ? 0.0f : 1.0f);
}
