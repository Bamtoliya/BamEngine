#ifndef BAM_CLOUD_HLSLI
#define BAM_CLOUD_HLSLI

// 광선과 구름층 구체 교차 거리 계산
bool IntersectCloudSphere(float3 rayOrigin, float3 rayDir, float sphereRadius, out float t0, out float t1)
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

// 3D 절차적 해시 및 노이즈
float CloudHash(float3 p)
{
    p = frac(p * 0.3183099f + 0.1f);
    p *= 17.0f;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float CloudNoise3D(float3 x)
{
    float3 p = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(
        lerp(lerp(CloudHash(p + float3(0,0,0)), CloudHash(p + float3(1,0,0)), f.x),
             lerp(CloudHash(p + float3(0,1,0)), CloudHash(p + float3(1,1,0)), f.x), f.y),
        lerp(lerp(CloudHash(p + float3(0,0,1)), CloudHash(p + float3(1,0,1)), f.x),
             lerp(CloudHash(p + float3(0,1,1)), CloudHash(p + float3(1,1,1)), f.x), f.y), f.z);
}

// 프랙탈 브라운 운동 (4-Octave fBM)
float CloudFBM(float3 p)
{
    float f = 0.0f;
    f += 0.5000f * CloudNoise3D(p); p *= 2.02f;
    f += 0.2500f * CloudNoise3D(p); p *= 2.03f;
    f += 0.1250f * CloudNoise3D(p); p *= 2.01f;
    f += 0.0625f * CloudNoise3D(p);
    return f;
}

// 구형 구름 층 렌더링 함수
float4 RenderCloudLayer(
    float3 rayOrigin, float3 rayDir, float3 sunDir, float3 sunColor,
    float time, float planetRadius, float cloudAltitude,
    float coverage, float density, float speed, uint enabled)
{
    if (enabled == 0u || coverage <= 0.01f || density <= 0.001f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float cloudRadius = planetRadius + cloudAltitude;
    float t0, t1;
    if (!IntersectCloudSphere(rayOrigin, rayDir, cloudRadius, t0, t1))
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 카메라 위치(지상: 내부 -> t1, 우주: 외부 -> t0) 판정
    float tCloud = (length(rayOrigin) < cloudRadius) ? max(t1, 0.0f) : max(t0, 0.0f);
    if (tCloud <= 0.0f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 cloudPos = rayOrigin + rayDir * tCloud;
    float3 normCloudPos = normalize(cloudPos);

    // 바람 이동 애니메이션
    float3 wind = float3(time * speed * 0.05f, 0.0f, time * speed * 0.02f);
    float3 samplePos = normCloudPos * 35.0f + wind;

    // fBM 노이즈 기반 구름 밀도 계산
    float noise = CloudFBM(samplePos);
    float cloudThreshold = 1.0f - coverage;
    float cloudAlpha = smoothstep(cloudThreshold, cloudThreshold + 0.25f, noise) * density;
    if (cloudAlpha <= 0.001f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 태양광 전방 산란 (Silver Lining) 및 음영
    float cosAngle = dot(rayDir, sunDir);
    float silverLining = pow(saturate(cosAngle * 0.5f + 0.5f), 4.0f) * 1.5f;
    float sunTerminator = saturate(dot(normCloudPos, sunDir) * 0.8f + 0.2f);

    // 일몰 노을 색상 연동
    float3 cloudSunColor = lerp(float3(1.0f, 0.6f, 0.3f), sunColor, saturate(sunDir.y * 3.0f));
    float3 cloudColor = lerp(float3(0.35f, 0.4f, 0.5f), cloudSunColor * (1.0f + silverLining), sunTerminator);

    return float4(cloudColor, cloudAlpha);
}

#endif // BAM_CLOUD_HLSLI