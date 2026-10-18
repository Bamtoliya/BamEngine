#pragma once

#include "Base.h"
#include "RHITexture.h"

namespace Engine
{
    class ENGINE_API CloudTextureGenerator
    {
    public:
        // 언리얼 규격 3D Base Shape 텍스처 생성 (64x64x64, R:Perlin-Worley, GBA:Worley 거품)
        static RHITexture* CreateBaseShapeTexture(RHI* rhi, uint32 size = 64);

        // 언리얼 규격 2D Weather Map 생성 (256x256, R:Coverage, G:TopHeight, B:Density)
        static RHITexture* CreateWeatherMapTexture(RHI* rhi, uint32 size = 256);
    };
}