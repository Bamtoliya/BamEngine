#include "common.hlsli"
#include "light.hlsli"
#include "shading_flags.hlsli"
#include "fog.hlsli"
#include "cloud.hlsli"

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct ShadowData
{
	float4x4 LightViewProjections[3];
	float4 CascadeSplits;
	float BaseBias;
	float SlopeBias;
	float NormalBias;
	float _pad;
	float4x4 SpotViewProjection;
	float SpotBaseBias;
	float SpotSlopeBias;
	float SpotNormalBias;
	float HasSpotShadow;
	float PointBaseBias;
	float PointSlopeBias;
	float PointNormalBias;
	float HasPointShadow;
	float3 PointLightPos;
	float PointLightRange;
};

struct LightingPassData
{
	ShadowData Shadow;
	VolumetricCloudData Cloud;
	float3 SunDirection;
	float PlanetRadius;
};

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPBR : register(t2, space3);
Texture2D gEmission : register(t3, space3);
Texture2D gPosition : register(t4, space3);
Texture2D gShadowDepth : register(t5, space3);
ByteAddressBuffer gLightBuffer : register(t6, space3);
TextureCube gPointShadowDepth : register(t7, space3);

SamplerState gBufferSampler : register(s0, space3);
ConstantBuffer<LightingPassData> gLightingData : register(b1, space3);
Texture3D<float4> gCloudBaseShape : register(t0, space1);
Texture2D<float4> gCloudWeatherMap : register(t1, space1);
SamplerState gCloudSampler : register(s0, space1);

static const float PI = 3.14159265359f;
static const uint LIGHT_BUFFER_HEADER_SIZE = 16u;
static const uint LIGHT_BUFFER_STRIDE = 80u;
static const uint INVALID_LIGHT_INDEX = 0xFFFFFFFFu;

Light LoadLight(uint lightIndex)
{
	uint address = LIGHT_BUFFER_HEADER_SIZE + lightIndex * LIGHT_BUFFER_STRIDE;
	Light light;

	light.Position = asfloat(gLightBuffer.Load3(address));
	light.Intensity = asfloat(gLightBuffer.Load(address + 12u));
	light.Direction = asfloat(gLightBuffer.Load3(address + 16u));
	light.Range = asfloat(gLightBuffer.Load(address + 28u));
	light.Color = asfloat(gLightBuffer.Load3(address + 32u));
	light.PackedFlags = gLightBuffer.Load(address + 44u);
	light.AttenuationCoeff = asfloat(gLightBuffer.Load3(address + 48u));
	light.Pad0 = asfloat(gLightBuffer.Load(address + 60u));
	light.SpotInnerCos = asfloat(gLightBuffer.Load(address + 64u));
	light.SpotOuterCos = asfloat(gLightBuffer.Load(address + 68u));
	light.SpotFalloff = asfloat(gLightBuffer.Load(address + 72u));
	light.Pad1 = asfloat(gLightBuffer.Load(address + 76u));

	return light;
}

uint FindAtmosphereSunLightIndex(uint numLights)
{
	float sunLength = length(gLightingData.SunDirection);
	if (sunLength <= 0.0001f)
		return INVALID_LIGHT_INDEX;

	float3 sunDirection = gLightingData.SunDirection / sunLength;
	uint selectedIndex = INVALID_LIGHT_INDEX;

	for (uint lightIndex = 0u; lightIndex < numLights; ++lightIndex)
	{
		Light light = LoadLight(lightIndex);
		if (GetLightType(light.PackedFlags) != LIGHT_TYPE_DIRECTIONAL ||
			!HasLightFlag(light.PackedFlags, LIGHT_FLAG_USE_IN_DEFERRED))
		{
			continue;
		}

		float directionLength = length(light.Direction);
		if (directionLength <= 0.0001f)
			continue;

		float3 toLight = -light.Direction / directionLength;
		if (dot(toLight, sunDirection) < 0.9999f)
			continue;

		if (selectedIndex != INVALID_LIGHT_INDEX)
			return INVALID_LIGHT_INDEX;

		selectedIndex = lightIndex;
	}

	return selectedIndex;
}

float DistributionGGX(float3 normal, float3 halfVector, float roughness)
{
	float a2 = roughness * roughness * roughness * roughness;
	float normalDotHalf = max(dot(normal, halfVector), 0.0f);
	float denominator = normalDotHalf * normalDotHalf * (a2 - 1.0f) + 1.0f;

	return a2 / (PI * denominator * denominator);
}

