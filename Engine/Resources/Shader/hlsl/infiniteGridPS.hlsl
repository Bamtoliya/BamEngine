#include "common.hlsli"
#include "matrix.hlsli"

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 NDC : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_TARGET;
    float Depth : SV_Depth;
};

float3 UnprojectPoint(float3 ndc)
{
    float4x4 inverseViewProjection = commonData.InvViewProjection;
    float4 worldPosition = mul(inverseViewProjection, float4(ndc, 1.0f));
    return worldPosition.xyz / worldPosition.w;
}

float4 Grid3D(float3 fragmentPosition, float scale, float thickness, bool drawAxis)
{
    float2 coordinate = fragmentPosition.xz * scale;
    float2 derivative = fwidth(coordinate);
    float2 grid = abs(frac(coordinate - 0.5f) - 0.5f) / derivative;
    float lineDistance = min(grid.x, grid.y) / thickness;
    float minimumZ = min(derivative.y, 1.0f);
    float minimumX = min(derivative.x, 1.0f);
    float4 color = float4(0.2f, 0.2f, 0.2f, 1.0f - min(lineDistance, 1.0f));

    if (drawAxis)
    {
        if (fragmentPosition.x > -0.1f * minimumX * thickness && fragmentPosition.x < 0.1f * minimumX * thickness)
        {
            color.rgb = float3(0.2f, 0.2f, 1.0f);
        }
        if (fragmentPosition.z > -0.1f * minimumZ * thickness && fragmentPosition.z < 0.1f * minimumZ * thickness)
        {
            color.rgb = float3(1.0f, 0.2f, 0.2f);
        }
    }

    return color;
}

PS_OUTPUT main(PS_INPUT input)
{
    float3 nearPoint = UnprojectPoint(float3(input.NDC, 0.0f));
    float3 farPoint = UnprojectPoint(float3(input.NDC, 1.0f));
    float3 rayDirection = farPoint - nearPoint;
    float t = -nearPoint.y / rayDirection.y;
    if (t < 0.0f) discard;

    float3 fragmentPosition = nearPoint + t * rayDirection;
    float4 clipPosition = mul(commonData.ViewProjection, float4(fragmentPosition, 1.0f));
    float4 grid1 = Grid3D(fragmentPosition, 1.0f, 1.0f, false);
    float4 grid10 = Grid3D(fragmentPosition, 0.1f, 2.5f, true);
    float4 grid100 = Grid3D(fragmentPosition, 0.01f, 5.0f, true);
    float4 color = grid1 + grid10 + grid100;
    color.a = max(grid1.a, max(grid10.a, grid100.a));
    color.a *= max(0.0f, 1.0f - length(fragmentPosition - commonData.CameraPos) / 150.0f);
    if (color.a <= 0.0f) discard;

    PS_OUTPUT output;
    output.Color = color;
    output.Depth = clipPosition.z / clipPosition.w + 0.00001f;
    return output;
}