#pragma once

#include "Base.h"
#include "Engine_API.h"
#include "RHITextureTypes.h"

BEGIN(Engine)

struct DDSLoadInfo
{
    uint32 width = 0;
    uint32 height = 0;
    uint32 depth = 1;
    uint32 mipLevels = 1;
    uint32 arraySize = 1;
    ETextureFormat format = ETextureFormat::UNKNOWN;
    ETextureDimension dimension = ETextureDimension::Texture2D;
    const uint8* pixelData = nullptr;
    size_t pixelDataSize = 0;
};

class ENGINE_API DDSLoader
{
public:
    static bool ParseDDS(const uint8* data, size_t dataSize, DDSLoadInfo& outInfo);
};

END
