#ifndef BAM_TONEMAPPING_HLSLI
#define BAM_TONEMAPPING_HLSLI

float3 ACESFilmic(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

#endif