#include "common.hlsli"

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float3 Direction    : TEXCOORD0;
};

struct SkyAtmosphereData
{
    float3 ZenithColor;     float _pad0;
    float3 HorizonColor;    float _pad1;
    float3 GroundColor;     float _pad2;
    float3 SunDirection;    float HorizonBlend;
    float3 SunColor;        float SunIntensity;
    float SkyIntensity;     float SunSize;      float2 _pad3;
};

ConstantBuffer<SkyAtmosphereData> sky : register(b1, space3);

float3 SkyGradient(float3 direction)
{
    float3 zenithColor = length(sky.ZenithColor) > 0.001f ? sky.ZenithColor : float3(0.18f, 0.42f, 0.85f);
    float3 horizonColor = length(sky.HorizonColor) > 0.001f ? sky.HorizonColor : float3(0.70f, 0.82f, 0.95f);
    float3 groundColor = length(sky.GroundColor) > 0.001f ? sky.GroundColor : float3(0.15f, 0.13f, 0.12f);
    float horizonBlend = sky.HorizonBlend > 0.001f ? sky.HorizonBlend : 0.35f;

    // 천정 ↔ 지평선 블렌드 (y: 0 ~ 1)
    float upperBlend = smoothstep(0.0f, horizonBlend, direction.y);

    // 지평선 ↔ 지면 블렌드  (y: -1 ~ 0)
    float lowerBlend = smoothstep(-horizonBlend, 0.0f, direction.y);
    
    float3 upperSky = lerp(horizonColor, zenithColor, upperBlend);
    return lerp(groundColor, upperSky, lowerBlend);
}

float3 SunDisc(float3 direction, float3 gradient)
{
    float3 sunDirection = length(sky.SunDirection) > 0.001f ? normalize(sky.SunDirection) : normalize(float3(0.4f, 0.7f, -0.5f));
    float3 sunColor = length(sky.SunColor) > 0.001f ? sky.SunColor : float3(1.0f, 0.95f, 0.85f);
    float sunIntensity = sky.SunIntensity > 0.001f ? sky.SunIntensity : 2.0f;
    float sunSize = sky.SunSize > 0.001f ? sky.SunSize : 0.035f;

    float cosAngle = dot(normalize(direction), sunDirection);

    float sunEdge = 1.0f - sunSize;
    float sunMask = smoothstep(sunEdge - 0.002f, sunEdge + 0.002f, cosAngle);

    // 태양 코로나 글로우
    float corona = pow(max(cosAngle, 0.0f), 64.0f) * 0.4f;

    return gradient + sunColor * sunIntensity * sunMask + sunColor * corona;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float skyIntensity = sky.SkyIntensity > 0.001f ? sky.SkyIntensity : 1.0f;

    float3 direction = normalize(input.Direction);
    float3 gradient = SkyGradient(direction);
    float3 color = SunDisc(direction, gradient);

    return float4(color * skyIntensity, 1.0f);
}