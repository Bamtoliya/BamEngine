#version 450

#include "common.glsl"
#include "light.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D gDiffuse;
layout(set = 2, binding = 1) uniform sampler2D gNormal;
layout(set = 2, binding = 2) uniform sampler2D gPBR;
layout(set = 2, binding = 3) uniform sampler2D gEmission;
layout(set = 2, binding = 4) uniform sampler2D gPosition;
layout(set = 2, binding = 5) uniform sampler2D gShadowDepth;

layout(set = 2, binding = 6) readonly buffer LightBuffer
{
    uint     NumLights;
    uint     Padding0;
    uint     Padding1;
    uint     Padding2;
    GPULight Lights[];
} gLightBuffer;

layout(std140, set = 3, binding = 1) uniform ShadowData
{
    mat4 lightViewProjection;
    float baseBias;
    float slopeBias;
    float normalBias;
    float _pad;
} gShadowData;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a2    = roughness * roughness * roughness * roughness;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float k  = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float CalcShadowFactor(GPULight light, vec3 worldPos, vec3 N)
{
    if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
        return 1.0;

    if (GetLightType(light.PackedFlags) != LIGHT_TYPE_DIRECTIONAL)
        return 1.0;

    vec3 L = normalize(-light.Direction);

    // 1) 각도 기반 bias 계산
    float cosTheta = clamp(dot(N, L), 0.0, 1.0);
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float depthBias = gShadowData.baseBias + gShadowData.slopeBias * sinTheta;

    // 2) Receiver normal offset (world-space)
    // Plane acne 완화에 특히 효과적
    float receiverOffset = gShadowData.normalBias;
    vec3 biasedWorldPos = worldPos + N * receiverOffset;

    // 3) Shadow map projection
    vec4 lightClip = gShadowData.lightViewProjection * vec4(biasedWorldPos, 1.0);
    vec3 proj = lightClip.xyz / max(lightClip.w, 0.0001);
    proj.xy = proj.xy * 0.5 + 0.5;
    proj.y  = 1.0 - proj.y;   // Vulkan NDC Y-flip
    proj.z  = proj.z * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;

    vec2 texelSize = 1.0 / vec2(textureSize(gShadowDepth, 0));

    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(gShadowDepth, proj.xy + vec2(x, y) * texelSize).r;
            visibility += (proj.z - depthBias <= closestDepth) ? 1.0 : 0.0;
        }
    }

    return visibility / 9.0;
}

vec3 CalcLight(GPULight light, vec3 worldPos, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, vec3 F0)
{
    uint ltype = GetLightType(light.PackedFlags);
    bool affD  = HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_DIFFUSE);
    bool affS  = HasLightFlag(light.PackedFlags, LIGHT_FLAG_AFFECT_SPECULAR);

    vec3  L;
    float attenuation;

    if (ltype == LIGHT_TYPE_DIRECTIONAL)
    {
        L           = normalize(-light.Direction);
        attenuation = 1.0;
    }
    else
    {
        vec3 toLight = light.Position - worldPos;
        float dist = length(toLight);
        if (dist < 0.0001) return vec3(0.0);
        L = toLight / dist;
        attenuation = CalcAttenuation(light, dist);
        if (ltype == LIGHT_TYPE_SPOT)
            attenuation *= CalcSpotFade(light, L);
    }

    if (attenuation <= 0.0) return vec3(0.0);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    vec3 radiance = light.Color * light.Intensity * attenuation * NdotL;

    vec3 result = vec3(0.0);
    if (affD) result += diffuse * radiance;
    if (affS) result += specular * radiance;
    return result;
}

void main()
{
    vec4 diffuseSample = texture(gDiffuse, inTexCoord);

    if(diffuseSample.a < 0.1)
        discard;

    vec3 N = normalize(texture(gNormal, inTexCoord).rgb * 2.0 - 1.0);
    vec4 pbr = texture(gPBR, inTexCoord);
    vec3 emission = texture(gEmission, inTexCoord).rgb;
    vec3 worldPos = texture(gPosition, inTexCoord).rgb;

    float roughness = max(pbr.r, 0.04);
    float metallic = pbr.g;
    float ao = pbr.a;
    vec3 albedo = diffuseSample.rgb;

    vec3 V = normalize(commonFragdata.cameraPos - worldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    bool consumedShadowMap = false;

    for (uint i = 0u; i < gLightBuffer.NumLights; ++i)
    {
        GPULight light = gLightBuffer.Lights[i];
        if (!HasLightFlag(light.PackedFlags, LIGHT_FLAG_USE_IN_DEFERRED))
            continue;

        float shadowFactor = 1.0;
        if (!consumedShadowMap && HasLightFlag(light.PackedFlags, LIGHT_FLAG_CAST_SHADOWS))
        {
            shadowFactor = CalcShadowFactor(light, worldPos, N);
            consumedShadowMap = true;
        }

        Lo += CalcLight(light, worldPos, N, V, albedo, roughness, metallic, F0) * shadowFactor;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo + emission;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, diffuseSample.a);
}