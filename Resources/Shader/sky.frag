#version 450

#include "common.glsl"

layout(std140, set = 3, binding = 1) uniform SkyAtmosphereUBO
{
    vec3  ZenithColor;    float _pad0;
    vec3  HorizonColor;   float _pad1;
    vec3  GroundColor;    float _pad2;
    vec3  SunDirection;   float HorizonBlend;
    vec3  SunColor;       float SunIntensity;
    float SkyIntensity;   float SunSize;        float _pad3[2];
} sky;

layout(location = 0) in vec3 inLocalDir;

layout(location = 0) out vec4 outColor;

//layout(set = 2, binding = 0) uniform sampler2D texSampler;

vec3 SkyGradient(vec3 dir)
{
    // dir.y: +1 = 천정(Zenith), 0 = 지평선(Horizon), -1 = 지면(Ground)
    float y = dir.y;

    // 천정 ↔ 지평선 블렌드 (y: 0 ~ 1)
    float upperBlend  = smoothstep(0.0, sky.HorizonBlend, y);
    // 지평선 ↔ 지면 블렌드  (y: -1 ~ 0)
    float lowerBlend  = smoothstep(-sky.HorizonBlend, 0.0, y);

    vec3 upperSky = mix(sky.HorizonColor, sky.ZenithColor, upperBlend);
    vec3 finalSky = mix(sky.GroundColor,  upperSky,        lowerBlend);

    return finalSky;
}

vec3 SunDisc(vec3 dir, vec3 gradient)
{
    vec3  sunDir   = normalize(sky.SunDirection);
    float cosAngle = dot(normalize(dir), sunDir);

    // SunSize: 코사인 값 기준 (0.02 ≈ 약 1.1도 반경)
    float sunEdge  = 1.0 - sky.SunSize;
    float sunMask  = smoothstep(sunEdge - 0.002, sunEdge + 0.002, cosAngle);

    // 태양 주변 코로나 (부드러운 글로우)
    float corona   = pow(max(cosAngle, 0.0), 128.0) * 0.3;

    vec3  sunColor = sky.SunColor * sky.SunIntensity;
    return gradient + sunColor * sunMask + sky.SunColor * corona;
}

void main()
{
    vec3 dir      = normalize(inLocalDir);
    vec3 gradient = SkyGradient(dir);
    vec3 color    = SunDisc(dir, gradient);

    outColor = vec4(color * sky.SkyIntensity, 1.0);
    //outColor = vec4(0.2f, 0.2f, 1.0f, 1.0f);
}