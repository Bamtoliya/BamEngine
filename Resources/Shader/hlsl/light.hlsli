#define LIGHT_TYPE_POINT       0u
#define LIGHT_TYPE_DIRECTIONAL 1u
#define LIGHT_TYPE_SPOT        2u
#define LIGHT_TYPE_SKY         3u

#define ATTENUATION_COEFFICIENTS   0u
#define ATTENUATION_INVERSE_SQUARE 1u
#define ATTENUATION_DISABLED       2u

#define LIGHT_FLAG_CAST_SHADOWS    (1u << 0u)
#define LIGHT_FLAG_USE_IN_FORWARD  (1u << 1u)
#define LIGHT_FLAG_USE_IN_DEFERRED (1u << 2u)
#define LIGHT_FLAG_VOLUMETRIC      (1u << 3u)
#define LIGHT_FLAG_AFFECT_DIFFUSE  (1u << 4u)
#define LIGHT_FLAG_AFFECT_SPECULAR (1u << 5u)
#define LIGHT_FLAG_AFFECT_AMBIENT  (1u << 6u)
#define LIGHT_FLAG_AFFECT_EMISSIVE (1u << 7u)

struct Light
{
    float3  Position;
    float   Intensity;

    float3  Direction;
    float   Range;

    float3  Color;
    uint    PackedFlags;

    float3  AttenuationCoeff;
    float   Pad0;

    float   SpotInnerCos;
    float   SpotOuterCos;
    float   SpotFalloff;
    float   Pad1;
};

uint GetLightType(uint flags)
{
    return (flags >> 24u) & 0x3u;
}

uint GetAttenuationMode(uint flags)
{
    return (flags >> 26u) & 0x3u;
}

bool HasLightFlag(uint flags, uint bit)
{
    return (flags & bit) != 0u;
}

float CalcAttenuation(Light light, float dist)
{
    uint mode = GetAttenuationMode(light.PackedFlags);
    if (mode == ATTENUATION_DISABLED) return 1.0;

    float rangeFade = 1.0 - clamp(dist / max(light.Range, 0.0001), 0.0, 1.0);
    rangeFade *= rangeFade;

    if (mode == ATTENUATION_INVERSE_SQUARE)
        return rangeFade / max(dist * dist, 0.0001);

    // ATTENUATION_COEFFICIENTS
    float3  c     = light.AttenuationCoeff;
    float denom = c.x + c.y * dist + c.z * dist * dist;
    return rangeFade / max(denom, 0.0001);
}

float CalcSpotFade(Light light, float3 L)
{
    float cosTheta = dot(-L, normalize(light.Direction));
    float t = clamp(
        (cosTheta - light.SpotOuterCos) / max(light.SpotInnerCos - light.SpotOuterCos, 0.0001),
        0.0, 1.0
    );
    return pow(t, light.SpotFalloff);
}