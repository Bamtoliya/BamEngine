// 레이트레이싱 반사 (DXR 인라인 RayQuery, cs_6_5, DirectX12 전용)
// 픽셀마다 광선을 하나 쏜다. 매끈한 면은 거울 방향으로, 거친 면은 GGX 분포에서 뽑은 방향으로 쏘고 이후 패스가 누적·필터한다.
// rgb는 닿은 면의 밝기에 합성 비율을 곱한 값, a는 합성 비율이다. 너무 거칠거나 닿은 면이 없으면 비율이 0이 되어 환경광 반사를 쓴다.
// 닿은 면은 법선 맵을 적용해 확산·정반사 직접광(GGX), 그림자 광선 하나(대표 방향광), 환경광, 발광을 계산한다.
#include "common.hlsli"
#include "light.hlsli"
#include "raytracing.hlsli"
#include "raytracingSurface.hlsli"
#include "reflection.hlsli"

static const float PI = 3.14159265359f;
static const uint INVALID_LIGHT = 0xFFFFFFFFu;
// 강한 빛 하나가 누적 결과를 오래 흐리지 않게 자른다.
static const float MAX_REFLECTION_RADIANCE = 64.0f;

struct RaytracedReflectionParams
{
	float NormalBias;          // 표면 법선 방향 시작점 오프셋(m)
	float DistanceBiasScale;   // 카메라 거리 1m당 추가 오프셋(m)
	float RayMaxDistance;
	float RoughnessFadeStart;  // 이보다 거칠면 반사를 줄이기 시작한다
	float RoughnessFadeEnd;    // 이보다 거칠면 추적하지 않는다
	uint InstanceTable;        // RaytracedInstance 표의 번호
	uint SunLight;             // 그림자 광선을 쏘는 방향광의 광원 버퍼 인덱스
	float PixelSpread;         // 거리 1m에서 픽셀 하나의 너비(m)
	float3 SkyZenith;
	float IBLIntensity;
	uint HasIBLMaps;
	uint MaxHitLights;         // 닿은 면에서 확인할 광원 수
	uint FrameIndex;
	float MirrorRoughness;     // 이보다 매끈하면 거울 방향만 쏜다
};

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPBR : register(t2, space3);
Texture2D gPosition : register(t4, space3);
TextureCube gIrradianceMap : register(t10, space3);
RaytracingAccelerationStructure gSceneTLAS : register(t13, space3);
RWTexture2D<float4> gReflection : register(u2, space3);
// 닿은 곳까지의 거리. 닿지 않았으면 REFLECTION_MISS_DISTANCE, 추적하지 않은 픽셀은 REFLECTION_NOT_TRACED
RWTexture2D<float> gHitDistance : register(u3, space3);
ConstantBuffer<RaytracedReflectionParams> gParams : register(b1, space3);

// 광선이 닿은 곳. RayQuery는 함수 인자로 넘기지 않고 필요한 값만 꺼내 쓴다.
struct ReflectionHit
{
	uint InstanceIndex;
	uint PrimitiveIndex;
	float2 Barycentrics;
	float Distance;
	float3x4 ObjectToWorld;
	float3x4 WorldToObject;
};

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

float TraceShadow(float3 origin, float3 direction)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.0f;
	ray.TMax = gParams.RayMaxDistance;
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, RAYTRACING_MASK_REFLECTION_SHADOW, ray);
	RESOLVE_CANDIDATES(query)
	return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0f : 1.0f;
}

// 아래 네 함수는 lightingPS.hlsl과 같은 식이다. 한쪽을 바꾸면 다른 쪽도 맞춘다.
float DistributionGGX(float3 normal, float3 halfVector, float roughness)
{
	const float a2 = roughness * roughness * roughness * roughness;
	const float normalDotHalf = max(dot(normal, halfVector), 0.0f);
	const float denominator = normalDotHalf * normalDotHalf * (a2 - 1.0f) + 1.0f;
	return a2 / (PI * denominator * denominator);
}

