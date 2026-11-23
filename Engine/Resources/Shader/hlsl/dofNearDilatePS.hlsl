#include "depthOfField.hlsli"

// 앞쪽 CoC 타일을 최대 흐림 반지름만큼 이웃으로 넓힌다. 흐린 앞 물체가 번질 수 있는 범위다.
Texture2D gTile : register(t0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    uint width, height;
    gTile.GetDimensions(width, height);
    const int2 tile = int2(input.Position.xy);
    const int2 maxTile = int2(width, height) - 1;
    const int radius = (int)min(gDof.DilateTiles, 4u);

    float nearCoC = 0.0f;
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
            nearCoC = max(nearCoC, gTile.Load(int3(clamp(tile + int2(x, y), int2(0, 0), maxTile), 0)).r);
    }
    return float4(nearCoC, 0.0f, 0.0f, 0.0f);
}
