// 레이트레이싱 방향광 그림자 마스크 (DXR 인라인 RayQuery, cs_6_5, DirectX12 전용)
// G-Buffer의 월드 위치에서 첫 번째 그림자 방향광 쪽으로 광선을 하나 쏘아 가시성(1 = 빛, 0 = 차폐)을 기록한다.
// 차폐 대상은 RaytracingSceneManager가 TLAS에 넣은 정적 불투명 캐스터뿐이다. 나머지 캐스터는 섀도맵이 담당하고
// lightingPS가 min(마스크, 섀도맵)으로 합성한다.
#include "common.hlsli"
#include "light.hlsli"
#include "shading_flags.hlsli"

struct RaytracedShadowParams
{
	float NormalBias;          // 표면 법선 방향 시작점 오프셋(m)
	float DistanceBiasScale;   // 카메라 거리 1m당 추가 오프셋(m) — 원거리 위치 정밀도 보정
	float RayMaxDistance;      // 광선 최대 길이(m)
	uint  Pad;
};

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPosition : register(t4, space3);
RaytracingAccelerationStructure gSceneTLAS : register(t13, space3);
RWTexture2D<float> gShadowMask : register(u2, space3);
ConstantBuffer<RaytracedShadowParams> gShadowParams : register(b1, space3);

static const uint INVALID_LIGHT = 0xFFFFFFFFu;

// lightingPS가 섀도맵을 적용하는 광원과 같은 규칙: 지연 조명에 쓰이는 첫 번째 그림자 방향광.
uint FindShadowDirectionalLight()
{
	const uint numLights = gLightBuffer.Load(0u);
	for (uint i = 0u; i < numLights; ++i)
	{
		const uint flags = gLightBuffer.Load(LIGHT_BUFFER_HEADER_SIZE + i * LIGHT_BUFFER_STRIDE + 44u);
		if (GetLightType(flags) == LIGHT_TYPE_DIRECTIONAL &&
			HasLightFlag(flags, LIGHT_FLAG_USE_IN_DEFERRED) && HasLightFlag(flags, LIGHT_FLAG_CAST_SHADOWS))
			return i;
	}
	return INVALID_LIGHT;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	uint width, height;
	gShadowMask.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	const int3 pixel = int3(dispatchId.xy, 0);
	float visibility = 1.0f;

	const float4 diffuse = gDiffuse.Load(pixel);
	const float4 normalSample = gNormal.Load(pixel);
	const uint packedData = UnpackShadingData(normalSample.a);
	const bool receiveShadow = (packedData & FLAG_RECEIVE_SHADOWS) != 0u;
	const bool isLit = (packedData & SHADING_MODEL_MASK) != SHADING_MODEL_UNLIT;

	const uint lightIndex = FindShadowDirectionalLight();
	if (diffuse.a >= 0.01f && receiveShadow && isLit && lightIndex != INVALID_LIGHT)
	{
		const Light light = LoadLight(lightIndex);
		const float3 normal = normalize(normalSample.rgb * 2.0f - 1.0f);
		const float3 worldPosition = gPosition.Load(pixel).xyz;
		const float3 toLight = normalize(-light.Direction);

		if (dot(normal, toLight) <= 0.0f)
		{
			// 빛을 등진 면은 조명 계산에서도 0이므로 광선을 쏘지 않는다.
			visibility = 0.0f;
		}
		else
		{
			const float cameraDistance = distance(worldPosition, commonData.CameraPos);
			const float bias = gShadowParams.NormalBias + cameraDistance * gShadowParams.DistanceBiasScale;

			RayDesc ray;
			ray.Origin = worldPosition + normal * bias;
			ray.Direction = toLight;
			ray.TMin = 0.0f;
			ray.TMax = gShadowParams.RayMaxDistance;

			// 차폐 여부만 필요하므로 첫 교차에서 탐색을 끝낸다.
			RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> query;
			query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, 0xFF, ray);
			query.Proceed();
			if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
				visibility = 0.0f;
		}
	}

	gShadowMask[dispatchId.xy] = visibility;
}