float GeometrySmith(float normalDotView, float normalDotLight, float roughness)
{
	float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
	float geometryView = normalDotView / (normalDotView * (1.0f - k) + k);
	float geometryLight = normalDotLight / (normalDotLight * (1.0f - k) + k);

	return geometryView * geometryLight;
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
	return f0 + (1.0f - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

static const float2 VOGEL_DISK_16[16] = {
	float2(-0.1345f, 0.1165f), float2(0.1874f, -0.2443f), float2(-0.0694f, 0.3891f), float2(-0.2505f, -0.3951f),
	float2(0.5057f, 0.1706f),  float2(-0.4907f, 0.2928f), float2(0.2072f, -0.6033f), float2(0.2520f, 0.6375f),
	float2(-0.6277f, -0.3292f), float2(0.6791f, -0.2741f), float2(-0.3541f, 0.7247f), float2(-0.2198f, -0.8197f),
	float2(0.7161f, 0.4921f),  float2(-0.8407f, 0.3168f), float2(0.4746f, -0.7937f), float2(0.1895f, 0.9657f)
};

float InterleavedGradientNoise(float2 screenPos)
{
	float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	return frac(magic.z * frac(dot(screenPos, magic.xy)));
}

bool FindBlocker(float3 projCoord, float depthBias, float2 searchRadius, float2x2 rot, float2 minUV, float2 maxUV, out float avgBlockerDepth)
{
	int blockerCount = 0;
	float blockerDepthSum = 0.0f;

	// 8개 샘플로 주변 차폐물(Blocker) 평균 깊이 탐색
	for (int i = 0; i < 8; ++i)
	{
		float2 offset = mul(rot, VOGEL_DISK_16[i * 2]) * searchRadius;
		float2 sampleUV = clamp(projCoord.xy + offset, minUV, maxUV);
		float sampleDepth = gShadowDepth.SampleLevel(gBufferSampler, sampleUV, 0).r;
		if (sampleDepth < projCoord.z - depthBias)
		{
			blockerDepthSum += sampleDepth;
			blockerCount++;
		}
	}

	if (blockerCount == 0)
	{
		avgBlockerDepth = 0.0f;
		return false;
	}

	avgBlockerDepth = blockerDepthSum / float(blockerCount);
	return true;
}

// ── 캐스케이드 디버그 시각화 매크로 (1로 설정 시 영역별 RGB 틴트 출력) ──
#define DEBUG_CASCADE_COLORS 0

// 단일 캐스케이드에 대한 PCSS + Vogel Disk 16-Tap 샘플링
float EvaluateCascadeShadow(uint cascadeIndex, float3 biasedWorldPos, float depthBias, float2 screenPos, float2x2 rot, float2 texelSize)
{
	float4 lightClip = mul(gLightingData.Shadow.LightViewProjections[cascadeIndex], float4(biasedWorldPos, 1.0f));
	float3 projection = lightClip.xyz / max(lightClip.w, 0.0001f);
	projection.xy = projection.xy * 0.5f + 0.5f;
	projection.y = 1.0f - projection.y;

	if (projection.x < 0.0f || projection.x > 1.0f || projection.y < 0.0f || projection.y > 1.0f || projection.z > 1.0f)
	{
		return 1.0f;
	}

	float2 atlasOffset = float2((cascadeIndex % 2) * 0.5f, (cascadeIndex / 2) * 0.5f);
	float2 atlasUV = projection.xy * 0.5f + atlasOffset;
	float3 atlasProj = float3(atlasUV, projection.z);

	float2 minUV = atlasOffset + texelSize * 1.5f;
	float2 maxUV = atlasOffset + 0.5f - texelSize * 1.5f;

	const float lightSizeUV = 0.0175f;
	float2 searchRadius = texelSize * 8.0f;
	float avgBlockerDepth = 0.0f;

	if (!FindBlocker(atlasProj, depthBias, searchRadius, rot, minUV, maxUV, avgBlockerDepth))
	{
		return 1.0f;
	}

	float penumbraRatio = saturate((projection.z - avgBlockerDepth) / max(avgBlockerDepth, 0.0001f));
	float filterRadiusScale = clamp(penumbraRatio * lightSizeUV, texelSize.x * 1.2f, texelSize.x * 8.0f);
	float2 filterRadius = float2(filterRadiusScale, filterRadiusScale);

	float visibility = 0.0f;
	for (int i = 0; i < 16; ++i)
	{
		float2 sampleOffset = mul(rot, VOGEL_DISK_16[i]) * filterRadius;
		float2 sampleUV = clamp(atlasProj.xy + sampleOffset, minUV, maxUV);
		float closestDepth = gShadowDepth.Sample(gBufferSampler, sampleUV).r;
		visibility += (projection.z - depthBias <= closestDepth) ? 1.0f : 0.0f;
	}

	return visibility / 16.0f;
}

float3 GetCascadeDebugColor(float3 worldPosition)
{
	float viewDepth = mul(commonData.View, float4(worldPosition, 1.0f)).z;
	if (viewDepth > gLightingData.Shadow.CascadeSplits.z)
		return float3(1.0f, 1.0f, 1.0f);

	const float3 colors[3] = {
		float3(1.0f, 0.3f, 0.3f), // Cascade 0: 빨강
		float3(0.3f, 1.0f, 0.3f), // Cascade 1: 초록
		float3(0.3f, 0.5f, 1.0f)  // Cascade 2: 파랑
	};

	const float blendBandRatio = 0.15f;
	uint primaryCascade = 0;
	float splitDist = gLightingData.Shadow.CascadeSplits.x;

	if (viewDepth > gLightingData.Shadow.CascadeSplits.y)
	{
		primaryCascade = 2;
		splitDist = gLightingData.Shadow.CascadeSplits.z;
	}
	else if (viewDepth > gLightingData.Shadow.CascadeSplits.x)
	{
		primaryCascade = 1;
		splitDist = gLightingData.Shadow.CascadeSplits.y;
	}

	float3 color = colors[primaryCascade];
	float blendStart = splitDist * (1.0f - blendBandRatio);

	if (primaryCascade < 2 && viewDepth > blendStart)
	{
		float blendFactor = saturate((viewDepth - blendStart) / (splitDist - blendStart));
		color = lerp(color, colors[primaryCascade + 1], blendFactor);
	}
	return color;
}

float CalcShadowFactor(Light light, float3 worldPosition, float3 normal, float2 screenPos)
{
	if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS) ||
		GetLightType(light.PackedFlags) != LIGHT_TYPE_DIRECTIONAL)
	{
		return 1.0f;
	}

	float3 lightDirection = normalize(-light.Direction);
	float cosTheta = saturate(dot(normal, lightDirection));
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));
	float depthBias = gLightingData.Shadow.BaseBias + gLightingData.Shadow.SlopeBias * sinTheta;
	float3 biasedWorldPosition = worldPosition + normal * gLightingData.Shadow.NormalBias;

	// 카메라 뷰 공간 깊이(View Z)를 기준으로 캐스케이드 선택
	float viewDepth = mul(commonData.View, float4(worldPosition, 1.0f)).z;
	if (viewDepth > gLightingData.Shadow.CascadeSplits.z)
	{
		return 1.0f; // 최대 그림자 거리 초과 시 그림자 생략
	}

	uint shadowWidth;
	uint shadowHeight;
	gShadowDepth.GetDimensions(shadowWidth, shadowHeight);
	float2 texelSize = 1.0f / float2(shadowWidth, shadowHeight);

	float randomAngle = InterleavedGradientNoise(screenPos) * 6.2831853f;
	float sinA, cosA;
	sincos(randomAngle, sinA, cosA);
	float2x2 rot = float2x2(cosA, -sinA, sinA, cosA);

	// 주 캐스케이드 및 전환 경계선 계산
	const float blendBandRatio = 0.15f; // 경계선 직전 15% 구간에서 블렌딩
	uint primaryCascade = 0;
	float splitDist = gLightingData.Shadow.CascadeSplits.x;

	if (viewDepth > gLightingData.Shadow.CascadeSplits.y)
	{
		primaryCascade = 2;
		splitDist = gLightingData.Shadow.CascadeSplits.z;
	}
	else if (viewDepth > gLightingData.Shadow.CascadeSplits.x)
	{
		primaryCascade = 1;
		splitDist = gLightingData.Shadow.CascadeSplits.y;
	}

	float blendStart = splitDist * (1.0f - blendBandRatio);
	float shadow = EvaluateCascadeShadow(primaryCascade, biasedWorldPosition, depthBias, screenPos, rot, texelSize);

	// 1) 다음 캐스케이드와의 경계선 블렌딩 (Cascade 0 -> 1, Cascade 1 -> 2)
	if (primaryCascade < 2 && viewDepth > blendStart)
	{
		float blendFactor = saturate((viewDepth - blendStart) / (splitDist - blendStart));
		float nextShadow = EvaluateCascadeShadow(primaryCascade + 1, biasedWorldPosition, depthBias, screenPos, rot, texelSize);
		shadow = lerp(shadow, nextShadow, blendFactor);
	}
	// 2) 마지막 캐스케이드(원거리)에서의 최대 거리 페이드아웃 (Fade-out to Light)
	else if (primaryCascade == 2 && viewDepth > blendStart)
	{
		float fadeFactor = saturate((splitDist - viewDepth) / (splitDist - blendStart));
		shadow = lerp(1.0f, shadow, fadeFactor);
	}

	return shadow;
}

