// 프로브 GI 광선 추적 (DXR 인라인 RayQuery, cs_6_5, DirectX12 전용)
// 이번 프레임에 갱신할 프로브마다 광선을 RaysPerProbe개 쏜다. 결과 텍스처의 열은 광선, 행은 갱신 순서다.
// rgb는 닿은 면이 광선 쪽으로 내보내는 밝기, a는 닿은 거리다. 뒷면에 닿으면 밝기 0과 음수 거리(-0.2 × 거리)를 써서
// 벽 속 프로브를 구분할 수 있게 한다. 닿지 않으면 환경광과 RayMaxDistance를 쓴다.
// 닿은 면은 확산 직접광(그림자를 드리우는 광원마다 그림자 광선 하나), 환경광, 발광을 계산한다.
// 닿은 면이 볼륨 안이면 환경광 대신 이전 프레임의 프로브 조도를 써서 반사가 여러 번 이어지게 한다(다중 반사).
// 광선은 벽 속에서 옮긴 프로브 위치(상태 텍스처의 rgb)에서 쏜다.
#include "common.hlsli"
#include "light.hlsli"
#include "raytracing.hlsli"
#include "raytracingSurface.hlsli"
#include "probeGISample.hlsli"

static const float PI = 3.14159265359f;
// 강한 빛 하나가 이력에 오래 남지 않게 자른다. 측정 전 시작값이다.
static const float MAX_PROBE_RADIANCE = 64.0f;
// 환경 맵은 밝은 점(해)이 광선 몇 개에만 잡혀 깜빡이지 않도록 조금 거른 밉을 읽는다.
static const float ENVIRONMENT_MIP = 2.0f;

TextureCube gIrradianceMap : register(t10, space3);
TextureCube gPrefilteredEnvMap : register(t11, space3);
RaytracingAccelerationStructure gSceneTLAS : register(t13, space3);
RWTexture2D<float4> gRayData : register(u2, space3);
ConstantBuffer<ProbeGIUpdateParams> gParams : register(b1, space3);

// 후보로 돌아온 삼각형이 알파 검사를 통과하는지. 알파 검사가 필요한 인스턴스만 후보가 된다.
bool AcceptCandidate(uint instanceIndex, uint primitiveIndex, float2 barycentrics)
{
	return PassesAlphaTest(LoadRaytracedInstance(gParams.InstanceTable, instanceIndex), primitiveIndex, barycentrics);
}

#define RESOLVE_CANDIDATES(query) \
	while (query.Proceed()) \
	{ \
		if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE && \
			AcceptCandidate(query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics())) \
			query.CommitNonOpaqueTriangleHit(); \
	}

float TraceShadow(float3 origin, float3 direction, float maxDistance)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.0f;
	ray.TMax = maxDistance;
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, RAYTRACING_MASK_REFLECTION_SHADOW, ray);
	RESOLVE_CANDIDATES(query)
	return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0f : 1.0f;
}

// direction 쪽에서 들어오는 하늘·환경의 밝기. 해는 닿은 면의 직접광으로 따로 더하므로 넣지 않는다.
float3 EnvironmentRadiance(float3 direction)
{
	if (gParams.HasIBLMaps != 0u)
		return gPrefilteredEnvMap.SampleLevel(gBindlessSampler, direction, ENVIRONMENT_MIP).rgb * gParams.IBLIntensity;

	float3 sky = float3(0.0f, 0.0f, 0.0f);
	bool hasSkyLight = false;
	const uint lightCount = min(gLightBuffer.Load(0u), gParams.MaxHitLights);
	for (uint lightIndex = 0u; lightIndex < lightCount; ++lightIndex)
	{
		const Light light = LoadLight(lightIndex);
		if (GetLightType(light.PackedFlags) != LIGHT_TYPE_SKY || !HasLightFlag(light.PackedFlags, LIGHT_FLAG_USE_IN_DEFERRED) ||
			!HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_DIFFUSE))
			continue;
		hasSkyLight = true;
		sky += EvaluateSkyLight(light, direction);
	}
	if (!hasSkyLight)
		sky = lerp(gParams.SkyZenith * 0.25f, gParams.SkyZenith, saturate(direction.y * 0.5f + 0.5f));
	return sky * gParams.IBLIntensity;
}

