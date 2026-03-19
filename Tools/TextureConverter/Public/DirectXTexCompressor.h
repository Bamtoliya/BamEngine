#pragma once

#include "TextureCompressorInterface.h"

BEGIN(Tools)
class DirectXTexCompressor : public TextureCompressorInterface
{
public:
	virtual bool CompressTexture(const std::wstring& inputFilePath, const std::wstring& outputFilePath, Engine::ETextureFormat targetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM, ECompressFlags flags = ECompressFlags::None) override;
	virtual bool GetRawCompressedData(const std::wstring& inputFilePath, std::vector<uint8>& outData, tagTextureBinaryHeader& header, Engine::ETextureFormat targetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM, ECompressFlags flags = ECompressFlags::None) override;
};
END