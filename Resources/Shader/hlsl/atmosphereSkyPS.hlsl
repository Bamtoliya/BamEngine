#include "common.hlsli"
#include "cloud.hlsli"

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float3 Direction    : TEXCOORD0;
};

// ── 3D 볼륨 텍스처 및 2D 웨더 맵 바인딩 (Material 영역: space1) ──
Texture3D<float4> baseShapeTex   : register(t0, space1);
Texture2D<float4> weatherMapTex  : register(t1, space1);
SamplerState      cloudSampler   : register(s0, space1);

struct SkyAtmosphereData
{
    float3 ZenithColor;     float _pad0;
    float3 HorizonColor;    float _pad1;
    float3 GroundColor;     float _pad2;
    float3 SunDirection;    float HorizonBlend;
    float3 SunColor;        float SunIntensity;
    float SkyIntensity;     float SunSize;      float2 _pad3;

    // CPU 구조체에 남아 있는 기존 구름 설정과 배치를 일치시킨다.
    float CloudCoverage;
    float CloudDensity;
    float CloudSpeed;
    float CloudAltitude;

    uint EnableClouds;
    float3 _pad4;

    VolumetricCloudData Cloud;
};
ConstantBuffer<SkyAtmosphereData> sky : register(b1, space3);

static const float PI = 3.14159265359f;
static const float PLANET_RADIUS = 6360000.0f;     // 6,360 km (지구 표면)
static const float ATMOS_RADIUS  = 6420000.0f;     // 6,420 km (대기권 상단: 60km 두께)
static const float HR = 8000.0f;                   // Rayleigh 스케일 높이 (8km)
static const float HM = 1200.0f;                   // Mie 스케일 높이 (1.2km)
static const float3 BETA_RAYLEIGH = float3(5.8e-6f, 13.5e-6f, 33.1e-6f); // 680nm, 550nm, 440nm
static const float3 BETA_MIE = float3(2.1e-5f, 2.1e-5f, 2.1e-5f);
static const float MIE_G = 0.78f;                  // Henyey-Greenstein 비대칭 인자

// 광선과 구체의 교차 거리 계산 (tMin, tMax)
bool RaySphereIntersect(float3 rayOrigin, float3 rayDir, float sphereRadius, out float t0, out float t1)
{
    float b = dot(rayOrigin, rayDir);
    float c = dot(rayOrigin, rayOrigin) - sphereRadius * sphereRadius;
    float d = b * b - c;
    if (d < 0.0f) { t0 = -1.0f; t1 = -1.0f; return false; }
    float sqrtD = sqrt(d);
    t0 = -b - sqrtD;
    t1 = -b + sqrtD;
    return true;
}

// Rayleigh 위상 함수
float PhaseRayleigh(float cosTheta)
{
    return (3.0f / (16.0f * PI)) * (1.0f + cosTheta * cosTheta);
}

