#include "common.hlsli"
#include "matrix.hlsli"

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 NDC : TEXCOORD0;
};

float4 Grid2D(float2 coordinate, float scale, float thickness)
{
    float2 scaledCoordinate = coordinate * scale;
    float2 derivative = fwidth(scaledCoordinate);
    float2 grid = abs(frac(scaledCoordinate - 0.5f) - 0.5f) / derivative;
    float lineDistance = min(grid.x, grid.y);
    float alpha = 1.0f - smoothstep(thickness - 1.0f, thickness + 1.0f, lineDistance);
    float lodFade = 1.0f - smoothstep(0.1f, 0.8f, length(derivative));
    return float4(0.4f, 0.4f, 0.4f, alpha * 0.5f * lodFade);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float4x4 inverseViewProjection = commonData.InvViewProjection;
    float4 worldPosition = mul(commonData.InvViewProjection, float4(input.NDC, 0.0f, 1.0f));
    float2 coordinate = worldPosition.xy / worldPosition.w;
    float4 grid1 = Grid2D(coordinate, 1.0f, 1.0f);
    float4 grid10 = Grid2D(coordinate, 0.1f, 1.5f);
    float4 color = lerp(grid1, grid10, grid10.a);
    float2 derivative = fwidth(coordinate);
    float2 axisDistance = abs(coordinate) / derivative;
    float xAxisAlpha = 1.0f - smoothstep(0.5f, 2.5f, axisDistance.y);
    float yAxisAlpha = 1.0f - smoothstep(0.5f, 2.5f, axisDistance.x);

    if (xAxisAlpha > 0.0f) color = lerp(color, float4(1.0f, 0.3f, 0.3f, 1.0f), xAxisAlpha);
    if (yAxisAlpha > 0.0f) color = lerp(color, float4(0.4f, 1.0f, 0.4f, 1.0f), yAxisAlpha);
    if (color.a <= 0.02f) discard;

    return color;
}