float CalcSpotShadowFactor(Light light, float3 worldPosition, float3 normal, float2 screenPos)
{
	if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS) ||
		GetLightType(light.PackedFlags) != LIGHT_TYPE_SPOT ||
		gLightingData.Shadow.HasSpotShadow < 0.5f)
	{
		return 1.0f;
	}

	float3 toLight = light.Position - worldPosition;
	float distToLight = length(toLight);
	if (distToLight > light.Range || distToLight <= 0.001f)
	{
		return 1.0f;
	}

	float3 lightDirection = toLight / distToLight;
	float cosTheta = saturate(dot(normal, lightDirection));
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));
	float depthBias = gLightingData.Shadow.SpotBaseBias + gLightingData.Shadow.SpotSlopeBias * sinTheta;
	float3 biasedWorldPosition = worldPosition + normal * gLightingData.Shadow.SpotNormalBias;

	// 스팟 라이트 원근 클립 공간 변환
	float4 lightClip = mul(gLightingData.Shadow.SpotViewProjection, float4(biasedWorldPosition, 1.0f));
	if (lightClip.w <= 0.001f)
	{
		return 1.0f; // 광원 뒤쪽
	}

	float3 projection = lightClip.xyz / lightClip.w;
	projection.xy = projection.xy * 0.5f + 0.5f;
	projection.y = 1.0f - projection.y;

	// 원근 투영 절두체(Frustum) 경계 검사
	if (projection.x < 0.0f || projection.x > 1.0f || projection.y < 0.0f || projection.y > 1.0f || projection.z < 0.0f || projection.z > 1.0f)
	{
		return 1.0f;
	}

	uint shadowWidth, shadowHeight;
	gShadowDepth.GetDimensions(shadowWidth, shadowHeight);
	float2 texelSize = 1.0f / float2(shadowWidth, shadowHeight);

	// 우하단 사분면 (0.5, 0.5) 아틀라스 매핑
	float2 atlasOffset = float2(0.5f, 0.5f);
	float2 atlasUV = projection.xy * 0.5f + atlasOffset;
	float3 atlasProj = float3(atlasUV, projection.z);

	// 아틀라스 우하단 사분면 경계 클램핑 (사분면 침범 방지)
	float2 minUV = atlasOffset + texelSize * 1.5f;
	float2 maxUV = float2(1.0f, 1.0f) - texelSize * 1.5f;

	// 픽셀별 무작위 회전각 계산
	float randomAngle = InterleavedGradientNoise(screenPos) * 6.2831853f;
	float sinA, cosA;
	sincos(randomAngle, sinA, cosA);
	float2x2 rot = float2x2(cosA, -sinA, sinA, cosA);

	// 16-Tap Vogel Disk PCF 필터링
	float2 filterRadius = texelSize * 2.5f;
	float visibility = 0.0f;
	for (int i = 0; i < 16; ++i)
	{
		float2 sampleOffset = mul(rot, VOGEL_DISK_16[i]) * filterRadius;
		float2 sampleUV = clamp(atlasProj.xy + sampleOffset, minUV, maxUV);
		float closestDepth = gShadowDepth.Sample(gBufferSampler, sampleUV).r;
		visibility += (projection.z - depthBias <= closestDepth) ? 1.0f : 0.0f;
	}

	return visibility / 16.0f;
}

