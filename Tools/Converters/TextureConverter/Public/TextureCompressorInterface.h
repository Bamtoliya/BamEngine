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
	FastBC7 = 1 << 7,
};

ENABLE_BITMASK_OPERATORS(ECompressFlags)

enum class ETextureCompressionMode : uint8
{
	CpuStandard,
	CpuFast,
	GpuBC7,
};

enum class ETextureCompressionBackend : uint8
{
	Cpu,
	CpuFast,
	Gpu,
	CpuFallback,
	CachedCpu,
	CachedCpuFast,
	CachedGpu,
};

struct TextureCompressionResult
{
	ETextureCompressionBackend Backend = ETextureCompressionBackend::Cpu;
	uint32 GpuError = 0;
};

inline ECompressFlags ApplyTextureCompressionMode(ECompressFlags flags, ETextureCompressionMode mode, Engine::ETextureFormat format)
{
	flags &= ~(ECompressFlags::UseGPU | ECompressFlags::FastBC7);
	if (format == Engine::ETextureFormat::BC7_RGBA_UNORM)
	{
		if (mode == ETextureCompressionMode::CpuFast) flags |= ECompressFlags::FastBC7;
		else if (mode == ETextureCompressionMode::GpuBC7) flags |= ECompressFlags::UseGPU;
	}
	return flags;
}

BEGIN(Tools)
class TextureCompressorInterface
{
public:
	virtual ~TextureCompressorInterface() = default;
	virtual bool CompressTexture(const std::wstring& inputFilePath, const std::wstring& outputFilePath, Engine::ETextureFormat targetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM, ECompressFlags flags = ECompressFlags::None) BAM_PURE;
	virtual bool GetRawCompressedData(const std::wstring& inputFilePath, std::vector<uint8>& outData, TextureBinaryHeader& header, Engine::ETextureFormat targetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM, ECompressFlags flags = ECompressFlags::None, TextureCompressionResult* result = nullptr) BAM_PURE;
	virtual bool SaveCompressedDataToDDS(const std::wstring& outputFilePath, const std::vector<uint8>& data, const TextureBinaryHeader& header) BAM_PURE;
};

class TextureCompressorFactory
{
public:
	static TextureCompressorInterface* Create();
};
END