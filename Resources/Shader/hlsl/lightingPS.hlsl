#include "common.hlsli"
#include "light.hlsli"
#include "shading_flags.hlsli"
#include "fog.hlsli"

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

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPBR : register(t2, space3);
Texture2D gEmission : register(t3, space3);
Texture2D gPosition : register(t4, space3);
Texture2D gShadowDepth : register(t5, space3);
ByteAddressBuffer gLightBuffer : register(t6, space3);

SamplerState gBufferSampler : register(s0, space3);
ConstantBuffer<ShadowData> gShadowData : register(b1, space3);

static const float PI = 3.14159265359f;
static const uint LIGHT_BUFFER_HEADER_SIZE = 16u;
static const uint LIGHT_BUFFER_STRIDE = 80u;

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

float CalcShadowFactor(Light light, float3 worldPosition, float3 normal)
{
	if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS) ||
		GetLightType(light.PackedFlags) != LIGHT_TYPE_DIRECTIONAL)
	{
		return 1.0f;
	}

	float3 lightDirection = normalize(-light.Direction);
	float cosTheta = saturate(dot(normal, lightDirection));
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));
	float depthBias = gShadowData.BaseBias + gShadowData.SlopeBias * sinTheta;
	float3 biasedWorldPosition = worldPosition + normal * gShadowData.NormalBias;

	float4 lightClip = mul(float4(biasedWorldPosition, 1.0f), gShadowData.LightViewProjection);
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
	float visibility = 0.0f;

	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float closestDepth = gShadowDepth.Sample(gBufferSampler, projection.xy + float2(x, y) * texelSize).r;
			visibility += projection.z - depthBias <= closestDepth ? 1.0f : 0.0f;
		}
	}

	return visibility / 9.0f;
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

// Resources/Shader/hlsl/lightingPS.hlsl

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
        if (receiveShadow && !consumedShadowMap && HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
        {
            shadowFactor = CalcShadowFactor(light, worldPosition, normal);
            consumedShadowMap = true;
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