static const float3 CUBE_SAMPLE_OFFSETS[20] = {
	float3( 1,  1,  1), float3( 1, -1,  1), float3(-1, -1,  1), float3(-1,  1,  1),
	float3( 1,  1, -1), float3( 1, -1, -1), float3(-1, -1, -1), float3(-1,  1, -1),
	float3( 1,  1,  0), float3( 1, -1,  0), float3(-1, -1,  0), float3(-1,  1,  0),
	float3( 1,  0,  1), float3(-1,  0,  1), float3( 1,  0, -1), float3(-1,  0, -1),
	float3( 0,  1,  1), float3( 0, -1,  1), float3( 0, -1, -1), float3( 0,  1, -1)
};

float CalcPointShadowFactor(Light light, float3 worldPosition, float3 normal, float2 screenPos)
{
	if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS) ||
		GetLightType(light.PackedFlags) != LIGHT_TYPE_POINT ||
		gLightingData.Shadow.HasPointShadow < 0.5f)
	{
		return 1.0f;
	}

	float3 toLight = gLightingData.Shadow.PointLightPos - worldPosition;
	float currentDist = length(toLight);
	float farZ = gLightingData.Shadow.PointLightRange;

	if (currentDist >= farZ || currentDist <= 0.001f)
	{
		return 1.0f;
	}

	float3 lightDir = toLight / currentDist;
	float cosTheta = saturate(dot(normal, lightDir));
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));
	// 비선형 원근 뎁스에 맞추어 기본 바이어스를 0.0008f로 정밀화
	float depthBias = 0.0008f + 0.0015f * sinTheta;
	float3 biasedWorldPos = worldPosition + normal * 0.005f;
	float3 dirFromLight = biasedWorldPos - gLightingData.Shadow.PointLightPos;
	float zView = max(abs(dirFromLight.x), max(abs(dirFromLight.y), abs(dirFromLight.z)));
	const float nearZ = 0.05f;
	if (zView <= nearZ)
	{
		return 1.0f;
	}
	float projZ = (farZ / (farZ - nearZ)) * (1.0f - (nearZ / zView));
	// 20-Tap PCF 필터링
	float diskRadius = (1.0f + (currentDist / farZ)) / 80.0f;
	float visibility = 0.0f;
	[unroll(20)]
	for (int i = 0; i < 20; ++i)
	{
		float3 sampleDir = dirFromLight + CUBE_SAMPLE_OFFSETS[i] * diskRadius;
		float closestDepth = gPointShadowDepth.Sample(gBufferSampler, sampleDir).r;
		visibility += (projZ - depthBias <= closestDepth) ? 1.0f : 0.0f;
	}
	return visibility / 20.0f;
}
float CalcContactShadow(float3 worldPosition, float3 normal, float3 lightDirection, float2 screenPos)
{
	const float maxDistance = 0.12f; // 최대 12cm (초근접 접촉부에 집중)
	const int stepCount = 16;        // 16스텝 -> 스텝당 약 7.5mm로 정밀화
	const float thickness = 0.04f;   // 차폐 판정 두께 (4cm)
	const float stepSize = maxDistance / float(stepCount);

	float dither = InterleavedGradientNoise(screenPos);
	// 시작 오프셋을 1.5mm로 줄여 접촉면 틈새 밀착
	float3 rayPos = worldPosition + normal * 0.0015f + lightDirection * (stepSize * dither);

	float shadow = 1.0f;

	[unroll(16)]
	for (int i = 0; i < stepCount; ++i)
	{
		rayPos += lightDirection * stepSize;

		float4 clipPos = mul(commonData.ViewProjection, float4(rayPos, 1.0f));
		if (clipPos.w <= 0.0f) break;

		float2 uv = clipPos.xy / clipPos.w * float2(0.5f, -0.5f) + 0.5f;
		if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) break;

		float3 surfacePos = gPosition.SampleLevel(gBufferSampler, uv, 0).rgb;
		if (length(surfacePos) < 0.0001f) continue;

		float rayDist = length(rayPos - commonData.CameraPos);
		float surfaceDist = length(surfacePos - commonData.CameraPos);
		float depthDiff = rayDist - surfaceDist;

		// 1mm 이상 파고들면 즉시 완벽한 접촉 그림자로 차폐
		if (depthDiff > 0.001f && depthDiff < thickness)
		{
			shadow = 0.0f;
			break;
		}
	}

	return shadow;
}

