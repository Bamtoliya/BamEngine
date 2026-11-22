// 레이트레이싱 반사 (DXR 인라인 RayQuery, cs_6_5, DirectX12 전용)
// 매끈한 면에서 거울 방향으로 광선을 하나 쏘아 닿은 면의 밝기(rgb)와 합성 비율(a)을 기록한다.
// 거칠기가 커지거나 닿은 면이 없으면 비율이 0이 되어 lightingPS가 기존 환경광 반사를 쓴다.
// 닿은 면은 확산 직접광, 그림자 광선 하나(대표 방향광), 환경광, 발광만 계산한다. 정반사 하이라이트와 법선 맵은 쓰지 않는다.
#include "common.hlsli"
#include "light.hlsli"
#include "raytracing.hlsli"

static const float PI = 3.14159265359f;
static const uint INVALID_LIGHT = 0xFFFFFFFFu;

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
	uint2 Pad;
};

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPBR : register(t2, space3);
Texture2D gPosition : register(t4, space3);
TextureCube gIrradianceMap : register(t10, space3);
RaytracingAccelerationStructure gSceneTLAS : register(t13, space3);
RWTexture2D<float4> gReflection : register(u2, space3);
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

// 광선이 닿은 곳에서 픽셀 하나가 덮는 넓이에 맞는 밉
float SelectMip(uint width, uint height, uint levels, float uvArea, float worldArea, float coneWidth)
{
	const float texels = uvArea * (float)width * (float)height;
	if (texels <= 0.0f || worldArea <= 0.0f)
		return 0.0f;
	return clamp(0.5f * log2(texels / worldArea) + log2(max(coneWidth, 1e-5f)), 0.0f, (float)levels - 1.0f);
}

float4 SampleInstanceTexture(uint textureIndex, float2 uv, float uvArea, float worldArea, float coneWidth)
{
	uint width, height, levels;
	gBindlessTextures[NonUniformResourceIndex(textureIndex)].GetDimensions(0u, width, height, levels);
	const float mip = SelectMip(width, height, levels, uvArea, worldArea, coneWidth);
	return gBindlessTextures[NonUniformResourceIndex(textureIndex)].SampleLevel(gBindlessSampler, uv, mip);
}