float GeometrySmith(float normalDotView, float normalDotLight, float roughness)
{
	const float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
	return normalDotView / (normalDotView * (1.0f - k) + k) * normalDotLight / (normalDotLight * (1.0f - k) + k);
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
	return f0 + (1.0f - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 EnvBRDFApprox(float3 f0, float roughness, float normalDotView)
{
	const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
	const float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
	const float4 r = roughness * c0 + c1;
	const float a004 = min(r.x * r.x, exp2(-9.28f * normalDotView)) * r.x + r.y;
	const float2 AB = float2(-1.04f, 1.04f) * a004 + r.zw;
	return f0 * AB.x + AB.y;
}

float3 ShadeHit(ReflectionHit hit, float3 origin, float3 direction, float primaryDistance)
{
	const RaytracedInstance instance = LoadRaytracedInstance(gParams.InstanceTable, hit.InstanceIndex);
	const float hitDistance = hit.Distance;
	// 밉은 카메라에서 픽셀 하나가 퍼진 너비로 고른다.
	const HitSurface surface = LoadHitSurface(instance, hit.PrimitiveIndex, hit.Barycentrics, hit.ObjectToWorld, hit.WorldToObject,
		origin, direction, hitDistance, gParams.PixelSpread * (primaryDistance + hitDistance));
	const float3 position = surface.Position;
	const float3 normal = surface.Normal;
	// 그림자 광선의 시작점은 법선 맵과 무관한 정점 법선 방향으로 띄운다.
	const float3 surfaceNormal = surface.SurfaceNormal;
	const float3 albedo = surface.Albedo;
	const float roughness = surface.Roughness;
	const float metallic = surface.Metallic;
	const float ambientOcclusion = surface.AmbientOcclusion;
	const float3 emission = surface.Emission;

	// 반사 광선을 거슬러 원래 면(또는 카메라) 쪽을 보는 방향
	const float3 viewDirection = -direction;
	const float normalDotView = max(dot(normal, viewDirection), 0.0f);
	const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	const float3 diffuseColor = albedo * (1.0f - metallic);
	// 환경광의 정반사는 미리 거른 환경 맵이 없어 조도에 lightingPS와 같은 근사 BRDF를 곱한다.
	const float3 ambientColor = diffuseColor + EnvBRDFApprox(f0, roughness, normalDotView);
	const float3 shadowOrigin = position + surfaceNormal * (gParams.NormalBias + (primaryDistance + hitDistance) * gParams.DistanceBiasScale);

	float3 direct = float3(0.0f, 0.0f, 0.0f);
	float3 skyIrradiance = float3(0.0f, 0.0f, 0.0f);
	bool hasSkyLight = false;
	const uint lightCount = min(gLightBuffer.Load(0u), gParams.MaxHitLights);
	for (uint lightIndex = 0u; lightIndex < lightCount; ++lightIndex)
	{
		const Light light = LoadLight(lightIndex);
		if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_USE_IN_DEFERRED))
			continue;
		const bool affectsDiffuse = HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_DIFFUSE);
		const bool affectsSpecular = HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_SPECULAR);
		if (!affectsDiffuse && !affectsSpecular)
			continue;

		const uint lightType = GetLightType(light.PackedFlags);
		if (lightType == LIGHT_TYPE_SKY)
		{
			if (!affectsDiffuse)
				continue;
			hasSkyLight = true;
			skyIrradiance += EvaluateSkyLight(light, normal);
			continue;
		}

		float3 toLight;
		float attenuation;
		if (!EvaluateLightIncidence(light, lightType, position, toLight, attenuation))
			continue;

		const float normalDotLight = dot(normal, toLight);
		if (normalDotLight <= 0.0f || attenuation <= 0.0f)
			continue;

		float visibility = 1.0f;
		if (lightIndex == gParams.SunLight && HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
			visibility = lerp(1.0f - light.ShadowStrength, 1.0f, TraceShadow(shadowOrigin, toLight));

		// lightingPS의 CalcLight와 같은 Cook-Torrance 정반사와 확산 반사
		const float3 halfVector = normalize(viewDirection + toLight);
		const float3 fresnel = FresnelSchlick(max(dot(halfVector, viewDirection), 0.0f), f0);
		const float3 radiance = light.Color * light.Intensity * attenuation * normalDotLight * visibility;
		if (affectsDiffuse)
			direct += (1.0f - fresnel) * diffuseColor / PI * radiance;
		if (affectsSpecular)
		{
			const float ndf = DistributionGGX(normal, halfVector, roughness);
			const float geometry = GeometrySmith(normalDotView, normalDotLight, roughness);
			direct += ndf * geometry * fresnel / max(4.0f * normalDotView * normalDotLight, 0.0001f) * radiance;
		}
	}

	float3 irradiance;
	if (gParams.HasIBLMaps != 0u)
	{
		irradiance = gIrradianceMap.SampleLevel(gBindlessSampler, normal, 0.0f).rgb;
	}
	else if (hasSkyLight)
	{
		irradiance = skyIrradiance;
	}
	else
	{
		irradiance = lerp(gParams.SkyZenith * 0.25f, gParams.SkyZenith, saturate(normal.y * 0.5f + 0.5f));
	}
	return direct + ambientColor * irradiance * gParams.IBLIntensity * ambientOcclusion + emission;
}

