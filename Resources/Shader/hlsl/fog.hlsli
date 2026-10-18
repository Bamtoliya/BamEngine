#ifndef BAM_FOG_HLSLI
#define BAM_FOG_HLSLI

struct FogData
{
    float3 FogColor;
    float FogDensity;
    float FogHeight;
    float FogHeightFalloff;
    float FogStartDistance;
    float FogEndDistance;
    uint FogType;
    float FogMaxOpacity;
    uint FogEnabled;
    float _pad;
};

ConstantBuffer<FogData> gFogData : register(b0, space3);

float4 CalculateFog(float3 worldPosition, float3 cameraPosition)
{
    if (gFogData.FogEnabled == 0u)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 ray = worldPosition - cameraPosition;
    float rayLength = length(ray);
    if (rayLength <= gFogData.FogStartDistance)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float fogFactor = 0.0f;

	// 0: Linear (선형 거리 안개)
    if (gFogData.FogType == 0u)
    {
        float range = max(gFogData.FogEndDistance - gFogData.FogStartDistance, 0.001f);
        fogFactor = saturate((rayLength - gFogData.FogStartDistance) / range);
    }
	// 1: Exponential (지수 거리 안개)
    else if (gFogData.FogType == 1u)
    {
        float d = rayLength - gFogData.FogStartDistance;
        fogFactor = saturate(1.0f - exp(-d * gFogData.FogDensity));
    }
	// 2: ExponentialHeight (지수 높이 안개 - 고도 선적분)
    else if (gFogData.FogType == 2u)
    {
        float effectiveDist = rayLength - gFogData.FogStartDistance;
        float falloff = max(gFogData.FogHeightFalloff, 0.001f);
        float cameraHeightRel = cameraPosition.y - gFogData.FogHeight;
        float deltaY = ray.y * (effectiveDist / rayLength);

        float opticalDepth;
        if (abs(deltaY) > 0.001f)
        {
            opticalDepth = gFogData.FogDensity * exp(-falloff * cameraHeightRel) *
				(1.0f - exp(-falloff * deltaY)) / (falloff * deltaY) * effectiveDist;
        }
        else
        {
            opticalDepth = gFogData.FogDensity * exp(-falloff * cameraHeightRel) * effectiveDist;
        }
        fogFactor = saturate(1.0f - exp(-max(opticalDepth, 0.0f)));
    }

    fogFactor *= gFogData.FogMaxOpacity;
    return float4(gFogData.FogColor, fogFactor);
}

#endif // BAM_FOG_HLSLI