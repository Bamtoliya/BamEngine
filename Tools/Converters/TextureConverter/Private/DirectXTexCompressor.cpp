#pragma once

#include "DirectXTexCompressor.h"
#include "Logger.h"
#pragma push_macro("new")
#undef new
#include <d3d11.h>
#include <DirectXTex.h>
#include <objbase.h>
#include <Windows.h>
#include <wrl/client.h>
#pragma pop_macro("new")
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include "../../Common/DdcCommon.h"

using namespace Tools;

namespace
{
	class MtaLifetime
	{
	public:
		MtaLifetime() : m_Result(CoIncrementMTAUsage(&m_Cookie)) {}

		~MtaLifetime()
		{
			if (SUCCEEDED(m_Result))
				CoDecrementMTAUsage(m_Cookie);
		}

		bool IsAvailable() const { return SUCCEEDED(m_Result); }

	private:
		CO_MTA_USAGE_COOKIE m_Cookie{};
		HRESULT m_Result;
	};

	class ScopedComInitialization
	{
	public:
		ScopedComInitialization()
		{
			// DirectXTex caches its WIC factory beyond individual conversion calls.
			static MtaLifetime mtaLifetime;
			if (!mtaLifetime.IsAvailable())
			{
				BAM_LOG(Error, "Tools", "Asset", "Failed to retain COM MTA for texture conversion.");
				return;
			}

			m_Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			if (FAILED(m_Result))
				BAM_LOG(Error, "Tools", "Asset", "Failed to initialize COM MTA for texture conversion: {}",
					static_cast<unsigned long>(m_Result));
		}

		~ScopedComInitialization()
		{
			if (SUCCEEDED(m_Result))
				CoUninitialize();
		}

		bool IsAvailable() const
		{
			return SUCCEEDED(m_Result);
		}

	private:
		HRESULT m_Result = E_FAIL;
	};

	constexpr uint32_t DDC_MAGIC = 0x43445442; // "BTDC" (Bam Texture Derived Cache)
	constexpr uint32_t DDC_VERSION = 1;

	DXGI_FORMAT GetStoredDXGIFormat(Engine::ETextureFormat format)
	{
		switch (format)
		{
		case Engine::ETextureFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Engine::ETextureFormat::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case Engine::ETextureFormat::BC1_RGBA_UNORM: return DXGI_FORMAT_BC1_UNORM;
		case Engine::ETextureFormat::BC1_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC1_UNORM_SRGB;
		case Engine::ETextureFormat::BC2_RGBA_UNORM: return DXGI_FORMAT_BC2_UNORM;
		case Engine::ETextureFormat::BC2_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC2_UNORM_SRGB;
		case Engine::ETextureFormat::BC3_RGBA_UNORM: return DXGI_FORMAT_BC3_UNORM;
		case Engine::ETextureFormat::BC3_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC3_UNORM_SRGB;
		case Engine::ETextureFormat::BC4_R_UNORM: return DXGI_FORMAT_BC4_UNORM;
		case Engine::ETextureFormat::BC5_RG_UNORM: return DXGI_FORMAT_BC5_UNORM;
		case Engine::ETextureFormat::BC7_RGBA_UNORM: return DXGI_FORMAT_BC7_UNORM;
		case Engine::ETextureFormat::BC7_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC7_UNORM_SRGB;
		default: return DXGI_FORMAT_UNKNOWN;
		}
	}

	bool ValidateStoredTexture(const Engine::TextureBinaryHeader& header, size_t dataSize)
	{
		const auto format = GetStoredDXGIFormat(header.format);
		if (format == DXGI_FORMAT_UNKNOWN || header.dimension != Engine::ETextureDimension::Texture2D ||
			header.width == 0 || header.height == 0 || header.depth != 1 || header.arraySize != 1 ||
			header.mipLevels == 0 || header.mipLevels > Engine::Texture::CalculateMipLevels(header.width, header.height) ||
			header.dataSize != dataSize)
			return false;

		size_t width = header.width, height = header.height, total = 0;
		for (uint32_t mip = 0; mip < header.mipLevels; ++mip)
		{
			size_t rowPitch = 0, slicePitch = 0;
			if (FAILED(DirectX::ComputePitch(format, width, height, rowPitch, slicePitch)) ||
				total > dataSize || slicePitch > dataSize - total)
				return false;
			total += slicePitch;
			width = (std::max)(size_t{ 1 }, width / 2);
			height = (std::max)(size_t{ 1 }, height / 2);
		}
		return total == dataSize;
	}