float3 ShadeHit(ReflectionHit hit, float3 origin, float3 direction, float primaryDistance)
{
	const RaytracedInstance instance = LoadRaytracedInstance(gParams.InstanceTable, hit.InstanceIndex);
	const uint3 indices = LoadTriangleIndices(instance, hit.PrimitiveIndex);
	const float3 weights = GetBarycentricWeights(hit.Barycentrics);
	const float3x4 objectToWorld = hit.ObjectToWorld;
	const float3x4 worldToObject = hit.WorldToObject;
	ByteAddressBuffer positions = gBindlessBuffers[NonUniformResourceIndex(instance.PositionBuffer)];
	ByteAddressBuffer materials = gBindlessBuffers[NonUniformResourceIndex(instance.MaterialBuffer)];

	const float3 p0 = asfloat(positions.Load3(indices.x * instance.PositionStride));
	const float3 p1 = asfloat(positions.Load3(indices.y * instance.PositionStride));
	const float3 p2 = asfloat(positions.Load3(indices.z * instance.PositionStride));
	const float3 edge1 = mul((float3x3)objectToWorld, p1 - p0);
	const float3 edge2 = mul((float3x3)objectToWorld, p2 - p0);
	const float3 faceCross = cross(edge1, edge2);
	const float worldArea = length(faceCross);

	const float2 uv0 = LoadVertexUV(instance, indices.x);
	const float2 uv1 = LoadVertexUV(instance, indices.y);
	const float2 uv2 = LoadVertexUV(instance, indices.z);
	const float2 uv = uv0 * weights.x + uv1 * weights.y + uv2 * weights.z;
	const float2 uvEdge1 = uv1 - uv0;
	const float2 uvEdge2 = uv2 - uv0;
	const float uvArea = abs(uvEdge1.x * uvEdge2.y - uvEdge1.y * uvEdge2.x);

	float3 normal = float3(0.0f, 0.0f, 0.0f);
	float4 vertexColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	for (uint corner = 0u; corner < 3u; ++corner)
	{
		const uint address = indices[corner] * instance.MaterialStride;
		normal += asfloat(materials.Load3(address + VERTEX_NORMAL_OFFSET)) * weights[corner];
		vertexColor += asfloat(materials.Load4(address + VERTEX_COLOR_OFFSET)) * weights[corner];
	}
	// 법선은 역행렬의 전치로 변환한다.
	normal = mul(normal, (float3x3)worldToObject);
	const float normalLength = length(normal);
	const float3 faceNormal = worldArea > 0.0f ? faceCross / worldArea : -direction;
	normal = normalLength > 1e-6f ? normal / normalLength : faceNormal;
	if (dot(normal, direction) > 0.0f)
		normal = -normal;

	const float hitDistance = hit.Distance;
	const float3 position = origin + direction * hitDistance;
	const float coneWidth = gParams.PixelSpread * (primaryDistance + hitDistance) / max(abs(dot(normal, direction)), 0.1f);

	float3 albedo = instance.BaseColor.rgb * vertexColor.rgb;
	if (instance.BaseColorTexture != INVALID_BINDLESS_INDEX)
		albedo *= SampleInstanceTexture(instance.BaseColorTexture, uv, uvArea, worldArea, coneWidth).rgb;

	float roughness = clamp(instance.Roughness, 0.04f, 1.0f);
	float metallic = saturate(instance.Metallic);
	float ambientOcclusion = saturate(instance.AmbientOcclusion);
	if (instance.PbrTexture != INVALID_BINDLESS_INDEX)
	{
		const float4 pbr = SampleInstanceTexture(instance.PbrTexture, uv, uvArea, worldArea, coneWidth);
		roughness = clamp(pbr.r, 0.04f, 1.0f);
		metallic = saturate(pbr.g);
		ambientOcclusion = pbr.a > 0.01f ? pbr.a : ambientOcclusion;
	}

	float3 emission = instance.EmissiveColor;
	if (instance.EmissiveTexture != INVALID_BINDLESS_INDEX)
		emission = SampleInstanceTexture(instance.EmissiveTexture, uv, uvArea, worldArea, coneWidth).rgb;

	// 금속은 확산 반사가 없으므로 환경광을 자기 색으로 반사한다.
	const float3 diffuseColor = albedo * (1.0f - metallic);
	const float3 ambientColor = diffuseColor + lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic) * (1.0f - 0.5f * roughness);
	const float3 shadowOrigin = position + normal * (gParams.NormalBias + (primaryDistance + hitDistance) * gParams.DistanceBiasScale);

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
			const float indirect = max(light.Range, 0.0f) * max(light.Intensity, 0.0f);
			const float up = saturate(dot(normal, normalize(light.Direction)) * 0.5f + 0.5f);
			skyIrradiance += lerp(max(light.AttenuationCoeff, 0.0f), max(light.Color, 0.0f), up) * indirect;
			continue;
		}

		float3 toLight;
		float attenuation = 1.0f;
		if (lightType == LIGHT_TYPE_DIRECTIONAL)
		{
			toLight = normalize(-light.Direction);
		}
		else
		{
			const float3 delta = light.Position - position;
			const float lightDistance = length(delta);
			if (lightDistance < 0.0001f || lightDistance > light.Range)
				continue;
			toLight = delta / lightDistance;
			attenuation = CalcAttenuation(light, lightDistance);
			if (lightType == LIGHT_TYPE_SPOT)
				attenuation *= CalcSpotFade(light, toLight);
		}

		const float normalDotLight = dot(normal, toLight);
		if (normalDotLight <= 0.0f || attenuation <= 0.0f)
			continue;

		float visibility = 1.0f;
		if (lightIndex == gParams.SunLight && HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
			visibility = lerp(1.0f - light.ShadowStrength, 1.0f, TraceShadow(shadowOrigin, toLight));
		direct += diffuseColor / PI * light.Color * light.Intensity * attenuation * normalDotLight * visibility;
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

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint width, height;
	gReflection.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	const int3 pixel = int3(dispatchId.xy, 0);
	float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);

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
		const float3 direction = reflect(toSurface / max(cameraDistance, 1e-4f), normal);

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
			result = float4(any(isnan(radiance)) || any(isinf(radiance)) ? float3(0.0f, 0.0f, 0.0f) : min(radiance, 1000.0f), weight);
		}
	}

	gReflection[dispatchId.xy] = result;
}
