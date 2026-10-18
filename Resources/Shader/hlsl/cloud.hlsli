#ifndef BAM_CLOUD_HLSLI
#define BAM_CLOUD_HLSLI

struct VolumetricCloudData
{
    float LayerBottomAltitude;
    float LayerHeight;
    float TracingMaxDistance;
    float CloudCoverage;

    float CloudDensity;
    float WeatherScale;
    float ShapeScale;
    float DetailScale;

    float3 WindDirection;
    float WindSpeed;
    float3 CloudAlbedo;
    float ExtinctionFactor;

    float MorphSpeed;
    float PowderExtinction;
    uint EnableClouds;
    float _pad0;
    
    float4 _pad1[3];
};

static const float CLOUD_PI = 3.14159265359f;

bool IntersectSphere(float3 rayOrigin, float3 rayDir, float sphereRadius, out float t0, out float t1)
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

float HenyeyGreensteinPhase(float cosAngle, float g)
{
    float g2 = g * g;
    return (1.0f - g2) / (4.0f * CLOUD_PI * pow(max(1.0f + g2 - 2.0f * g * cosAngle, 0.0001f), 1.5f));
}

// 언리얼 적운(Cumulus) 전형의 높이별 그래디언트
float CumulusHeightGradient(float h, float cloudTop)
{
    float baseCut = smoothstep(0.0f, 0.12f, h);
    float topRound = smoothstep(cloudTop, cloudTop * 0.70f, h);
    return baseCut * topRound;
}

// ── 3D 텍스처 및 2D 웨더 맵 기반 초고속 볼륨 밀도 샘플링 ──
float SampleCloudDensity(
    float3 pos, float layerBottom, float layerTop,
    VolumetricCloudData cloud, float3 windOffset,
    Texture3D<float4> baseShapeTex, Texture2D<float4> weatherMapTex, SamplerState smp)
{
    float dist = length(pos);
    float h = saturate((dist - layerBottom) / (layerTop - layerBottom));
    if (h <= 0.005f || h >= 0.995f) return 0.0f;

    // 1. 2D Weather Map 샘플링 (거시적 구름 군집과 시원한 빈 하늘의 분포)
    float2 weatherUV = (pos.xz + windOffset.xz * 0.2f) * cloud.WeatherScale;
    float4 weather = weatherMapTex.SampleLevel(smp, weatherUV, 0);

    float localCoverage = weather.r * cloud.CloudCoverage;
    if (localCoverage <= 0.02f) return 0.0f; // 맑은 하늘 구역은 즉시 스킵!

    // 2. 적운 높이 그래디언트 (웨더 맵 G 채널로 구름 꼭대기 고도 조절)
    float cloudTop = max(weather.g, 0.45f);
    float heightGrad = CumulusHeightGradient(h, cloudTop);
    if (heightGrad <= 0.001f) return 0.0f;

    // 3. 3D Base Shape 텍스처 샘플링 (하드웨어 삼선형 보간)
    // R: Perlin-Worley 기본 덩어리, GBA: Worley 거품 디테일
    float3 shapeCoord = (pos + windOffset) * cloud.ShapeScale;
    float4 baseNoise = baseShapeTex.SampleLevel(smp, shapeCoord, 0);

    // 언리얼 방식: Base Shape(R)에 고주파 Worley 거품(G, B, A)을 에로전 침식
    float worleyErosion = baseNoise.g * 0.625f + baseNoise.b * 0.25f + baseNoise.a * 0.125f;
    float cloudVal = baseNoise.r - (1.0f - worleyErosion) * 0.35f;

    // 4. 로컬 커버리지와 임계값 결합
    float threshold = saturate(1.0f - localCoverage * 1.35f);
    float cloudShape = saturate((cloudVal - threshold) / max(1.0f - threshold, 0.001f));

    return cloudShape * heightGrad * cloud.CloudDensity * weather.b;
}

