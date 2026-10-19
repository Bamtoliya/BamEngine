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
// 언리얼 적운(Cumulus) 전형의 높이별 그래디언트 (밑면 평평, 윗면 둥근 돔)
float CumulusHeightGradient(float h, float cloudTop)
{
    float baseCut = smoothstep(0.0f, 0.15f, h);
    float topRound = 1.0f - smoothstep(cloudTop * 0.65f, cloudTop, h);
    return baseCut * topRound;
}

// ── 3D 텍스처 및 2D 웨더 맵 기반 볼륨 밀도 샘플링 ──
float SampleCloudDensity(
    float3 pos, float layerBottom, float layerTop,
    VolumetricCloudData cloud, float3 windOffset, float time,
    Texture3D<float4> baseShapeTex, Texture2D<float4> weatherMapTex, SamplerState smp)
{
    float dist = length(pos);
    float h = saturate((dist - layerBottom) / (layerTop - layerBottom));
    if (h <= 0.005f || h >= 0.995f) return 0.0f;

    float3 relPos = float3(pos.x, dist - layerBottom, pos.z);

    // Weather, 형태, 디테일이 같은 바람을 따라 이동한다.
    float3 advectedPos = relPos - windOffset;
    float3 shapeCoord = advectedPos * cloud.ShapeScale;

    // 큰 형태를 직접 이동시키지 않고, 변형에 사용하는 저주파 필드를 변화시킨다.
    float phase = time * cloud.MorphSpeed;
    float3 evolutionCoord = shapeCoord * 0.25f + phase * float3(0.11f, 0.07f, 0.13f);
    float4 evolution = baseShapeTex.SampleLevel(smp, evolutionCoord, 0);

    float2 weatherUV = advectedPos.xz * cloud.WeatherScale;
    float4 weather = weatherMapTex.SampleLevel(smp, weatherUV, 0);

    // 공간마다 다른 커버리지 변화로 생성·소멸을 유도한다.
    float growth = (evolution.r - 0.5f) * 0.5f;
    float localCoverage = saturate(weather.r * (cloud.CloudCoverage * 1.6f) + growth);
    if (cloud.CloudCoverage <= 0.0f || localCoverage <= 0.04f) return 0.0f;

    float cloudTop = max(weather.g, 0.5f);
    float heightGrad = CumulusHeightGradient(h, cloudTop);
    if (heightGrad <= 0.001f) return 0.0f;

    // 위치마다 다른 변위로 윤곽을 변형한다.
    float3 warp = (evolution.gba - 0.5f) * 0.15f;
    float baseNoise = baseShapeTex.SampleLevel(smp, shapeCoord + warp, 0).r;

    float baseShape = baseNoise * heightGrad;
    float threshold = saturate(1.0f - localCoverage);
    float cloudShape = saturate((baseShape - threshold * 0.45f) / max(1.0f - threshold * 0.45f, 0.01f));
    if (cloudShape <= 0.0f) return 0.0f;

    float3 detailCoord = advectedPos * cloud.DetailScale + warp * 2.0f;
    float3 detailNoise = baseShapeTex.SampleLevel(smp, detailCoord, 0).gba;

    float worley = dot(detailNoise, float3(0.625f, 0.25f, 0.125f));
    cloudShape = saturate(cloudShape - (1.0f - worley) * 0.20f * (1.0f - cloudShape));

    return cloudShape * cloud.CloudDensity;
}