// 보이는 미세면만 뽑는 GGX 표본(Heitz 2018). view와 결과는 법선이 +Z인 공간이다.
float3 SampleVisibleNormal(float3 view, float alpha, float2 random)
{
	const float3 stretched = normalize(float3(alpha * view.x, alpha * view.y, view.z));
	const float lengthSquared = stretched.x * stretched.x + stretched.y * stretched.y;
	const float3 t1 = lengthSquared > 0.0f ? float3(-stretched.y, stretched.x, 0.0f) / sqrt(lengthSquared) : float3(1.0f, 0.0f, 0.0f);
	const float3 t2 = cross(stretched, t1);
	const float radius = sqrt(random.x);
	const float phi = 2.0f * PI * random.y;
	const float p1 = radius * cos(phi);
	const float blend = 0.5f * (1.0f + stretched.z);
	const float p2 = lerp(sqrt(max(0.0f, 1.0f - p1 * p1)), radius * sin(phi), blend);
	const float3 normal = p1 * t1 + p2 * t2 + sqrt(max(0.0f, 1.0f - p1 * p1 - p2 * p2)) * stretched;
	return normalize(float3(alpha * normal.x, alpha * normal.y, max(0.0f, normal.z)));
}

float3 SampleReflectionDirection(float3 toSurface, float3 normal, float roughness, uint2 pixel)
{
	const float3 mirror = reflect(toSurface, normal);
	if (roughness < gParams.MirrorRoughness)
		return mirror;

	const float3 up = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	const float3 tangent = normalize(cross(up, normal));
	const float3 bitangent = cross(normal, tangent);
	const float3 view = float3(dot(-toSurface, tangent), dot(-toSurface, bitangent), max(dot(-toSurface, normal), 0.001f));
	// 픽셀마다 다른 시작값에서 프레임마다 고르게 퍼지는 수열을 쓴다.
	const float2 random = frac(HashPixel(pixel) + (float)(gParams.FrameIndex % 1024u) * float2(0.7548776662f, 0.5698402910f));
	const float3 halfVector = SampleVisibleNormal(normalize(view), roughness * roughness, random);
	const float3 direction = reflect(toSurface, halfVector.x * tangent + halfVector.y * bitangent + halfVector.z * normal);
	// 표면 아래로 향하는 표본은 거울 방향으로 바꾼다.
	return dot(direction, normal) > 0.0f ? normalize(direction) : mirror;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint width, height;
	gReflection.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	const int3 pixel = int3(dispatchId.xy, 0);
	float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float hitDistance = REFLECTION_NOT_TRACED;

	const float4 normalSample = gNormal.Load(pixel);
	const uint packedData = UnpackShadingData(normalSample.a);
	const bool isLit = (packedData & SHADING_MODEL_MASK) != SHADING_MODEL_UNLIT;
	const float roughness = max(gPBR.Load(pixel).r, 0.04f);
	const float weight = 1.0f - smoothstep(gParams.RoughnessFadeStart, gParams.RoughnessFadeEnd, roughness);

	if (gDiffuse.Load(pixel).a >= 0.01f && isLit && weight > 0.0f)
	{
		const float3 normal = normalize(normalSample.rgb * 2.0f - 1.0f);
		const float3 worldPosition = gPosition.Load(pixel).xyz;
		const float3 toSurface = worldPosition - commonData.CameraPos;
		const float cameraDistance = length(toSurface);
		const float3 direction = SampleReflectionDirection(toSurface / max(cameraDistance, 1e-4f), normal, roughness, dispatchId.xy);
		hitDistance = REFLECTION_MISS_DISTANCE;

		RayDesc ray;
		ray.Origin = worldPosition + normal * (gParams.NormalBias + cameraDistance * gParams.DistanceBiasScale);
		ray.Direction = direction;
		ray.TMin = 0.0f;
		ray.TMax = gParams.RayMaxDistance;
		RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
		query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, RAYTRACING_MASK_REFLECTION, ray);
		RESOLVE_CANDIDATES(query)
		if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		{
			// 잘못된 값 하나가 블룸으로 번지지 않게 한다.
			ReflectionHit hit;
			hit.InstanceIndex = query.CommittedInstanceID();
			hit.PrimitiveIndex = query.CommittedPrimitiveIndex();
			hit.Barycentrics = query.CommittedTriangleBarycentrics();
			hit.Distance = query.CommittedRayT();
			hit.ObjectToWorld = query.CommittedObjectToWorld3x4();
			hit.WorldToObject = query.CommittedWorldToObject3x4();
			const float3 radiance = ShadeHit(hit, ray.Origin, direction, cameraDistance);
			const bool invalid = any(isnan(radiance)) || any(isinf(radiance));
			const float brightest = max(radiance.r, max(radiance.g, radiance.b));
			const float3 clamped = radiance * min(1.0f, MAX_REFLECTION_RADIANCE / max(brightest, 1e-4f));
			result = float4(invalid ? float3(0.0f, 0.0f, 0.0f) : clamped * weight, weight);
			hitDistance = min(hit.Distance, REFLECTION_MISS_DISTANCE);
		}
	}

	gReflection[dispatchId.xy] = result;
	gHitDistance[dispatchId.xy] = hitDistance;
}