	std::filesystem::path GetDdcCachePath(const std::wstring& inputFilePath,
		Engine::ETextureFormat targetFormat, ECompressFlags flags)
	{
		std::error_code ec;
		const auto path = std::filesystem::absolute(inputFilePath, ec).lexically_normal();
		if (ec || !std::filesystem::exists(path, ec)) return {};

		Tools::Ddc::DdcHasher hasher;
		const std::string u8Path = path.string();
		hasher.Combine(u8Path.data(), u8Path.size());
		hasher.CombineFileMetadata(path);
		hasher.CombinePod(targetFormat);
		hasher.CombinePod(flags);
		hasher.CombineString(HasFlag(flags, ECompressFlags::UseGPU) ? "tex_ddc_v2_gpu"
			: targetFormat == Engine::ETextureFormat::R8G8B8A8_UNORM ? "tex_ddc_v2_uncompressed" : "tex_ddc_v1");

		return hasher.ResolvePath(Tools::Ddc::FindDdcDirectory("textures"));
	}

	bool TryLoadFromDdc(const std::filesystem::path& ddcPath,
		std::vector<uint8>& outData, Engine::TextureBinaryHeader& outHeader,
		Engine::ETextureFormat targetFormat, ECompressFlags flags)
	{
		std::error_code ec;
		if (ddcPath.empty() || !std::filesystem::exists(ddcPath, ec)) return false;

		std::ifstream file(ddcPath, std::ios::binary);
		if (!file.is_open()) return false;

		uint32_t magic = 0, version = 0;
		file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		file.read(reinterpret_cast<char*>(&version), sizeof(version));
		if (!file || magic != DDC_MAGIC || version != DDC_VERSION)
		{
			BAM_LOG(Warn, "Tools", "Asset", "Invalid texture cache header; rebuilding.");
			return false;
		}

		file.read(reinterpret_cast<char*>(&outHeader), sizeof(outHeader));
		auto expectedFormat = GetStoredDXGIFormat(targetFormat);
		if (HasFlag(flags, ECompressFlags::SRGB))
			expectedFormat = DirectX::MakeSRGB(expectedFormat);
		if (!file || outHeader.dataSize == 0 || outHeader.dataSize > 256 * 1024 * 1024 ||
			expectedFormat == DXGI_FORMAT_UNKNOWN || GetStoredDXGIFormat(outHeader.format) != expectedFormat ||
			!ValidateStoredTexture(outHeader, outHeader.dataSize))
		{
			BAM_LOG(Warn, "Tools", "Asset", "Invalid texture cache metadata; rebuilding.");
			return false;
		}

		outData.resize(outHeader.dataSize);
		file.read(reinterpret_cast<char*>(outData.data()), outHeader.dataSize);
		if (!file)
		{
			BAM_LOG(Warn, "Tools", "Asset", "Truncated texture cache data; rebuilding.");
			return false;
		}
		return true;
	}

