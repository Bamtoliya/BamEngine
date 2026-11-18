#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>

#include "Texture.h"
#include "TextureBinaryHeader.h"
#include "DDSLoader.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

using namespace Engine;

void CheckMipLevelsCalculation()
{
	std::cout << "[Test 1] Texture::CalculateMipLevels Verification" << std::endl;

	assert(Texture::CalculateMipLevels(0, 0) == 1);
	assert(Texture::CalculateMipLevels(1, 1) == 1);
	assert(Texture::CalculateMipLevels(2, 2) == 2);
	assert(Texture::CalculateMipLevels(2, 1) == 2);
	assert(Texture::CalculateMipLevels(1, 2) == 2);
	assert(Texture::CalculateMipLevels(3, 3) == 2);
	assert(Texture::CalculateMipLevels(4, 4) == 3);
	assert(Texture::CalculateMipLevels(8, 8) == 4);
	assert(Texture::CalculateMipLevels(16, 16) == 5);
	assert(Texture::CalculateMipLevels(32, 32) == 6);
	assert(Texture::CalculateMipLevels(64, 64) == 7);
	assert(Texture::CalculateMipLevels(512, 256) == 10);
	assert(Texture::CalculateMipLevels(1024, 1024) == 11);
	assert(Texture::CalculateMipLevels(1920, 1080) == 11);
	assert(Texture::CalculateMipLevels(2048, 2048) == 12);
	assert(Texture::CalculateMipLevels(4096, 4096) == 13);

	std::cout << "  -> CalculateMipLevels passed all boundary assertions." << std::endl;
}

void CheckMipChainDownsampling()
{
	std::cout << "[Test 2] CPU Mip Chain Downsampling Verification" << std::endl;

	const uint32 width = 4;
	const uint32 height = 4;
	const uint32 mipLevels = Texture::CalculateMipLevels(width, height);
	assert(mipLevels == 3); // 4x4, 2x2, 1x1

	// 4x4 RGBA 단색(255, 128, 64, 255) 텍스처 생성
	std::vector<uint8_t> mip0(width * height * 4);
	for (size_t i = 0; i < width * height; ++i)
	{
		mip0[i * 4 + 0] = 255;
		mip0[i * 4 + 1] = 128;
		mip0[i * 4 + 2] = 64;
		mip0[i * 4 + 3] = 255;
	}

	// 밉 레벨별 전체 바이트 계산
	uint32 totalBytes = 0;
	uint32 curW = width;
	uint32 curH = height;
	for (uint32 m = 0; m < mipLevels; ++m)
	{
		totalBytes += curW * curH * 4;
		curW = (std::max)(1u, curW >> 1);
		curH = (std::max)(1u, curH >> 1);
	}

	assert(totalBytes == (4 * 4 * 4) + (2 * 2 * 4) + (1 * 1 * 4)); // 64 + 16 + 4 = 84 바이트
	std::vector<uint8_t> pixelData(totalBytes);
	std::memcpy(pixelData.data(), mip0.data(), mip0.size());

	// Mip 1 (2x2), Mip 2 (1x1) 생성
	uint32 prevOffset = 0;
	uint32 prevW = width;
	uint32 prevH = height;

	for (uint32 m = 1; m < mipLevels; ++m)
	{
		const uint32 targetW = (std::max)(1u, width >> m);
		const uint32 targetH = (std::max)(1u, height >> m);
		const uint32 curOffset = prevOffset + prevW * prevH * 4;

		const uint8_t* prevPixels = pixelData.data() + prevOffset;
		uint8_t* curPixels = pixelData.data() + curOffset;

		unsigned char* result = stbir_resize_uint8_linear(
			prevPixels, static_cast<int>(prevW), static_cast<int>(prevH), 0,
			curPixels, static_cast<int>(targetW), static_cast<int>(targetH), 0,
			STBIR_RGBA);

		assert(result != nullptr);

		prevOffset = curOffset;
		prevW = targetW;
		prevH = targetH;
	}

	// Mip 1 (2x2) 픽셀 값 검증: 단색이므로 모두 255, 128, 64, 255에 근접해야 함
	const uint8_t* mip1Pixels = pixelData.data() + (4 * 4 * 4);
	for (size_t i = 0; i < 2 * 2; ++i)
	{
		assert(mip1Pixels[i * 4 + 0] >= 250);
		assert(std::abs(static_cast<int>(mip1Pixels[i * 4 + 1]) - 128) <= 2);
		assert(std::abs(static_cast<int>(mip1Pixels[i * 4 + 2]) - 64) <= 2);
		assert(mip1Pixels[i * 4 + 3] >= 250);
	}

	// Mip 2 (1x1) 픽셀 값 검증
	const uint8_t* mip2Pixels = pixelData.data() + (4 * 4 * 4) + (2 * 2 * 4);
	assert(mip2Pixels[0] >= 250);
	assert(std::abs(static_cast<int>(mip2Pixels[1]) - 128) <= 2);
	assert(std::abs(static_cast<int>(mip2Pixels[2]) - 64) <= 2);
	assert(mip2Pixels[3] >= 250);

	std::cout << "  -> CPU Mip Chain Downsampling verified with 100% color preservation." << std::endl;
}

