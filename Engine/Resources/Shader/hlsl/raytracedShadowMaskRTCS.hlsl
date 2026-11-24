// 레이트레이싱 그림자 마스크 (DXR 인라인 RayQuery, cs_6_5, DirectX12 전용)
// G-Buffer의 월드 위치에서 광원마다 광선을 하나 쏘아 가시성(1 = 빛, 0 = 차폐)을 채널에 기록한다.
// 광원 크기(SourceSize)가 있으면 광원 원판 안의 한 점을 프레임마다 다르게 골라 쏘고, 가린 물체까지의 거리로 반그림자 반지름을
// 함께 기록한다. 그 결과는 raytracedShadowTemporalRTCS·raytracedShadowFilterRTCS가 누적하고 흐린다.
// 채널별 광원은 CPU(AssignRaytracedShadows)가 정하고 lightingPS가 같은 인덱스로 읽는다.
// 차폐 대상은 RaytracingSceneManager가 TLAS에 넣은 정적 불투명 캐스터뿐이다. 나머지 캐스터는 섀도맵을 가진 광원만 담당하고
// lightingPS가 min(마스크, 섀도맵)으로 합성한다.
#include "common.hlsli"
#include "light.hlsli"
#include "raytracing.hlsli"
#include "raytracedShadow.hlsli"
#include "reflection.hlsli"

// RaytracingInstanceData.h와 같다.
static const uint RAYTRACING_MASK_SHADOW = 1u;
// 점·스포트 섀도맵의 Near와 같다. 광원을 감싼 전구 메시에 막히지 않게 한다.
static const float LOCAL_LIGHT_NEAR = 0.05f;

struct RaytracedShadowParams
{
	float NormalBias;          // 표면 법선 방향 시작점 오프셋(m)
	float DistanceBiasScale;   // 카메라 거리 1m당 추가 오프셋(m) — 원거리 위치 정밀도 보정
	float RayMaxDistance;      // 방향광 광선의 최대 길이(m)
	uint  FrameIndex;          // 광원 원판의 표본 위치를 프레임마다 바꾼다
	uint4 Lights;              // 채널별 광원 버퍼 인덱스
	uint  WritePenumbra;       // 1이면 u3에 반그림자 반지름을 쓴다(누적·필터를 쓰는 프레임)
	uint  InstanceTable;       // Masked 알파 테스트용 RaytracedInstance 표 번호
	uint2 Pad;
};

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPosition : register(t4, space3);
RaytracingAccelerationStructure gSceneTLAS : register(t13, space3);
RWTexture2D<float4> gShadowMask : register(u2, space3);
RWTexture2D<float4> gPenumbra : register(u3, space3);
ConstantBuffer<RaytracedShadowParams> gShadowParams : register(b1, space3);

struct ShadowSample
{
	float Visibility;
	float Penumbra;   // 받는 면에서 반그림자 반지름(m). 가려지지 않았거나 광원 크기가 0이면 0
};

// 방향 n에 수직인 두 축 (Duff et al. 2017)
void BuildBasis(float3 n, out float3 t, out float3 b)
{
	const float s = n.z >= 0.0f ? 1.0f : -1.0f;
	const float a = -1.0f / (s + n.z);
	const float c = n.x * n.y * a;
	t = float3(1.0f + s * n.x * n.x * a, s * c, -s * n.x);
	b = float3(c, s + n.y * n.y * a, -n.y);
}

// 단위 원판 안의 점. 픽셀마다 다른 값에서 시작해 R2 수열로 프레임마다 옮기므로 몇 프레임 안에 원판을 고르게 덮는다.
float2 SampleDisk(uint2 pixel, uint channel)
{
	const float2 start = HashPixel(pixel + uint2(channel * 7919u, channel * 104729u));
	const float2 u = frac(start + (float)(gShadowParams.FrameIndex % 1024u) * float2(0.7548777f, 0.5698403f));
	const float radius = sqrt(u.x);
	const float angle = u.y * 6.2831853f;
	return radius * float2(cos(angle), sin(angle));
}

bool AcceptShadowCandidate(uint instanceIndex, uint primitiveIndex, float2 barycentrics)
{
	if (gShadowParams.InstanceTable == INVALID_BINDLESS_INDEX)
		return true;
	return PassesAlphaTest(LoadRaytracedInstance(gShadowParams.InstanceTable, instanceIndex), primitiveIndex, barycentrics);
}

#define RESOLVE_SHADOW_CANDIDATES(query) \
	while (query.Proceed()) \
	{ \
		if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE && \
			AcceptShadowCandidate(query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics())) \
			query.CommitNonOpaqueTriangleHit(); \
	}