	void SaveToDdc(const std::filesystem::path& ddcPath,
		const std::vector<uint8>& data, const Engine::TextureBinaryHeader& header)
	{
		if (ddcPath.empty() || data.empty()) return;

		wchar_t temporaryFile[MAX_PATH]{};
		if (!GetTempFileNameW(ddcPath.parent_path().c_str(), L"BTD", 0, temporaryFile))
		{
			BAM_LOG(Warn, "Tools", "Asset", "Failed to create texture cache staging file: {}", GetLastError());
			return;
		}
		const std::filesystem::path tmpPath(temporaryFile);
		std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
		if (!file.is_open())
		{
			BAM_LOG(Warn, "Tools", "Asset", "Failed to open texture cache staging file.");
			std::error_code ec;
			std::filesystem::remove(tmpPath, ec);
			return;
		}

		file.write(reinterpret_cast<const char*>(&DDC_MAGIC), sizeof(DDC_MAGIC));
		file.write(reinterpret_cast<const char*>(&DDC_VERSION), sizeof(DDC_VERSION));
		file.write(reinterpret_cast<const char*>(&header), sizeof(header));
		file.write(reinterpret_cast<const char*>(data.data()), data.size());
		file.close();
		if (!file || !MoveFileExW(tmpPath.c_str(), ddcPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			BAM_LOG(Warn, "Tools", "Asset", "Failed to publish texture cache staging file.");
			std::error_code ec;
			std::filesystem::remove(tmpPath, ec);
		}
	}
}

static DXGI_FORMAT GetDXGIFormat(Engine::ETextureFormat format) {
	switch (format)
	{
	case Engine::ETextureFormat::BC1_RGBA_UNORM:	return DXGI_FORMAT_BC1_UNORM;
	case Engine::ETextureFormat::BC2_RGBA_UNORM:	return DXGI_FORMAT_BC2_UNORM;
	case Engine::ETextureFormat::BC3_RGBA_UNORM:	return DXGI_FORMAT_BC3_UNORM;		
	case Engine::ETextureFormat::BC4_R_UNORM: 		return DXGI_FORMAT_BC4_UNORM;
	case Engine::ETextureFormat::BC5_RG_UNORM: 		return DXGI_FORMAT_BC5_UNORM;
	case Engine::ETextureFormat::BC7_RGBA_UNORM:	return DXGI_FORMAT_BC7_UNORM;
	default:
		break;
	}
	return DXGI_FORMAT_R8G8B8A8_UNORM;
}

static HRESULT CompressImage(const DirectX::ScratchImage& source, DXGI_FORMAT format,
	ECompressFlags flags, DirectX::ScratchImage& output, bool& gpuFallback, uint32& gpuError)
{
	const bool isBC7 = format == DXGI_FORMAT_BC7_UNORM || format == DXGI_FORMAT_BC7_UNORM_SRGB;
	if (HasFlag(flags, ECompressFlags::UseGPU) && HasFlag(flags, ECompressFlags::FastBC7))
	{
		BAM_LOG(Error, "Tools", "Asset", "GPU compression and fast CPU compression cannot be combined.");
		return E_INVALIDARG;
	}

	if (HasFlag(flags, ECompressFlags::UseGPU))
	{
		if (isBC7)
		{
			Microsoft::WRL::ComPtr<ID3D11Device> device;
			HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
				nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, nullptr);
			if (SUCCEEDED(hr))
			{
				hr = DirectX::Compress(device.Get(), source.GetImages(), source.GetImageCount(), source.GetMetadata(),
					format, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_ALPHA_WEIGHT_DEFAULT, output);
				if (SUCCEEDED(hr))
				{
					BAM_LOG(Info, "Tools", "Asset", "Texture compression backend: GPU BC7.");
					return hr;
				}
				output.Release();
			}
			BAM_LOG(Warn, "Tools", "Asset", "GPU BC7 compression unavailable or failed: HRESULT={}; using CPU.",
				static_cast<unsigned long>(hr));
			gpuError = static_cast<uint32>(hr);
		}
		else
		{
			BAM_LOG(Info, "Tools", "Asset", "GPU compression does not support this format; using CPU.");
		}
		gpuFallback = true;
	}

	DWORD options = DirectX::TEX_COMPRESS_DEFAULT;
	if (HasFlag(flags, ECompressFlags::HighQuality))
		options |= DirectX::TEX_COMPRESS_PARALLEL;
	if (isBC7 && HasFlag(flags, ECompressFlags::FastBC7))
		options |= DirectX::TEX_COMPRESS_BC7_QUICK;
	BAM_LOG(Info, "Tools", "Asset", "Texture compression backend: {}.",
		isBC7 && HasFlag(flags, ECompressFlags::FastBC7) ? "CPU fast BC7" : "CPU standard");
	return DirectX::Compress(source.GetImages(), source.GetImageCount(), source.GetMetadata(), format,
		static_cast<DirectX::TEX_COMPRESS_FLAGS>(options), DirectX::TEX_ALPHA_WEIGHT_DEFAULT, output);
}