float2 ComputeCloudSunTransmittance(
    float3 pos, float3 sunDir, float planetRadius, float layerBottom, float layerTop, float extinctionCoeff,
    VolumetricCloudData cloud, float3 windOffset, float time,
    Texture3D<float4> baseShapeTex, Texture2D<float4> weatherMapTex, SamplerState smp)
{
    const float MULTIPLE_EXTINCTION_SCALE = 0.5f;

    float tPlanet0, tPlanet1;
    if (IntersectSphere(pos, sunDir, planetRadius, tPlanet0, tPlanet1) && tPlanet0 > 0.0f)
        return float2(0.0f, 0.0f);

    float tTop0, tTop1;
    if (!IntersectSphere(pos, sunDir, layerTop, tTop0, tTop1) || tTop1 <= 0.0f)
        return float2(1.0f, 1.0f);

    float tStart = max(tTop0, 0.0f);
    float firstEnd = tTop1;
    float secondStart = tTop1;

    float tBottom0, tBottom1;
    if (IntersectSphere(pos, sunDir, layerBottom, tBottom0, tBottom1) &&
        tBottom1 > tStart && tBottom0 < tTop1)
    {
        firstEnd = clamp(tBottom0, tStart, tTop1);
        secondStart = clamp(tBottom1, tStart, tTop1);
    }

    float firstLength = firstEnd - tStart;
    float pathLength = firstLength + (tTop1 - secondStart);
    if (pathLength <= 0.0f || extinctionCoeff <= 0.0f) return float2(1.0f, 1.0f);

    const float TARGET_LIGHT_STEP_LENGTH = 350.0f;
    const int MIN_LIGHT_STEPS = 4;
    const int MAX_LIGHT_STEPS = 8;

    int lightSteps = clamp(int(ceil(pathLength / TARGET_LIGHT_STEP_LENGTH)), MIN_LIGHT_STEPS, MAX_LIGHT_STEPS);
    float opticalDepth = 0.0f;

    for (int j = 0; j < lightSteps; ++j)
    {
        float u0 = float(j) / float(lightSteps);
        float u1 = float(j + 1) / float(lightSteps);
        float s0 = pathLength * u0 * u0;
        float s1 = pathLength * u1 * u1;
        float s = (s0 + s1) * 0.5f;

        float t = s < firstLength ? tStart + s : secondStart + (s - firstLength);
        float3 lightPos = pos + sunDir * t;
        float density = SampleCloudDensity(
            lightPos, layerBottom, layerTop, cloud, windOffset, time, baseShapeTex, weatherMapTex, smp);

        opticalDepth += density * extinctionCoeff * (s1 - s0);

        // 감쇠가 약한 다중 산란 항까지 충분히 작아졌을 때 종료한다.
        if (opticalDepth * MULTIPLE_EXTINCTION_SCALE >= 8.0f)
            return float2(0.0f, 0.0f);
    }

    // x: 단일 산란용 투과율, y: 다중 산란 근사용 투과율
    return exp(-opticalDepth * float2(1.0f, MULTIPLE_EXTINCTION_SCALE));
}

