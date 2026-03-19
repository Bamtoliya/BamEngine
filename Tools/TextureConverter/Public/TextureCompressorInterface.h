#pragma once

#include "Engine_Includes.h"
#include "RenderTypes.h"
#include "Texture.h"

enum class ECompressFlags : uint32
{
	None = 0,
	GenerateMipMaps = 1 << 0,
	HighQuality = 1 << 1,
	UseGPU = 1 << 2,
	SRGB = 1 << 3,
	FilpY = 1 << 4,
	PremultiplyAlpha = 1 << 5,
	NormalMap = 1 << 6,
};

ENABLE_BITMASK_OPERATORS(ECompressFlags)

BEGIN(Tools)
class TextureCompressorInterface
{
public:
	virtual ~TextureCompressorInterface() = default;
	virtual bool CompressTexture(const std::wstring& inputFilePath, const std::wstring& outputFilePath, Engine::ETextureFormat targetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM, ECompressFlags flags = ECompressFlags::None) PURE;
	virtual bool GetRawCompressedData(const std::wstring& inputFilePath, std::vector<uint8>& outData, tagTextureBinaryHeader& header, Engine::ETextureFormat targetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM, ECompressFlags flags = ECompressFlags::None) PURE;
};

class TextureCompressorFactory
{
public:
	static TextureCompressorInterface* Create();
};
END