void CheckHeaderValidationAndDataSize()
{
	std::cout << "[Test 3] TextureBinaryHeader Multilevel Mipmap Layout Verification" << std::endl;

	TextureBinaryHeader header{};
	header.width = 256;
	header.height = 128;
	header.depth = 1;
	header.mipLevels = Texture::CalculateMipLevels(header.width, header.height); // max(256, 128) -> 9 levels
	header.arraySize = 1;
	header.format = ETextureFormat::R8G8B8A8_UNORM;
	header.dimension = ETextureDimension::Texture2D;

	assert(header.mipLevels == 9);

	// 전체 밉 레벨 크기 계산
	uint32 expectedBytes = 0;
	uint32 curW = header.width;
	uint32 curH = header.height;
	for (uint32 m = 0; m < header.mipLevels; ++m)
	{
		expectedBytes += curW * 4 * curH;
		curW = (std::max)(1u, curW >> 1);
		curH = (std::max)(1u, curH >> 1);
	}

	header.dataSize = expectedBytes;

	// 256x128 밉 체인 바이트 크기:
	// Mip 0: 256*128*4 = 131,072
	// Mip 1: 128*64*4  = 32,768
	// Mip 2: 64*32*4   = 8,192
	// Mip 3: 32*16*4   = 2,048
	// Mip 4: 16*8*4    = 512
	// Mip 5: 8*4*4     = 128
	// Mip 6: 4*2*4     = 32
	// Mip 7: 2*1*4     = 8
	// Mip 8: 1*1*4     = 4
	// Total = 174,764 바이트 (정확히 원본의 약 1.333배)
	assert(expectedBytes == 174764);

	std::cout << "  -> 256x128 9-level Mip chain total bytes: " << expectedBytes
	          << " (Overhead: " << (float)expectedBytes / (256 * 128 * 4) << "x)" << std::endl;
}

void CheckDDSLoader()
{
	std::cout << "[Test 4] DDSLoader Parsing & Block Compression Verification" << std::endl;

	// 인메모리 BC7 256x128 9-level Mipmap DDS 바이너리 생성 및 파싱 검증
	const uint32 width = 256;
	const uint32 height = 128;
	const uint32 mipLevels = 9;

	// BC7은 4x4 블록당 16바이트
	uint32 expectedPayloadBytes = 0;
	uint32 curW = width;
	uint32 curH = height;
	for (uint32 m = 0; m < mipLevels; ++m)
	{
		uint32 bw = (curW + 3) / 4;
		uint32 bh = (curH + 3) / 4;
		expectedPayloadBytes += bw * bh * 16;
		curW = (std::max)(1u, curW >> 1);
		curH = (std::max)(1u, curH >> 1);
	}

	std::vector<uint8_t> ddsBuffer(148 + expectedPayloadBytes, 0);

	// 1. Magic ("DDS ")
	uint32 magic = 0x20534444;
	std::memcpy(ddsBuffer.data(), &magic, 4);

	// 2. DDS_HEADER
	uint32* header = reinterpret_cast<uint32*>(ddsBuffer.data() + 4);
	header[0] = 124; // dwSize
	header[1] = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000; // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT
	header[2] = height; // dwHeight
	header[3] = width;  // dwWidth
	header[4] = 0;
	header[5] = 1;      // dwDepth
	header[6] = mipLevels; // dwMipMapCount

	// ddspf (offset 4 + 76 = 80 in ddsBuffer)
	header[18] = 32; // ddspf.dwSize
	header[19] = 0x4; // ddspf.dwFlags (DDPF_FOURCC)
	header[20] = 0x30315844; // ddspf.dwFourCC ("DX10")

	// 3. DDS_HEADER_DXT10 (offset 128 in ddsBuffer)
	uint32* dxt10 = reinterpret_cast<uint32*>(ddsBuffer.data() + 128);
	dxt10[0] = 98; // dxgiFormat: DXGI_FORMAT_BC7_UNORM
	dxt10[1] = 3;  // resourceDimension: D3D10_RESOURCE_DIMENSION_TEXTURE2D
	dxt10[2] = 0;
	dxt10[3] = 1;  // arraySize
	dxt10[4] = 0;

	// 파싱 실행
	DDSLoadInfo info{};
	bool parseResult = DDSLoader::ParseDDS(ddsBuffer.data(), ddsBuffer.size(), info);
	assert(parseResult == true);
	assert(info.width == width);
	assert(info.height == height);
	assert(info.mipLevels == mipLevels);
	assert(info.format == ETextureFormat::BC7_RGBA_UNORM);
	assert(info.pixelDataSize == expectedPayloadBytes);
	assert(IsBlockCompressed(info.format) == true);
	assert(GetBlockCompressedBlockSize(info.format) == 16);

	std::cout << "  -> BC7 DDS (256x128, 9 Mips) parsed successfully. Payload size: "
	          << info.pixelDataSize << " bytes (Uncompressed RGBA would be 174,764 bytes - "
	          << (100.0f - (float)info.pixelDataSize / 174764.0f * 100.0f) << "% saved)." << std::endl;
}

int main()
{
	std::cout << "=== Running TextureMipmapCheck ===" << std::endl;

	CheckMipLevelsCalculation();
	CheckMipChainDownsampling();
	CheckHeaderValidationAndDataSize();
	CheckDDSLoader();

	std::cout << "=== TextureMipmapCheck PASSED ===" << std::endl;
	return 0;
}
