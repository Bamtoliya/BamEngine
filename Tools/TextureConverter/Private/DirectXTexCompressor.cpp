#pragma once

#include "DirectXTexCompressor.h"
#include <DirectXTex.h>

using namespace Tools;

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

bool DirectXTexCompressor::GetRawCompressedData(const std::wstring& inputFilePath, std::vector<uint8>& outData, tagTextureBinaryHeader& outHeader, Engine::ETextureFormat targetFormat, ECompressFlags flags)
{
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

	outHeader.Width = static_cast<uint32>(meta.width);
	outHeader.Height = static_cast<uint32>(meta.height);
	outHeader.Depth = static_cast<uint32>(meta.depth);
	outHeader.MipLevels = static_cast<uint32>(meta.mipLevels);
	outHeader.ArraySize = static_cast<uint32>(meta.arraySize);
	outHeader.Format = targetFormat;

	// Dimension 판단 (1D, 2D, 3D, Cube)
	switch (meta.dimension)
	{
	case DirectX::TEX_DIMENSION_TEXTURE1D:
		outHeader.Dimension = Engine::ETextureDimension::Texture1D;
		break;
	case DirectX::TEX_DIMENSION_TEXTURE2D:
		outHeader.Dimension = meta.IsCubemap() ? Engine::ETextureDimension::TextureCube : Engine::ETextureDimension::Texture2D;
		break;
	case DirectX::TEX_DIMENSION_TEXTURE3D:
		outHeader.Dimension = Engine::ETextureDimension::Texture3D;
		break;
	default:
		break;
	}

	outHeader.DataSize = static_cast<uint32>(currentImage->GetPixelsSize());

	// 5. Raw 픽셀 데이터를 outData 벡터에 복사
	const uint8_t* pPixels = currentImage->GetPixels();
	outData.assign(pPixels, pPixels + outHeader.DataSize);

	return true;
}