bool DirectXTexCompressor::CompressTexture(const std::wstring& inputFilePath, const std::wstring& outputFilePath, Engine::ETextureFormat targetFormat, ECompressFlags flags)
{
	std::vector<uint8> data;
	Engine::TextureBinaryHeader header{};
	return GetRawCompressedData(inputFilePath, data, header, targetFormat, flags) &&
		SaveCompressedDataToDDS(outputFilePath, data, header);
}

bool DirectXTexCompressor::GetRawCompressedData(const std::wstring& inputFilePath, std::vector<uint8>& outData, Engine::TextureBinaryHeader& outHeader, Engine::ETextureFormat targetFormat, ECompressFlags flags, TextureCompressionResult* result)
{
	if (result) *result = {};
	if (HasFlag(flags, ECompressFlags::UseGPU) && HasFlag(flags, ECompressFlags::FastBC7))
	{
		BAM_LOG(Error, "Tools", "Asset", "GPU compression and fast CPU compression cannot be combined.");
		return false;
	}
	// ── 0. DDC 디스크 캐시 조회 (적중 시 0.1ms 즉시 반환) ──
	const auto ddcPath = GetDdcCachePath(inputFilePath, targetFormat, flags);
	if (TryLoadFromDdc(ddcPath, outData, outHeader, targetFormat, flags))
	{
		if (result)
			result->Backend = HasFlag(flags, ECompressFlags::UseGPU) && targetFormat == Engine::ETextureFormat::BC7_RGBA_UNORM
				? ETextureCompressionBackend::CachedGpu
				: HasFlag(flags, ECompressFlags::FastBC7) && targetFormat == Engine::ETextureFormat::BC7_RGBA_UNORM
				? ETextureCompressionBackend::CachedCpuFast : ETextureCompressionBackend::CachedCpu;
		BAM_LOG(Info, "Tools", "Asset", "Texture compression backend: cached result.");
		return true;
	}

	ScopedComInitialization comInitialization;
	if (!comInitialization.IsAvailable()) return false;

	HRESULT hr;
	DirectX::ScratchImage image;

	// 1. 이미지 로드
	hr = DirectX::LoadFromWICFile(inputFilePath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
	if (FAILED(hr)) return false;

	DirectX::ScratchImage mipChain;
	const DirectX::ScratchImage* currentImage = &image;

	// 2. 밉맵 생성
	if (HasFlag(flags, ECompressFlags::GenerateMipMaps))
	{
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
		if (SUCCEEDED(hr)) currentImage = &mipChain;
	}

	DirectX::ScratchImage compressedImage;
	DirectX::ScratchImage convertedImage;
	DXGI_FORMAT dxgiFormat = GetDXGIFormat(targetFormat);
	if (HasFlag(flags, ECompressFlags::SRGB))
	{
		dxgiFormat = DirectX::MakeSRGB(dxgiFormat);
	}

	// 3. 텍스처 압축
	bool gpuFallback = false;
	uint32 gpuError = 0;
	if (DirectX::IsCompressed(dxgiFormat))
	{
		hr = CompressImage(*currentImage, dxgiFormat, flags, compressedImage, gpuFallback, gpuError);

		if (SUCCEEDED(hr)) currentImage = &compressedImage;
		else return false;
		if (result)
		{
			result->Backend = gpuFallback ? ETextureCompressionBackend::CpuFallback
				: HasFlag(flags, ECompressFlags::UseGPU) ? ETextureCompressionBackend::Gpu
				: HasFlag(flags, ECompressFlags::FastBC7) && targetFormat == Engine::ETextureFormat::BC7_RGBA_UNORM
				? ETextureCompressionBackend::CpuFast : ETextureCompressionBackend::Cpu;
			result->GpuError = gpuError;
		}
	}
	else if (currentImage->GetMetadata().format != dxgiFormat)
	{
		hr = DirectX::Convert(currentImage->GetImages(), currentImage->GetImageCount(), currentImage->GetMetadata(),
			dxgiFormat, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, convertedImage);
		if (FAILED(hr)) return false;
		currentImage = &convertedImage;
	}

	// 4. 추출된 데이터를 바탕으로 Header 구성
	const DirectX::TexMetadata& meta = currentImage->GetMetadata();

	outHeader.width = static_cast<uint32>(meta.width);
	outHeader.height = static_cast<uint32>(meta.height);
	outHeader.depth = static_cast<uint32>(meta.depth);
	outHeader.mipLevels = static_cast<uint32>(meta.mipLevels);
	outHeader.arraySize = static_cast<uint32>(meta.arraySize);
	outHeader.format = targetFormat;
	if (HasFlag(flags, ECompressFlags::SRGB))
	{
		if (targetFormat == Engine::ETextureFormat::BC7_RGBA_UNORM)
			outHeader.format = Engine::ETextureFormat::BC7_RGBA_UNORM_SRGB;
		else if (targetFormat == Engine::ETextureFormat::BC1_RGBA_UNORM)
			outHeader.format = Engine::ETextureFormat::BC1_RGBA_UNORM_SRGB;
		else if (targetFormat == Engine::ETextureFormat::BC2_RGBA_UNORM)
			outHeader.format = Engine::ETextureFormat::BC2_RGBA_UNORM_SRGB;
		else if (targetFormat == Engine::ETextureFormat::BC3_RGBA_UNORM)
			outHeader.format = Engine::ETextureFormat::BC3_RGBA_UNORM_SRGB;
		else if (targetFormat == Engine::ETextureFormat::R8G8B8A8_UNORM)
			outHeader.format = Engine::ETextureFormat::R8G8B8A8_UNORM_SRGB;
	}

	switch (meta.dimension)
	{
	case DirectX::TEX_DIMENSION_TEXTURE1D:
		outHeader.dimension = Engine::ETextureDimension::Texture1D;
		break;
	case DirectX::TEX_DIMENSION_TEXTURE2D:
		outHeader.dimension = meta.IsCubemap() ? Engine::ETextureDimension::TextureCube : Engine::ETextureDimension::Texture2D;
		break;
	case DirectX::TEX_DIMENSION_TEXTURE3D:
		outHeader.dimension = Engine::ETextureDimension::Texture3D;
		break;
	default:
		break;
	}

	if (currentImage->GetPixelsSize() > (std::numeric_limits<uint32>::max)())
	{
		BAM_LOG(Error, "Tools", "Asset", "Texture data exceeds the supported size.");
		return false;
	}
	outHeader.dataSize = static_cast<uint32>(currentImage->GetPixelsSize());
	if (GetStoredDXGIFormat(outHeader.format) != meta.format ||
		!ValidateStoredTexture(outHeader, currentImage->GetPixelsSize()))
	{
		BAM_LOG(Error, "Tools", "Asset", "Unsupported or inconsistent texture output layout.");
		return false;
	}

	// 5. Raw 픽셀 데이터를 outData 벡터에 복사
	const uint8_t* pPixels = currentImage->GetPixels();
	outData.assign(pPixels, pPixels + outHeader.dataSize);

	// ── 6. DDC 디스크 캐시에 원자적 저장 ──
	if (!gpuFallback)
		SaveToDdc(ddcPath, outData, outHeader);

	return true;
}

bool DirectXTexCompressor::SaveCompressedDataToDDS(const std::wstring& outputFilePath, const std::vector<uint8>& data, const Engine::TextureBinaryHeader& header)
{
	if (outputFilePath.empty() || !ValidateStoredTexture(header, data.size()))
	{
		BAM_LOG(Error, "Tools", "Asset", "Invalid texture data for DDS output.");
		return false;
	}

	DirectX::ScratchImage image;
	const HRESULT initResult = image.Initialize2D(GetStoredDXGIFormat(header.format), header.width, header.height, 1, header.mipLevels);
	if (FAILED(initResult) || image.GetPixelsSize() != data.size())
	{
		BAM_LOG(Error, "Tools", "Asset", "Failed to reconstruct DDS image: {}", static_cast<unsigned long>(initResult));
		return false;
	}

	std::memcpy(image.GetPixels(), data.data(), data.size());
	const HRESULT saveResult = DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
		DirectX::DDS_FLAGS_NONE, outputFilePath.c_str());
	if (FAILED(saveResult))
		BAM_LOG(Error, "Tools", "Asset", "Failed to write DDS image: {}", static_cast<unsigned long>(saveResult));
	return SUCCEEDED(saveResult);
}