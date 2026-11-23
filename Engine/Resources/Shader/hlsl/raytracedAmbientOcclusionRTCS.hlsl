// 레이트레이싱 앰비언트 오클루전 (DXR 인라인 RayQuery, cs_6_5, DirectX12 전용)
// 법선 반구에 코사인 분포로 짧은 광선을 쏘아 반경 안에서 막히지 않은 비율을 기록한다.
// 차폐 대상은 TLAS의 그림자 캐스터(정적 불투명 메시)뿐이다. 스킨드·동적 메시와 지형은 가리지 못한다.
// 표본이 적어 노이즈가 크며 ambientOcclusionTemporalCS가 누적한다.
#include "ambientOcclusion.hlsli"

// RaytracingInstanceData.h와 같다.
static const uint RAYTRACING_MASK_SHADOW = 1u;

RaytracingAccelerationStructure gSceneTLAS : register(t13, space3);
RWTexture2D<float> gRawOcclusion : register(u2, space3);

float3 CosineSampleHemisphere(float3 normal, float u1, float u2)
{
	const float radius = sqrt(u1);
	const float phi = 2.0f * AO_PI * u2;
	const float3 tangentSample = float3(radius * cos(phi), radius * sin(phi), sqrt(max(1.0f - u1, 0.0f)));
	const float3 helper = abs(normal.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
	const float3 tangent = normalize(cross(helper, normal));
	const float3 bitangent = cross(normal, tangent);
	return normalize(tangent * tangentSample.x + bitangent * tangentSample.y + normal * tangentSample.z);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint width, height;
	gRawOcclusion.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	float3 normal, position;
	if (!ReceivesAmbientOcclusion(int3(dispatchId.xy, 0), normal, position))
	{
		gRawOcclusion[dispatchId.xy] = 1.0f;
		return;
	}

	const float cameraDistance = distance(position, gAOParams.CameraPosition);
	const float3 origin = position + normal * (gAOParams.NormalBias + cameraDistance * gAOParams.DistanceBiasScale);
	const uint rayCount = max(gAOParams.RayCount, 1u);

	float visible = 0.0f;
	[loop]
	for (uint rayIndex = 0u; rayIndex < rayCount; ++rayIndex)
	{
		RayDesc ray;
		ray.Origin = origin;
		ray.Direction = CosineSampleHemisphere(normal,
			RandomAO(dispatchId.xy, gAOParams.FrameIndex, rayIndex * 2u), RandomAO(dispatchId.xy, gAOParams.FrameIndex, rayIndex * 2u + 1u));
		ray.TMin = 0.0f;
		ray.TMax = gAOParams.Radius;

		// 막혔는지만 필요하므로 첫 교차에서 끝낸다.
		RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> query;
		query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, RAYTRACING_MASK_SHADOW, ray);
		query.Proceed();
		if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
			visible += 1.0f;
	}

	gRawOcclusion[dispatchId.xy] = visible / rayCount;
}