// ── 언리얼 엔진 규격의 볼류메트릭 클라우드 레이마칭 ──
float4 RenderCloudLayer(
    float3 rayOrigin, float3 rayDir, float3 sunDir, float3 sunColor, float sunIntensity, float skyIntensity,
    float time, float planetRadius, VolumetricCloudData cloud, float2 pixelPosition,
    bool hitPlanet, float tPlanet0,
    Texture3D<float4> baseShapeTex, Texture2D<float4> weatherMapTex, SamplerState smp)
{
    if (cloud.EnableClouds == 0u || cloud.CloudCoverage <= 0.0f || cloud.CloudDensity <= 0.0f ||
        cloud.LayerHeight <= 0.0f || cloud.TracingMaxDistance <= 0.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float layerBottom = planetRadius + cloud.LayerBottomAltitude;
    float layerTop = layerBottom + cloud.LayerHeight;

    // 바깥 구체에서 카메라 앞쪽에 존재하는 구간을 구한다.
    float tTop0, tTop1;
    if (!IntersectSphere(rayOrigin, rayDir, layerTop, tTop0, tTop1) || tTop1 <= 0.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float tStart = max(tTop0, 0.0f);
    float tEnd = tTop1;

    // 안쪽 구체를 제외하여 가장 가까운 연속 구름 구간을 선택한다.
    float tBottom0, tBottom1;
    if (IntersectSphere(rayOrigin, rayDir, layerBottom, tBottom0, tBottom1) && tBottom1 > tStart)
    {
        if (tBottom0 > tStart)
        {
            // 구름층 위 또는 내부에서 안쪽 구체로 들어가는 경우
            tEnd = min(tEnd, tBottom0);
        }
        else
        {
            // 구름층 아래에서 안쪽 구체를 빠져나오는 경우
            tStart = max(tStart, tBottom1);
        }
    }

    // 행성 표면 뒤에 있는 구름 구간을 제외한다.
    if (hitPlanet && tPlanet0 > 0.0f)
        tEnd = min(tEnd, tPlanet0);

    tEnd = min(tEnd, tStart + cloud.TracingMaxDistance);

    if (tEnd <= tStart)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    const float TARGET_STEP_LENGTH = 25.0f;
    const int MIN_MARCH_STEPS = 64;
    const int MAX_MARCH_STEPS = 192;

    float rayLength = tEnd - tStart;
    int marchSteps = clamp(int(ceil(rayLength / TARGET_STEP_LENGTH)), MIN_MARCH_STEPS, MAX_MARCH_STEPS);
    float stepSize = rayLength / float(marchSteps);
    float jitter = frac(52.9829189f * frac(dot(floor(pixelPosition), float2(0.06711056f, 0.00583715f))));

    float cosAngle = dot(rayDir, sunDir);
    float phase = lerp(HenyeyGreensteinPhase(cosAngle, 0.76f), HenyeyGreensteinPhase(cosAngle, -0.2f), 0.30f);
    float multiplePhase = lerp(HenyeyGreensteinPhase(cosAngle, 0.38f), HenyeyGreensteinPhase(cosAngle, -0.1f), 0.30f);
    const float MULTIPLE_SCATTERING_STRENGTH = 0.35f;

    float transmittance = 1.0f;
    float3 cloudScattering = float3(0.0f, 0.0f, 0.0f);

    float3 sunLight = sunColor * (sunIntensity * 24.0f) * cloud.CloudAlbedo;
    float3 ambientLight = float3(0.38f, 0.46f, 0.60f) * (skyIntensity * 0.75f) * cloud.CloudAlbedo;
    float extinctionCoeff = max(cloud.ExtinctionFactor, 0.0f) * 0.004f;

    // 바람 이동 벡터
    float windLength = length(cloud.WindDirection);
    float3 windDirection = cloud.WindDirection / max(windLength, 0.0001f);
    float3 windOffset = windDirection * (time * cloud.WindSpeed);

    for (int i = 0; i < marchSteps; ++i)
    {
          float t = tStart + (float(i) + jitter) * stepSize;
        float3 pos = rayOrigin + rayDir * t;

                float sampleDensity = SampleCloudDensity(pos, layerBottom, layerTop, cloud, windOffset, time, baseShapeTex, weatherMapTex, smp);
        if (sampleDensity <= 0.001f) continue;

        // 태양광 투과 레이마칭 (3 스텝)
        float2 lightTransmittance = ComputeCloudSunTransmittance(
            pos, sunDir, planetRadius, layerBottom, layerTop, extinctionCoeff,
            cloud, windOffset, time, baseShapeTex, weatherMapTex, smp);

        float powderSugar = 1.0f - exp(-sampleDensity * max(cloud.PowderExtinction, 0.0f) * 1.25f);
        float3 singleScattering = sunLight * (lightTransmittance.x * phase * (1.0f + powderSugar));

        float multipleWeight = MULTIPLE_SCATTERING_STRENGTH * (1.0f - lightTransmittance.x);
        float3 multipleScattering = sunLight * cloud.CloudAlbedo *
            (lightTransmittance.y * multiplePhase * multipleWeight);

        float3 stepLight = singleScattering + multipleScattering + ambientLight;
        // 산란광 및 투과율 누적 (Beer-Lambert Law)
        float stepExtinction = sampleDensity * extinctionCoeff * stepSize;
        float stepTransmittance = exp(-stepExtinction);
        cloudScattering += stepLight * (1.0f - stepTransmittance) * transmittance;
        transmittance *= stepTransmittance;

        if (transmittance < 0.01f) break;
    }

    float cloudAlpha = saturate(1.0f - transmittance);
    if (cloudAlpha <= 0.001f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 finalCloudColor = cloudScattering / max(cloudAlpha, 0.001f);
    return float4(finalCloudColor, cloudAlpha);
}

#endif // BAM_CLOUD_HLSLI