// ── 언리얼 엔진 규격의 볼류메트릭 클라우드 레이마칭 ──
float4 RenderCloudLayer(
    float3 rayOrigin, float3 rayDir, float3 sunDir, float3 sunColor, float sunIntensity, float skyIntensity,
    float time, float planetRadius, VolumetricCloudData cloud,
    bool hitPlanet, float tPlanet0,
    Texture3D<float4> baseShapeTex, Texture2D<float4> weatherMapTex, SamplerState smp)
{
    if (cloud.EnableClouds == 0u || cloud.CloudCoverage <= 0.01f || cloud.CloudDensity <= 0.001f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float layerBottom = planetRadius + cloud.LayerBottomAltitude;
    float layerTop    = layerBottom + cloud.LayerHeight;

    float tBottom0, tBottom1;
    float tTop0, tTop1;
    bool hitBottom = IntersectSphere(rayOrigin, rayDir, layerBottom, tBottom0, tBottom1);
    bool hitTop    = IntersectSphere(rayOrigin, rayDir, layerTop, tTop0, tTop1);

    float tStart = -1.0f;
    float tEnd   = -1.0f;
    float camDist = length(rayOrigin);

    if (camDist < layerBottom) // 지상 관측자
    {
        if (rayDir.y <= 0.01f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (!hitBottom || !hitTop) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        tStart = max(tBottom1, 0.0f);
        tEnd   = max(tTop1, 0.0f);
    }
    else if (camDist > layerTop) // 우주/고고도 관측자
    {
        if (!hitTop || tTop0 <= 0.0f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        tStart = tTop0;
        tEnd = (hitBottom && tBottom0 > 0.0f) ? tBottom0 : tTop1;
    }
    else // 구름층 내부
    {
        tStart = 0.0f;
        tEnd = (hitTop && tTop1 > 0.0f) ? tTop1 : 10000.0f;
    }

    if (tStart >= tEnd || tEnd <= 0.0f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    if (hitPlanet && tPlanet0 > 0.0f)
    {
        if (tStart > tPlanet0) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        tEnd = min(tEnd, tPlanet0);
    }

    // 최대 추적 거리 제한
    tEnd = min(tEnd, tStart + cloud.TracingMaxDistance);

    const int MARCH_STEPS = 24; // 하드웨어 텍스처 가속으로 스텝 수를 24로 상향!
    float rayLength = tEnd - tStart;
    float stepSize = rayLength / float(MARCH_STEPS);

    float cosAngle = dot(rayDir, sunDir);
    float phase = lerp(HenyeyGreensteinPhase(cosAngle, 0.76f), HenyeyGreensteinPhase(cosAngle, -0.2f), 0.30f);

    float transmittance = 1.0f;
    float3 cloudScattering = float3(0.0f, 0.0f, 0.0f);

    float3 sunLight = sunColor * (sunIntensity * 24.0f) * cloud.CloudAlbedo;
    float3 ambientLight = float3(0.38f, 0.46f, 0.60f) * (skyIntensity * 4.5f) * cloud.CloudAlbedo;
    float extinctionCoeff = cloud.CloudDensity * cloud.ExtinctionFactor * 0.015f;

    // 바람 이동 벡터
    float3 windOffset = cloud.WindDirection * (time * cloud.WindSpeed);

    for (int i = 0; i < MARCH_STEPS; ++i)
    {
        float t = tStart + (float(i) + 0.5f) * stepSize;
        float3 pos = rayOrigin + rayDir * t;

        float sampleDensity = SampleCloudDensity(pos, layerBottom, layerTop, cloud, windOffset, baseShapeTex, weatherMapTex, smp);
        if (sampleDensity <= 0.001f) continue;

        // 태양광 투과 레이마칭 (3 스텝)
        float lightOptDepth = 0.0f;
        float lightStepSize = 250.0f;
        for (int j = 0; j < 3; ++j)
        {
            float3 lPos = pos + sunDir * ((float(j) + 0.5f) * lightStepSize);
            lightOptDepth += SampleCloudDensity(lPos, layerBottom, layerTop, cloud, windOffset, baseShapeTex, weatherMapTex, smp) * lightStepSize;
        }

        float lightTransmittance = exp(-lightOptDepth * extinctionCoeff * 1.6f);
        float powderSugar = 1.0f - exp(-sampleDensity * stepSize * cloud.PowderExtinction * 0.05f);
        float3 stepLight = sunLight * (lightTransmittance * phase * (1.0f + powderSugar)) + ambientLight * (1.0f - lightTransmittance * 0.5f);

        // 산란광 및 투과율 누적
        float stepExtinction = sampleDensity * extinctionCoeff * stepSize;
        float stepTransmittance = exp(-stepExtinction);
        cloudScattering += stepLight * (sampleDensity * (1.0f - stepTransmittance)) * transmittance;
        transmittance *= stepTransmittance;

        if (transmittance < 0.01f) break;
    }

    float cloudAlpha = saturate(1.0f - transmittance);
    if (cloudAlpha <= 0.001f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 finalCloudColor = cloudScattering / max(cloudAlpha, 0.001f);
    return float4(finalCloudColor, cloudAlpha);
}

#endif // BAM_CLOUD_HLSLI