float3 CalcLight(Light light, float3 worldPosition, float3 normal, float3 viewDirection, float3 albedo, float roughness, float metallic, float3 f0)
{
	uint lightType = GetLightType(light.PackedFlags);
	bool affectsDiffuse = HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_DIFFUSE);
	bool affectsSpecular = HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_SPECULAR);
	float3 lightDirection;
	float attenuation;

	if (lightType == LIGHT_TYPE_DIRECTIONAL)
	{
		lightDirection = normalize(-light.Direction);
		attenuation = 1.0f;
	}
	else
	{
		float3 toLight = light.Position - worldPosition;
		float distanceToLight = length(toLight);
		if (distanceToLight < 0.0001f) return float3(0.0f, 0.0f, 0.0f);

		lightDirection = toLight / distanceToLight;
		attenuation = CalcAttenuation(light, distanceToLight);
		if (lightType == LIGHT_TYPE_SPOT)
		{
			attenuation *= CalcSpotFade(light, lightDirection);
		}
	}

	if (attenuation <= 0.0f) return float3(0.0f, 0.0f, 0.0f);

	float normalDotLight = max(dot(normal, lightDirection), 0.0f);
	if (normalDotLight <= 0.0f) return float3(0.0f, 0.0f, 0.0f);

	float3 halfVector = normalize(viewDirection + lightDirection);
	float normalDotView = max(dot(normal, viewDirection), 0.0f);
	float ndf = DistributionGGX(normal, halfVector, roughness);
	float geometry = GeometrySmith(normalDotView, normalDotLight, roughness);
	float3 fresnel = FresnelSchlick(max(dot(halfVector, viewDirection), 0.0f), f0);
	float3 specular = ndf * geometry * fresnel / max(4.0f * normalDotView * normalDotLight, 0.0001f);
	float3 diffuse = (1.0f - fresnel) * (1.0f - metallic) * albedo / PI;
	float3 radiance = light.Color * light.Intensity * attenuation * normalDotLight;
	float3 result = float3(0.0f, 0.0f, 0.0f);

	if (affectsDiffuse) result += diffuse * radiance;
	if (affectsSpecular) result += specular * radiance;
	return result;
}