// Mie (Henyey-Greenstein) 위상 함수
float PhaseMie(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cosTheta;
    return (3.0f / (8.0f * PI)) * ((1.0f - g2) * (1.0f + cosTheta * cosTheta)) /
        ((2.0f + g2) * max(denom * sqrt(max(denom, 0.0001f)), 0.0001f));
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 rayDir = normalize(input.Direction);

    // C++ 버퍼 미바인딩(0) 대비 Fallback 기본값 설정
    float3 sunDir = length(sky.SunDirection) > 0.001f ? normalize(sky.SunDirection) : normalize(float3(0.4f, 0.7f, -0.5f));
    float3 sunColor = length(sky.SunColor) > 0.001f ? sky.SunColor : float3(1.0f, 0.95f, 0.85f);
    float sunIntensity = sky.SunIntensity > 0.001f ? sky.SunIntensity : 2.5f;
    float skyIntensity = sky.SkyIntensity > 0.001f ? sky.SkyIntensity : 1.0f;
    float sunSize = sky.SunSize > 0.001f ? sky.SunSize : 0.035f;
    float3 groundColor = length(sky.GroundColor) > 0.001f ? sky.GroundColor : float3(0.12f, 0.10f, 0.09f);

    // 월드 좌표는 미터 단위이며, 월드 원점은 행성 표면에 놓인다.
    float3 planetCenter = float3(0.0f, -PLANET_RADIUS, 0.0f);

    // 구체 교차 함수는 중심이 원점인 좌표를 받으므로 행성 기준으로 변환한다.
    float3 rayOrigin = commonData.CameraPos - planetCenter;

    float tAtmos0, tAtmos1;
    if (!RaySphereIntersect(rayOrigin, rayDir, ATMOS_RADIUS, tAtmos0, tAtmos1) || tAtmos1 <= 0.0f)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    float tPlanet0;
    bool hitPlanet = GetPlanetGroundHitDistance(rayOrigin, rayDir, PLANET_RADIUS, tPlanet0);

    float tStart = max(tAtmos0, 0.0f);
    float tEnd = hitPlanet ? min(tAtmos1, tPlanet0) : tAtmos1;

    // 지표 내부 또는 표면에서 지표 안쪽을 보는 경우에는 추적하지 않는다.
    if (tEnd <= tStart)
        return float4(groundColor * skyIntensity * 0.2f, 1.0f);

    float rayLength = tEnd - tStart;

    const int VIEW_SAMPLES = 64;
    const int SUN_SAMPLES = 4;

    float3 opticalDepthRayleigh = 0.0f;
    float3 opticalDepthMie = 0.0f;
    float3 inScatteredLight = 0.0f;

    float cosAngle = dot(rayDir, sunDir);
    float phaseR = PhaseRayleigh(cosAngle);
    float phaseM = PhaseMie(cosAngle, MIE_G);
    const bool ENABLE_CLOUD_ATMOSPHERE_SHADOW = true;

    float cloudLayerBottom = PLANET_RADIUS + sky.Cloud.LayerBottomAltitude;
    float cloudLayerTop = cloudLayerBottom + sky.Cloud.LayerHeight;
    float cloudExtinctionCoeff = max(sky.Cloud.ExtinctionFactor, 0.0f) * 0.004f;

    bool cloudShadowEnabled = ENABLE_CLOUD_ATMOSPHERE_SHADOW &&
        sky.Cloud.EnableClouds != 0u && sky.Cloud.CloudCoverage > 0.0f && sky.Cloud.CloudDensity > 0.0f &&
        sky.Cloud.LayerHeight > 0.0f && sky.Cloud.TracingMaxDistance > 0.0f && cloudExtinctionCoeff > 0.0f;

    float cloudWindLength = length(sky.Cloud.WindDirection);
    float3 cloudWindDirection = sky.Cloud.WindDirection / max(cloudWindLength, 0.0001f);
    float3 cloudWindOffset = cloudWindDirection * (commonData.Time * sky.Cloud.WindSpeed);

    // 시선 광선을 따라 레이마칭
    for (int i = 0; i < VIEW_SAMPLES; ++i)
    {
        float u0 = float(i) / float(VIEW_SAMPLES);
        float u1 = float(i + 1) / float(VIEW_SAMPLES);
        float intervalStart = tStart + rayLength * u0 * u0;
        float intervalEnd = tStart + rayLength * u1 * u1;
        float stepSize = intervalEnd - intervalStart;
        float t = (intervalStart + intervalEnd) * 0.5f;
        float3 samplePos = rayOrigin + rayDir * t;
        float sampleHeight = max(length(samplePos) - PLANET_RADIUS, 0.0f);

        float densityR = exp(-sampleHeight / HR) * stepSize;
        float densityM = exp(-sampleHeight / HM) * stepSize;
        opticalDepthRayleigh += densityR;
        opticalDepthMie += densityM;

        float tSun0, tSun1;
        RaySphereIntersect(samplePos, sunDir, ATMOS_RADIUS, tSun0, tSun1);
        float groundSunDistance;
        bool inEarthShadow = GetPlanetGroundHitDistance(samplePos, sunDir, PLANET_RADIUS, groundSunDistance);

        if (!inEarthShadow)
        {
            float sunStepSize = max(tSun1, 0.0f) / float(SUN_SAMPLES);
            float sunOptDepthR = 0.0f;
            float sunOptDepthM = 0.0f;

            for (int j = 0; j < SUN_SAMPLES; ++j)
            {
                float3 sunSamplePos = samplePos + sunDir * ((float(j) + 0.5f) * sunStepSize);
                float sunHeight = max(length(sunSamplePos) - PLANET_RADIUS, 0.0f);
                sunOptDepthR += exp(-sunHeight / HR) * sunStepSize;
                sunOptDepthM += exp(-sunHeight / HM) * sunStepSize;
            }

            float3 totalExtinction = BETA_RAYLEIGH * (opticalDepthRayleigh + sunOptDepthR) +
                                     BETA_MIE * 1.11f * (opticalDepthMie + sunOptDepthM);
            float3 transmittance = exp(-totalExtinction);

            float cloudSunTransmittance = 1.0f;
            if (cloudShadowEnabled)
            {
                float2 cloudLightTransmittance = ComputeCloudSunTransmittance(
                    samplePos, sunDir, PLANET_RADIUS, cloudLayerBottom, cloudLayerTop, cloudExtinctionCoeff,
                    sky.Cloud, cloudWindOffset, commonData.Time, baseShapeTex, weatherMapTex, cloudSampler);

                cloudSunTransmittance = cloudLightTransmittance.x;
            }

            float3 scattering = densityR * BETA_RAYLEIGH * phaseR + densityM * BETA_MIE * phaseM;
            inScatteredLight += scattering * transmittance * cloudSunTransmittance;
        }
    }

    float3 sunRadiance = sunColor * sunIntensity * 20.0f;
    float3 color = inScatteredLight * sunRadiance;

    // 지표면에 닿았을 경우 대지 색상과 대기 감쇠 합성
    if (hitPlanet)
    {
        float3 groundExtinction = exp(-(BETA_RAYLEIGH * opticalDepthRayleigh + BETA_MIE * 1.11f * opticalDepthMie));
        float groundLight = saturate(dot(normalize(rayOrigin + rayDir * tPlanet0), sunDir));
        color += groundColor * groundLight * groundExtinction * 0.2f;
    }
    else
    {
        // 태양 원반 (Sun Disc)
        float sunEdge = 1.0f - sunSize * 0.25f;
        float sunMask = smoothstep(sunEdge - 0.001f, sunEdge + 0.001f, cosAngle);
        float3 sunTransmittance = exp(-(BETA_RAYLEIGH * opticalDepthRayleigh + BETA_MIE * 1.11f * opticalDepthMie));
        color += sunRadiance * sunMask * sunTransmittance * 0.05f;
    }

    color *= skyIntensity;

    float4 clouds = RenderCloudLayer(
        rayOrigin, rayDir, sunDir, sunColor, sunIntensity, skyIntensity, commonData.Time,
        PLANET_RADIUS, sky.Cloud, input.Position.xy, hitPlanet, tPlanet0,
        baseShapeTex, weatherMapTex, cloudSampler);

    color = lerp(color, clouds.rgb, clouds.a);

    // ACES 근사 톤매핑 (HDR -> LDR)
    float3 a = color * (color + 0.0245786f) - 0.000090537f;
    float3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
    color = saturate(a / b);

    return float4(color, 1.0f);
}