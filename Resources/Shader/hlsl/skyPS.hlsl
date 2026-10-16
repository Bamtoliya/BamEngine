#include "common.hlsli"

struct PS_INPUT
{
    float3 Direction    : TEXCOORD1;
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
    // 천정 ↔ 지평선 블렌드 (y: 0 ~ 1)
    float upperBlend = smoothstep(0.0f, sky.HorizonBlend, direction.y);

    // 지평선 ↔ 지면 블렌드  (y: -1 ~ 0)
    float lowerBlend = smoothstep(-sky.HorizonBlend, 0.0f, direction.y);
    
    float3 upperSky = lerp(sky.HorizonColor, sky.ZenithColor, upperBlend);

    return lerp(sky.GroundColor, upperSky, lowerBlend);
}

float3 SunDisc(float3 direction, float3 gradient)
{
    float3 sunDirection = normalize(sky.SunDirection);
    float cosAngle = dot(normalize(direction), sunDirection);

    // SunSize: 코사인 값 기준 (0.02 ≈ 약 1.1도 반경)
    float sunEdge = 1.0f - sky.SunSize;
    float sunMask = smoothstep(sunEdge - 0.002f, sunEdge + 0.002f, cosAngle);

    // 태양 주변 코로나 (부드러운 글로우)
    float corona = pow(max(cosAngle, 0.0f), 128.0f) * 0.3f;
    float3 sunColor = sky.SunColor * sky.SunIntensity;

    return gradient + sunColor * sunMask + sky.SunColor * corona;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 direction = normalize(input.Direction);
    float3 gradient = SkyGradient(direction);
    float3 color = SunDisc(direction, gradient);

    return float4(color * sky.SkyIntensity, 1.0f);
}