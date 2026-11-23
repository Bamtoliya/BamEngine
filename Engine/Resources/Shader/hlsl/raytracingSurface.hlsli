// 광선이 닿은 면의 재질과 법선을 복원한다. 반사와 프로브 GI가 함께 쓴다 (DXR 인라인 RayQuery, DirectX12 전용).
// light.hlsli와 raytracing.hlsli를 먼저 포함해야 한다.
#ifndef BAM_RAYTRACING_SURFACE_HLSLI
#define BAM_RAYTRACING_SURFACE_HLSLI

// 광선이 닿은 곳에서 표본 하나가 덮는 넓이에 맞는 밉
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

struct HitSurface
{
	float3 Position;
	float3 Normal;         // 법선 맵을 적용하고 광선 쪽으로 돌린 법선
	float3 SurfaceNormal;  // 법선 맵 없이 광선 쪽으로 돌린 정점 법선. 그림자 광선 시작점을 띄울 때 쓴다.
	bool BackFace;         // 정점 법선 기준 뒷면에 닿았는지
	float3 Albedo;
	float Roughness;
	float Metallic;
	float AmbientOcclusion;
	float3 Emission;
};

// footprint: 닿은 곳에서 표본 하나가 덮는 너비(m)를 면이 광선과 수직일 때로 계산한 값. 비스듬한 면은 안에서 넓힌다.
HitSurface LoadHitSurface(RaytracedInstance instance, uint primitiveIndex, float2 barycentrics, float3x4 objectToWorld, float3x4 worldToObject,
	float3 origin, float3 direction, float hitDistance, float footprint)
{
	const uint3 indices = LoadTriangleIndices(instance, primitiveIndex);
	const float3 weights = GetBarycentricWeights(barycentrics);
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
	float3 tangent = float3(0.0f, 0.0f, 0.0f);
	float4 vertexColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	for (uint corner = 0u; corner < 3u; ++corner)
	{
		const uint address = indices[corner] * instance.MaterialStride;
		normal += asfloat(materials.Load3(address + VERTEX_NORMAL_OFFSET)) * weights[corner];
		tangent += asfloat(materials.Load3(address + VERTEX_TANGENT_OFFSET)) * weights[corner];
		vertexColor += asfloat(materials.Load4(address + VERTEX_COLOR_OFFSET)) * weights[corner];
	}
	// 법선은 역행렬의 전치로, 접선은 행렬 그대로 변환한다(gbufferVS와 같다).
	// 역행렬의 전치는 음수 배율에서도 바깥을 향하므로 gbufferVS의 행렬식 부호 보정과 같은 방향이 된다.
	normal = mul(normal, (float3x3)worldToObject);
	const float normalLength = length(normal);
	const float3 faceNormal = worldArea > 0.0f ? faceCross / worldArea : -direction;
	normal = normalLength > 1e-6f ? normal / normalLength : faceNormal;

	HitSurface surface;
	// 뒷면에서 본 면은 법선 맵을 적용한 뒤 뒤집는다. G-Buffer와 같은 접선 공간을 쓰기 위함이다.
	surface.BackFace = dot(normal, direction) > 0.0f;
	surface.Position = origin + direction * hitDistance;
	surface.SurfaceNormal = surface.BackFace ? -normal : normal;
	const float coneWidth = footprint / max(abs(dot(normal, direction)), 0.1f);

	// gbufferPS와 같은 법선 맵 처리. 접선이 없거나 법선과 평행하면 정점 법선을 쓴다.
	if (instance.NormalTexture != INVALID_BINDLESS_INDEX)
	{
		tangent = mul((float3x3)objectToWorld, tangent);
		tangent -= dot(tangent, normal) * normal;
		const float tangentLength = length(tangent);
		if (tangentLength > 1e-6f)
		{
			const float3 T = tangent / tangentLength;
			const float3x3 TBN = float3x3(T, cross(normal, T), normal);
			const float4 normalSample = SampleInstanceTexture(instance.NormalTexture, uv, uvArea, worldArea, coneWidth);
			const float3 tangentNormal = normalSample.xyz * 2.0f - 1.0f;
			if (normalSample.z > 0.45f && abs(length(tangentNormal) - 1.0f) < 0.4f)
				normal = normalize(mul(tangentNormal, TBN));
		}
	}
	surface.Normal = surface.BackFace ? -normal : normal;

	surface.Albedo = instance.BaseColor.rgb * vertexColor.rgb;
	if (instance.BaseColorTexture != INVALID_BINDLESS_INDEX)
		surface.Albedo *= SampleInstanceTexture(instance.BaseColorTexture, uv, uvArea, worldArea, coneWidth).rgb;

	surface.Roughness = clamp(instance.Roughness, 0.04f, 1.0f);
	surface.Metallic = saturate(instance.Metallic);
	surface.AmbientOcclusion = saturate(instance.AmbientOcclusion);
	if (instance.PbrTexture != INVALID_BINDLESS_INDEX)
	{
		const float4 pbr = SampleInstanceTexture(instance.PbrTexture, uv, uvArea, worldArea, coneWidth);
		surface.Roughness = clamp(pbr.r, 0.04f, 1.0f);
		surface.Metallic = saturate(pbr.g);
		surface.AmbientOcclusion = pbr.a > 0.01f ? pbr.a : surface.AmbientOcclusion;
	}

	surface.Emission = instance.EmissiveColor;
	if (instance.EmissiveTexture != INVALID_BINDLESS_INDEX)
		surface.Emission = SampleInstanceTexture(instance.EmissiveTexture, uv, uvArea, worldArea, coneWidth).rgb;
	return surface;
}

// 방향광·점광원·스포트의 입사 방향과 감쇠. 범위 밖이거나 너무 가까우면 false다. 스카이 광원은 부르지 않는다.
bool EvaluateLightIncidence(Light light, uint lightType, float3 position, out float3 toLight, out float attenuation)
{
	toLight = float3(0.0f, 1.0f, 0.0f);
	attenuation = 1.0f;
	if (lightType == LIGHT_TYPE_DIRECTIONAL)
	{
		toLight = normalize(-light.Direction);
		return true;
	}

	const float3 delta = light.Position - position;
	const float lightDistance = length(delta);
	if (lightDistance < 0.0001f || lightDistance > light.Range)
		return false;
	toLight = delta / lightDistance;
	attenuation = CalcAttenuation(light, lightDistance);
	if (lightType == LIGHT_TYPE_SPOT)
		attenuation *= CalcSpotFade(light, toLight);
	return true;
}

// 스카이 광원의 반구 조도(lightingPS와 같은 식). direction은 면 법선 또는 하늘을 보는 방향이다.
float3 EvaluateSkyLight(Light light, float3 direction)
{
	const float indirect = max(light.Range, 0.0f) * max(light.Intensity, 0.0f);
	const float up = saturate(dot(direction, normalize(light.Direction)) * 0.5f + 0.5f);
	return lerp(max(light.AttenuationCoeff, 0.0f), max(light.Color, 0.0f), up) * indirect;
}

#endif
