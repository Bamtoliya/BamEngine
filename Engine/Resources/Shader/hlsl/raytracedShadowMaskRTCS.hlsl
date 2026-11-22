// 레이트레이싱 그림자 마스크 (DXR 인라인 RayQuery, cs_6_5, DirectX12 전용)
// G-Buffer의 월드 위치에서 광원마다 광선을 하나 쏘아 가시성(1 = 빛, 0 = 차폐)을 채널에 기록한다.
// 채널별 광원은 CPU(AssignRaytracedShadows)가 정하고 lightingPS가 같은 인덱스로 읽는다.
// 차폐 대상은 RaytracingSceneManager가 TLAS에 넣은 정적 불투명 캐스터뿐이다. 나머지 캐스터는 섀도맵을 가진 광원만 담당하고
// lightingPS가 min(마스크, 섀도맵)으로 합성한다.
#include "common.hlsli"
#include "light.hlsli"
#include "shading_flags.hlsli"

static const uint MAX_RAYTRACED_SHADOW_LIGHTS = 4u;
static const uint INVALID_LIGHT = 0xFFFFFFFFu;
// 점·스포트 섀도맵의 Near와 같다. 광원을 감싼 전구 메시에 막히지 않게 한다.
static const float LOCAL_LIGHT_NEAR = 0.05f;

struct RaytracedShadowParams
{
	float NormalBias;          // 표면 법선 방향 시작점 오프셋(m)
	float DistanceBiasScale;   // 카메라 거리 1m당 추가 오프셋(m) — 원거리 위치 정밀도 보정
	float RayMaxDistance;      // 방향광 광선의 최대 길이(m)
	uint  Pad;
	uint4 Lights;              // 채널별 광원 버퍼 인덱스
};

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPosition : register(t4, space3);
RaytracingAccelerationStructure gSceneTLAS : register(t13, space3);
RWTexture2D<float4> gShadowMask : register(u2, space3);
ConstantBuffer<RaytracedShadowParams> gShadowParams : register(b1, space3);

// 조명 결과가 0인 픽셀(빛을 등진 면, 범위·원뿔 밖)은 광선을 쏘지 않는다.
float TraceVisibility(Light light, float3 origin, float3 normal)
{
	float3 toLight;
	float maxDistance;
	if (GetLightType(light.PackedFlags) == LIGHT_TYPE_DIRECTIONAL)
	{
		toLight = normalize(-light.Direction);
		maxDistance = gShadowParams.RayMaxDistance;
	}
	else
	{
		const float3 delta = light.Position - origin;
		const float lightDistance = length(delta);
		if (lightDistance <= LOCAL_LIGHT_NEAR || lightDistance > light.Range)
			return 1.0f;

		toLight = delta / lightDistance;
		if (GetLightType(light.PackedFlags) == LIGHT_TYPE_SPOT && dot(-toLight, normalize(light.Direction)) < light.SpotOuterCos)
			return 1.0f;
		maxDistance = lightDistance - LOCAL_LIGHT_NEAR;
	}

	if (dot(normal, toLight) <= 0.0f)
		return 0.0f;

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = toLight;
	ray.TMin = 0.0f;
	ray.TMax = maxDistance;

	// 차폐 여부만 필요하므로 첫 교차에서 탐색을 끝낸다.
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> query;
	query.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, 0xFF, ray);
	query.Proceed();
	return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0f : 1.0f;
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

	const float4 diffuse = gDiffuse.Load(pixel);
	const float4 normalSample = gNormal.Load(pixel);
	const uint packedData = UnpackShadingData(normalSample.a);
	const bool receiveShadow = (packedData & FLAG_RECEIVE_SHADOWS) != 0u;
	const bool isLit = (packedData & SHADING_MODEL_MASK) != SHADING_MODEL_UNLIT;

	if (diffuse.a >= 0.01f && receiveShadow && isLit)
	{
		const float3 normal = normalize(normalSample.rgb * 2.0f - 1.0f);
		const float3 worldPosition = gPosition.Load(pixel).xyz;
		const float cameraDistance = distance(worldPosition, commonData.CameraPos);
		const float3 origin = worldPosition + normal * (gShadowParams.NormalBias + cameraDistance * gShadowParams.DistanceBiasScale);

		for (uint channel = 0u; channel < MAX_RAYTRACED_SHADOW_LIGHTS; ++channel)
		{
			const uint lightIndex = gShadowParams.Lights[channel];
			if (lightIndex != INVALID_LIGHT)
				visibility[channel] = TraceVisibility(LoadLight(lightIndex), origin, normal);
		}
	}

	gShadowMask[dispatchId.xy] = visibility;
}
