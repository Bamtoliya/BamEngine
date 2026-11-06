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
    float   ShadowStrength;

    float   SpotInnerCos;
    float   SpotOuterCos;
    float   SpotFalloff;
    float   Pad1;
};

ByteAddressBuffer gLightBuffer : register(t6, space3);

static const uint LIGHT_BUFFER_HEADER_SIZE = 16u;
static const uint LIGHT_BUFFER_STRIDE = 80u;

Light LoadLight(uint lightIndex)
{
    const uint address = LIGHT_BUFFER_HEADER_SIZE + lightIndex * LIGHT_BUFFER_STRIDE;
    Light light;

    light.Position = asfloat(gLightBuffer.Load3(address));
    light.Intensity = asfloat(gLightBuffer.Load(address + 12u));
    light.Direction = asfloat(gLightBuffer.Load3(address + 16u));
    light.Range = asfloat(gLightBuffer.Load(address + 28u));
    light.Color = asfloat(gLightBuffer.Load3(address + 32u));
    light.PackedFlags = gLightBuffer.Load(address + 44u);
    light.AttenuationCoeff = asfloat(gLightBuffer.Load3(address + 48u));
    light.ShadowStrength = asfloat(gLightBuffer.Load(address + 60u));
    light.SpotInnerCos = asfloat(gLightBuffer.Load(address + 64u));
    light.SpotOuterCos = asfloat(gLightBuffer.Load(address + 68u));
    light.SpotFalloff = asfloat(gLightBuffer.Load(address + 72u));
    light.Pad1 = asfloat(gLightBuffer.Load(address + 76u));

    return light;
}

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