// 조명 결과가 0인 픽셀(빛을 등진 면, 범위·원뿔 밖)은 광선을 쏘지 않는다.
ShadowSample TraceVisibility(Light light, float3 origin, float3 normal, float2 disk)
{
	ShadowSample result;
	result.Visibility = 1.0f;
	result.Penumbra = 0.0f;

	const bool directional = GetLightType(light.PackedFlags) == LIGHT_TYPE_DIRECTIONAL;
	const float sourceSize = max(light.SourceSize, 0.0f);
	float3 toLight;
	float lightDistance = 0.0f;
	if (directional)
	{
		toLight = normalize(-light.Direction);
	}
	else
	{
		const float3 delta = light.Position - origin;
		lightDistance = length(delta);
		if (lightDistance <= LOCAL_LIGHT_NEAR || lightDistance > light.Range)
			return result;

		toLight = delta / lightDistance;
		if (GetLightType(light.PackedFlags) == LIGHT_TYPE_SPOT && dot(-toLight, normalize(light.Direction)) < light.SpotOuterCos)
			return result;
	}

	if (dot(normal, toLight) <= 0.0f)
	{
		result.Visibility = 0.0f;
		return result;
	}

	float3 direction = toLight;
	float maxDistance = gShadowParams.RayMaxDistance;
	if (directional)
	{
		if (sourceSize > 0.0f)
		{
			float3 t, b;
			BuildBasis(toLight, t, b);
			direction = normalize(toLight + (t * disk.x + b * disk.y) * sourceSize);
		}
	}
	else
	{
		float3 target = light.Position;
		if (sourceSize > 0.0f)
		{
			float3 t, b;
			BuildBasis(toLight, t, b);
			target += (t * disk.x + b * disk.y) * sourceSize;
		}
		const float3 delta = target - origin;
		const float targetDistance = length(delta);
		direction = delta / max(targetDistance, 1e-4f);
		// 광원 구 안쪽(전구 메시)에서 멈춘다.
		maxDistance = targetDistance - max(LOCAL_LIGHT_NEAR, sourceSize);
		if (maxDistance <= 0.0f)
			return result;
	}

	// 광원 원판의 일부가 면 아래에 있으면 그 표본은 면 자신이 가린다.
	if (dot(normal, direction) <= 0.0f)
	{
		result.Visibility = 0.0f;
		return result;
	}

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.0f;
	ray.TMax = maxDistance;

	if (sourceSize > 0.0f)
	{
		// 반그림자 반지름을 구하려면 받는 면에서 가장 가까운 물체까지의 거리가 필요하다.
		RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
		query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, RAYTRACING_MASK_SHADOW, ray);
		RESOLVE_SHADOW_CANDIDATES(query)
		if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		{
			result.Visibility = 0.0f;
			result.Penumbra = GetPenumbraRadius(directional, sourceSize, query.CommittedRayT(), lightDistance);
		}
		return result;
	}

	// 차폐 여부만 필요하므로 첫 교차에서 탐색을 끝낸다.
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, RAYTRACING_MASK_SHADOW, ray);
	RESOLVE_SHADOW_CANDIDATES(query)
	result.Visibility = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0f : 1.0f;
	return result;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint width, height;
	gShadowMask.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	const int3 pixel = int3(dispatchId.xy, 0);
	float4 visibility = float4(1.0f, 1.0f, 1.0f, 1.0f);
	float4 penumbra = float4(0.0f, 0.0f, 0.0f, 0.0f);

	const float4 normalSample = gNormal.Load(pixel);
	if (IsShadowReceiver(gDiffuse.Load(pixel), normalSample))
	{
		const float3 normal = normalize(normalSample.rgb * 2.0f - 1.0f);
		const float3 worldPosition = gPosition.Load(pixel).xyz;
		const float cameraDistance = distance(worldPosition, commonData.CameraPos);
		const float3 origin = worldPosition + normal * (gShadowParams.NormalBias + cameraDistance * gShadowParams.DistanceBiasScale);

		for (uint channel = 0u; channel < MAX_RAYTRACED_SHADOW_LIGHTS; ++channel)
		{
			const uint lightIndex = gShadowParams.Lights[channel];
			if (lightIndex == INVALID_LIGHT)
				continue;
			const ShadowSample shadowSample = TraceVisibility(LoadLight(lightIndex), origin, normal, SampleDisk(dispatchId.xy, channel));
			visibility[channel] = shadowSample.Visibility;
			penumbra[channel] = shadowSample.Penumbra;
		}
	}

	gShadowMask[dispatchId.xy] = visibility;
	if (gShadowParams.WritePenumbra != 0u)
		gPenumbra[dispatchId.xy] = penumbra;
}
