#pragma once

#include "DirectXTexCompressor.h"
#include <DirectXTex.h>
#include <filesystem>
#include <fstream>
#include "../../Common/DdcCommon.h"

using namespace Tools;

namespace
{
	constexpr uint32_t DDC_MAGIC = 0x43445442; // "BTDC" (Bam Texture Derived Cache)
	constexpr uint32_t DDC_VERSION = 1;

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
		hasher.CombineString("tex_ddc_v1");

		return hasher.ResolvePath(Tools::Ddc::FindDdcDirectory("textures"));
	}

	bool TryLoadFromDdc(const std::filesystem::path& ddcPath,
		std::vector<uint8>& outData, Engine::TextureBinaryHeader& outHeader)
	{
		std::error_code ec;
		if (ddcPath.empty() || !std::filesystem::exists(ddcPath, ec)) return false;

		std::ifstream file(ddcPath, std::ios::binary);
		if (!file.is_open()) return false;

		uint32_t magic = 0, version = 0;
		file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		file.read(reinterpret_cast<char*>(&version), sizeof(version));
		if (magic != DDC_MAGIC || version != DDC_VERSION) return false;

		file.read(reinterpret_cast<char*>(&outHeader), sizeof(outHeader));
		if (!file || outHeader.dataSize == 0 || outHeader.dataSize > 256 * 1024 * 1024) return false;

		outData.resize(outHeader.dataSize);
		file.read(reinterpret_cast<char*>(outData.data()), outHeader.dataSize);
		return file.good();
	}

	void SaveToDdc(const std::filesystem::path& ddcPath,
		const std::vector<uint8>& data, const Engine::TextureBinaryHeader& header)
	{
		if (ddcPath.empty() || data.empty()) return;

		std::error_code ec;
		const auto tmpPath = ddcPath.string() + ".tmp";
		std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
		if (!file.is_open()) return;

		file.write(reinterpret_cast<const char*>(&DDC_MAGIC), sizeof(DDC_MAGIC));
		file.write(reinterpret_cast<const char*>(&DDC_VERSION), sizeof(DDC_VERSION));
		file.write(reinterpret_cast<const char*>(&header), sizeof(header));
		file.write(reinterpret_cast<const char*>(data.data()), data.size());
		file.close();

		std::filesystem::rename(tmpPath, ddcPath, ec);
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

bool DirectXTexCompressor::CompressTexture(const std::wstring& inputFilePath, const std::wstring& outputFilePath, Engine::ETextureFormat targetFormat, ECompressFlags flags)
{
	HRESULT hr;
	DirectX::ScratchImage image;

	hr = DirectX::LoadFromWICFile(inputFilePath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
	if (FAILED(hr)) return false;

	DirectX::ScratchImage mipChain;
	const DirectX::ScratchImage* currentImage = &image;

	// 2. 밉맵 생성 (선택 사항이지만 3D 엔진에서는 필수 권장, 플래그 검사)
	if (HasFlag(flags, ECompressFlags::GenerateMipMaps))
	{
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
		if (SUCCEEDED(hr)) currentImage = &mipChain;
	}

	DirectX::ScratchImage compressedImage;
	DXGI_FORMAT dxgiFormat = GetDXGIFormat(targetFormat);

	// 3. 텍스처 압축 (BC7 등)
	if (DirectX::IsCompressed(dxgiFormat))
	{
		DWORD compressOptions = DirectX::TEX_COMPRESS_DEFAULT;

		// 고품질(혹은 빠른 연산 요구) 시 병렬 전개 사용
		if (HasFlag(flags, ECompressFlags::HighQuality))
		{
			compressOptions |= DirectX::TEX_COMPRESS_PARALLEL;
		}

		// static_cast로 Enum 규격에 맞게 매개변수 부여, Alpha 변수는 1.0f가 기본값
		hr = DirectX::Compress(currentImage->GetImages(), currentImage->GetImageCount(), currentImage->GetMetadata(),
			dxgiFormat, static_cast<DirectX::TEX_COMPRESS_FLAGS>(compressOptions), DirectX::TEX_ALPHA_WEIGHT_DEFAULT, compressedImage);

		if (SUCCEEDED(hr)) currentImage = &compressedImage;
		else return false;
	}

	// 4. DDS 파일로 저장
	hr = DirectX::SaveToDDSFile(currentImage->GetImages(), currentImage->GetImageCount(), currentImage->GetMetadata(),
		DirectX::DDS_FLAGS_NONE, outputFilePath.c_str());

	return SUCCEEDED(hr);
}

bool DirectXTexCompressor::GetRawCompressedData(const std::wstring& inputFilePath, std::vector<uint8>& outData, Engine::TextureBinaryHeader& outHeader, Engine::ETextureFormat targetFormat, ECompressFlags flags)
{
	// ── 0. DDC 디스크 캐시 조회 (적중 시 0.1ms 즉시 반환) ──
	const auto ddcPath = GetDdcCachePath(inputFilePath, targetFormat, flags);
	if (TryLoadFromDdc(ddcPath, outData, outHeader))
		return true;

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
	DXGI_FORMAT dxgiFormat = GetDXGIFormat(targetFormat);

	// 3. 텍스처 압축
	if (DirectX::IsCompressed(dxgiFormat))
	{
		DWORD compressOptions = DirectX::TEX_COMPRESS_DEFAULT;
		if (HasFlag(flags, ECompressFlags::HighQuality))
		{
			compressOptions |= DirectX::TEX_COMPRESS_PARALLEL;
		}

		hr = DirectX::Compress(currentImage->GetImages(), currentImage->GetImageCount(), currentImage->GetMetadata(),
			dxgiFormat, static_cast<DirectX::TEX_COMPRESS_FLAGS>(compressOptions), DirectX::TEX_ALPHA_WEIGHT_DEFAULT, compressedImage);

		if (SUCCEEDED(hr)) currentImage = &compressedImage;
		else return false;
	}

	// 4. 추출된 데이터를 바탕으로 Header 구성
	const DirectX::TexMetadata& meta = currentImage->GetMetadata();

	outHeader.width = static_cast<uint32>(meta.width);
	outHeader.height = static_cast<uint32>(meta.height);
	outHeader.depth = static_cast<uint32>(meta.depth);
	outHeader.mipLevels = static_cast<uint32>(meta.mipLevels);
	outHeader.arraySize = static_cast<uint32>(meta.arraySize);
	outHeader.format = targetFormat;

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

	outHeader.dataSize = static_cast<uint32>(currentImage->GetPixelsSize());

	// 5. Raw 픽셀 데이터를 outData 벡터에 복사
	const uint8_t* pPixels = currentImage->GetPixels();
	outData.assign(pPixels, pPixels + outHeader.dataSize);

	// ── 6. DDC 디스크 캐시에 원자적 저장 ──
	SaveToDdc(ddcPath, outData, outHeader);

	return true;
}