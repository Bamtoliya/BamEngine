#pragma once

#include "TextureCompressorInterface.h"

BEGIN(Tools)
class DirectXTexCompressor : public TextureCompressorInterface
{
public:
	virtual bool CompressTexture(const std::wstring& inputFilePath, const std::wstring& outputFilePath, Engine::ETextureFormat targetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM, ECompressFlags flags = ECompressFlags::None) override;
	virtual bool GetRawCompressedData(const std::wstring& inputFilePath, std::vector<uint8>& outData, TextureBinaryHeader& header, Engine::ETextureFormat targetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM, ECompressFlags flags = ECompressFlags::None, TextureCompressionResult* result = nullptr) override;
	virtual bool SaveCompressedDataToDDS(const std::wstring& outputFilePath, const std::vector<uint8>& data, const TextureBinaryHeader& header) override;
};
END