float CalcCloudShadowFactor(float3 worldPosition)
{
	VolumetricCloudData cloud = gLightingData.Cloud;
	float extinctionCoeff = max(cloud.ExtinctionFactor, 0.0f) * 0.004f;

	if (cloud.EnableClouds == 0u || cloud.CloudCoverage <= 0.0f || cloud.CloudDensity <= 0.0f ||
		cloud.LayerHeight <= 0.0f || cloud.TracingMaxDistance <= 0.0f || extinctionCoeff <= 0.0f)
	{
		return 1.0f;
	}

	float planetRadius = gLightingData.PlanetRadius;
	float sunLength = length(gLightingData.SunDirection);
	if (planetRadius <= 0.0f || sunLength <= 0.0001f)
		return 1.0f;

	float3 sunDirection = gLightingData.SunDirection / sunLength;
	float layerBottom = planetRadius + cloud.LayerBottomAltitude;
	float layerTop = layerBottom + cloud.LayerHeight;

	float windLength = length(cloud.WindDirection);
	float3 windDirection = cloud.WindDirection / max(windLength, 0.0001f);
	float3 windOffset = windDirection * (commonData.Time * cloud.WindSpeed);

	// 월드 원점은 행성 표면에 있으므로 행성 중심 기준 좌표로 변환한다.
	float3 planetPosition = worldPosition + float3(0.0f, planetRadius, 0.0f);

	// 행성 표면에서 발생하는 교차 오차를 줄이는 작은 시작점 바이어스.
	planetPosition += normalize(planetPosition) * 2.0f;

	float2 transmittance = ComputeCloudSunTransmittance(
		planetPosition, sunDirection, planetRadius, layerBottom, layerTop, extinctionCoeff,
		cloud, windOffset, commonData.Time, gCloudBaseShape, gCloudWeatherMap, gCloudSampler);

	return transmittance.x;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    int3 pixelCoord = int3(input.Position.xy, 0);

    float4 diffuseSample = gDiffuse.Load(pixelCoord);
    if (diffuseSample.a < 0.01f)
        discard;

    float4 normalSample = gNormal.Load(pixelCoord);
    uint packedData = UnpackShadingData(normalSample.a);
    uint shadingModel = packedData & SHADING_MODEL_MASK;
    bool receiveShadow = (packedData & FLAG_RECEIVE_SHADOWS) != 0u;

    float3 albedo = diffuseSample.rgb;
    float3 emission = gEmission.Load(pixelCoord).rgb;

	// [진단 & Unlit 처리]: 스프라이트는 원본 텍스처 색상 그대로 출력
    if (shadingModel == SHADING_MODEL_UNLIT)
    {
		// [TEST]: 만약 분기 진입 여부를 눈으로 확인하고 싶다면 아래 주석을 켜보세요 (형광 초록으로 출력됨)
		// return float4(0.0f, 1.0f, 0.0f, 1.0f);

        return float4(albedo + emission, diffuseSample.a);
    }

    float3 normal = normalize(normalSample.rgb * 2.0f - 1.0f);
    float4 pbr = gPBR.Load(pixelCoord);
    float3 worldPosition = gPosition.Load(pixelCoord).rgb;
    float roughness = max(pbr.r, 0.04f);
    float metallic = pbr.g;
    float ambientOcclusion = pbr.a;
    float3 viewDirection = normalize(commonData.CameraPos - worldPosition);
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 directLighting = float3(0.0f, 0.0f, 0.0f);
    float3 skyAmbient = float3(0.0f, 0.0f, 0.0f);
    bool hasSkyLight = false;
    bool consumedShadowMap = false;
    uint numLights = gLightBuffer.Load(0u);

	uint cloudSunLightIndex = INVALID_LIGHT_INDEX;
	if (receiveShadow && gLightingData.Cloud.EnableClouds != 0u)
	{
		cloudSunLightIndex = FindAtmosphereSunLightIndex(numLights);
	}

    for (uint lightIndex = 0u; lightIndex < numLights; ++lightIndex)
    {
        Light light = LoadLight(lightIndex);
        if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_USE_IN_DEFERRED))
            continue;

        uint lightType = GetLightType(light.PackedFlags);
        if (lightType == LIGHT_TYPE_SKY)
        {
            hasSkyLight = true;
            if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_DIFFUSE))
                continue;

            float hemiT = saturate(dot(normal, normalize(light.Direction)) * 0.5f + 0.5f);
            float3 zenith = max(light.Color, float3(0.0f, 0.0f, 0.0f));
            float3 ground = max(light.AttenuationCoeff, float3(0.0f, 0.0f, 0.0f));
            float3 hemiColor = lerp(ground, zenith, hemiT);
            float indirect = max(light.Range, 0.0f) * max(light.Intensity, 0.0f);
            skyAmbient += hemiColor * albedo * indirect;
            continue;
        }

		float shadowFactor = 1.0f;
        if (receiveShadow && !consumedShadowMap && lightType == LIGHT_TYPE_DIRECTIONAL && HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
        {
            float3 lightDir = normalize(-light.Direction);
            float mainShadow = CalcShadowFactor(light, worldPosition, normal, input.Position.xy);
            float contactShadow = CalcContactShadow(worldPosition, normal, lightDir, input.Position.xy);
            shadowFactor = min(mainShadow, contactShadow);
            consumedShadowMap = true;
        }
        else if (receiveShadow && lightType == LIGHT_TYPE_SPOT && HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
        {
            shadowFactor = CalcSpotShadowFactor(light, worldPosition, normal, input.Position.xy);
        }
		else if (receiveShadow && lightType == LIGHT_TYPE_POINT && HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
        {
            shadowFactor = CalcPointShadowFactor(light, worldPosition, normal, input.Position.xy);
        }

        if (receiveShadow && lightIndex == cloudSunLightIndex)
        {
            shadowFactor *= CalcCloudShadowFactor(worldPosition);
        }

        directLighting += CalcLight(light, worldPosition, normal, viewDirection, albedo, roughness, metallic, f0) * shadowFactor;
    }

    float3 ambient = hasSkyLight ? skyAmbient * ambientOcclusion : float3(0.03f, 0.03f, 0.03f) * albedo * ambientOcclusion;
    float3 color = ambient + directLighting + emission;
    float4 fog = CalculateFog(worldPosition, commonData.CameraPos);
    color = lerp(color, fog.rgb, fog.a);
#if DEBUG_CASCADE_COLORS
    color *= GetCascadeDebugColor(worldPosition);
#endif
    color = color / (color + 1.0f);
    color = pow(color, 1.0f / 2.2f);
    return float4(color, diffuseSample.a);
}