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
	float4x4 LightViewProjection;
	float BaseBias;
	float SlopeBias;
	float NormalBias;
	float _pad;
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

bool FindBlocker(float3 projCoord, float depthBias, float2 searchRadius, float2x2 rot, out float avgBlockerDepth)
{
	int blockerCount = 0;
	float blockerDepthSum = 0.0f;

	// 8개 샘플로 주변 차폐물(Blocker) 평균 깊이 탐색
	for (int i = 0; i < 8; ++i)
	{
		float2 offset = mul(rot, VOGEL_DISK_16[i * 2]) * searchRadius;
		float sampleDepth = gShadowDepth.SampleLevel(gBufferSampler, projCoord.xy + offset, 0).r;
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

	float4 lightClip = mul(gLightingData.Shadow.LightViewProjection, float4(biasedWorldPosition, 1.0f));
	float3 projection = lightClip.xyz / max(lightClip.w, 0.0001f);
	projection.xy = projection.xy * 0.5f + 0.5f;
	projection.y = 1.0f - projection.y;

	if (projection.x < 0.0f || projection.x > 1.0f || projection.y < 0.0f || projection.y > 1.0f || projection.z > 1.0f)
	{
		return 1.0f;
	}

	uint shadowWidth;
	uint shadowHeight;
	gShadowDepth.GetDimensions(shadowWidth, shadowHeight);
	float2 texelSize = 1.0f / float2(shadowWidth, shadowHeight);

	// 픽셀별 무작위 회전각 계산
	float randomAngle = InterleavedGradientNoise(screenPos) * 6.2831853f;
	float sinA, cosA;
	sincos(randomAngle, sinA, cosA);
	float2x2 rot = float2x2(cosA, -sinA, sinA, cosA);

	// [PCSS 1단계: Blocker Search]
	const float lightSizeUV = 0.035f; // 광원의 겉보기 크기 (반그림자 확산 계수)
	float2 searchRadius = texelSize * 8.0f;
	float avgBlockerDepth = 0.0f;

	if (!FindBlocker(projection, depthBias, searchRadius, rot, avgBlockerDepth))
	{
		return 1.0f; // 차폐물이 전혀 없음 -> 즉각 조기 종료 (GPU 절약)
	}

	// [PCSS 2단계: Penumbra Estimation (차폐물-수광부 거리에 따른 반경 결정)]
	float penumbraRatio = saturate((projection.z - avgBlockerDepth) / max(avgBlockerDepth, 0.0001f));
	float filterRadiusScale = clamp(penumbraRatio * lightSizeUV, texelSize.x * 1.2f, texelSize.x * 10.0f);
	float2 filterRadius = float2(filterRadiusScale, filterRadiusScale);

	// [PCSS 3단계: Filtering (거리 맞춤형 16-Tap Vogel Disk PCF)]
	float visibility = 0.0f;
	for (int i = 0; i < 16; ++i)
	{
		float2 sampleOffset = mul(rot, VOGEL_DISK_16[i]) * filterRadius;
		float closestDepth = gShadowDepth.Sample(gBufferSampler, projection.xy + sampleOffset).r;
		visibility += (projection.z - depthBias <= closestDepth) ? 1.0f : 0.0f;
	}

	return visibility / 16.0f;
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

    color = color / (color + 1.0f);
    color = pow(color, 1.0f / 2.2f);

    return float4(color, diffuseSample.a);
}