// 닿은 면이 광선을 거슬러 내보내는 확산 밝기
float3 ShadeProbeHit(HitSurface surface, float3 rayDirection)
{
	const float3 diffuseColor = surface.Albedo * (1.0f - surface.Metallic);
	const float3 shadowOrigin = surface.Position + surface.SurfaceNormal * gParams.NormalBias;

	float3 direct = float3(0.0f, 0.0f, 0.0f);
	float3 skyIrradiance = float3(0.0f, 0.0f, 0.0f);
	bool hasSkyLight = false;
	const uint lightCount = min(gLightBuffer.Load(0u), gParams.MaxHitLights);
	for (uint lightIndex = 0u; lightIndex < lightCount; ++lightIndex)
	{
		const Light light = LoadLight(lightIndex);
		if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_USE_IN_DEFERRED) || !HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_DIFFUSE))
			continue;

		const uint lightType = GetLightType(light.PackedFlags);
		if (lightType == LIGHT_TYPE_SKY)
		{
			hasSkyLight = true;
			skyIrradiance += EvaluateSkyLight(light, surface.Normal);
			continue;
		}

		float3 toLight;
		float attenuation;
		if (!EvaluateLightIncidence(light, lightType, surface.Position, toLight, attenuation))
			continue;
		const float normalDotLight = dot(surface.Normal, toLight);
		if (normalDotLight <= 0.0f || attenuation <= 0.0f)
			continue;

		// 그림자를 드리우는 광원은 모두 확인한다. 확인하지 않으면 벽 너머의 빛이 프로브로 샌다.
		float visibility = 1.0f;
		if (HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
		{
			const float maxDistance = lightType == LIGHT_TYPE_DIRECTIONAL ? gParams.RayMaxDistance : length(light.Position - shadowOrigin);
			visibility = lerp(1.0f - light.ShadowStrength, 1.0f, TraceShadow(shadowOrigin, toLight, maxDistance));
		}
		direct += diffuseColor / PI * light.Color * light.Intensity * attenuation * normalDotLight * visibility;
	}

	// 볼륨 밖이나 아직 쓸 프로브가 없으면 반사와 같은 기존 환경광을 쓴다.
	float3 irradiance;
	if (gParams.HasIBLMaps != 0u)
		irradiance = gIrradianceMap.SampleLevel(gBindlessSampler, surface.Normal, 0.0f).rgb;
	else if (hasSkyLight)
		irradiance = skyIrradiance;
	else
		irradiance = lerp(gParams.SkyZenith * 0.25f, gParams.SkyZenith, saturate(surface.Normal.y * 0.5f + 0.5f));
	irradiance *= gParams.IBLIntensity;

	// 다중 반사: 이전 프레임 아틀라스에서 닿은 면의 조도를 읽는다. 프로브 조도에는 하늘빛이 이미 들어 있다.
	if (gParams.IrradianceTexture != INVALID_BINDLESS_INDEX)
	{
		ProbeGILightingData gi;
		gi.GridOrigin = gParams.GridOrigin;
		gi.Enabled = 1u;
		gi.GridSpacing = gParams.GridSpacing;
		gi.Intensity = 1.0f;
		gi.GridCounts = gParams.GridCounts;
		gi.BlendDistance = gParams.BlendDistance;
		gi.NormalBias = gParams.SampleNormalBias;
		gi.ViewBias = gParams.SampleViewBias;
		gi.DebugView = 0u;
		gi._Pad = 0u;
		const float volumeWeight = ProbeVolumeWeight(gi, surface.Position);
		if (volumeWeight > 0.0f)
		{
			bool valid;
			const float3 probeIrradiance = SampleProbeIrradiance(gi, surface.Position, surface.Normal, -rayDirection,
				gBindlessTextures[gParams.IrradianceTexture], gBindlessTextures[gParams.DistanceTexture], gBindlessTextures[gParams.ProbeDataTexture],
				gBindlessSampler, valid);
			if (valid)
				irradiance = lerp(irradiance, probeIrradiance, volumeWeight);
		}
	}
	return direct + diffuseColor * irradiance * surface.AmbientOcclusion + surface.Emission;
}

[numthreads(32, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	const uint rayIndex = dispatchId.x;
	const uint row = dispatchId.y;
	if (rayIndex >= gParams.RaysPerProbe || row >= gParams.UpdateCount)
		return;

	const uint probeIndex = (gParams.FirstProbe + row) % gParams.ProbeCount;
	const float3 direction = ProbeRayDirection(rayIndex, gParams);

	RayDesc ray;
	// 벽 속에서 옮긴 이동량을 더한다(relocate 패스가 쓰며, 이번 프레임 값은 다음 갱신부터 반영된다).
	const float3 probeOffset = gBindlessTextures[gParams.ProbeDataTexture].Load(int3(ProbeDataTexel(probeIndex, gParams.GridCounts), 0)).xyz;
	ray.Origin = ProbeWorldPosition(probeIndex, gParams) + probeOffset;
	ray.Direction = direction;
	ray.TMin = 0.0f;
	ray.TMax = gParams.RayMaxDistance;
	RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, RAYTRACING_MASK_REFLECTION, ray);
	RESOLVE_CANDIDATES(query)

	float4 result;
	if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
	{
		result = float4(EnvironmentRadiance(direction), min(gParams.RayMaxDistance, 65000.0f));
	}
	else
	{
		const float hitDistance = query.CommittedRayT();
		const RaytracedInstance instance = LoadRaytracedInstance(gParams.InstanceTable, query.CommittedInstanceID());
		// 광선 하나가 맡는 입체각(4π / 광선 수)이 닿은 곳에서 덮는 너비로 밉을 고른다.
		const float footprint = hitDistance * sqrt(4.0f * PI / (float)gParams.RaysPerProbe);
		const HitSurface surface = LoadHitSurface(instance, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics(),
			query.CommittedObjectToWorld3x4(), query.CommittedWorldToObject3x4(), ray.Origin, direction, hitDistance, footprint);
		if (surface.BackFace)
		{
			result = float4(0.0f, 0.0f, 0.0f, -0.2f * hitDistance);
		}
		else
		{
			const float3 radiance = ShadeProbeHit(surface, direction);
			const bool invalid = any(isnan(radiance)) || any(isinf(radiance));
			const float brightest = max(radiance.r, max(radiance.g, radiance.b));
			result = float4(invalid ? float3(0.0f, 0.0f, 0.0f) : radiance * min(1.0f, MAX_PROBE_RADIANCE / max(brightest, 1e-4f)), hitDistance);
		}
	}
	gRayData[uint2(rayIndex, row)] = result;
}
