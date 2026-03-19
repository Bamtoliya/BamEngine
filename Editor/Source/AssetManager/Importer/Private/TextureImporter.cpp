#pragma once

#include "TextureImporter.h"
#include "SerializationHelper.h"
#include "TextureConverter.h"

EResult TextureImporter::Import(const filesystem::path & sourcePath, const filesystem::path & destDir)
{
	//uint32 width = { 0 }, height = { 0 }, channel = { 0 };
	//
	//stbi_uc* data = stbi_load(sourcePath.string().c_str(), reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height), reinterpret_cast<int*>(&channel), 4);
	//if (!data) return EResult::Fail;
	//channel = 4;
	//
	//
	//filesystem::path outputPath = destDir;
	//
	//outputPath.replace_extension(".bamtex");
	//
	//BinaryArchive archive(EArchiveMode::Write);
	//
	//tagTextureBinaryHeader texHeader;
	//texHeader.Width = width;
	//texHeader.Height = height;
	//texHeader.Depth = 1;
	//texHeader.MipLevels = 1;
	//texHeader.ArraySize = 1;
	//texHeader.Format = ETextureFormat::R8G8B8A8_UNORM;
	//texHeader.Dimension = ETextureDimension::Texture2D;
	//texHeader.DataSize = width * height * channel;
	//
	//archive.Process("TextureHeader", texHeader);
	//
	//size_t dataSize = width * height * channel;
	//archive.ProcessRaw("PixelData", data, dataSize);
	//stbi_image_free(data);
	//return archive.SaveToFile(outputPath.string()) ? EResult::Success : EResult::Fail;

	filesystem::path outputPath = destDir;
	outputPath.replace_extension(".bamtex");

	// 1. 팩토리에서 압축기 생성
	Tools::TextureCompressorInterface* compressor = Tools::TextureCompressorFactory::Create();
	if (!compressor) return EResult::Fail;

	std::vector<uint8> rawData;
	Engine::tagTextureBinaryHeader texHeader;

	// 압축 옵션 설정 (필요시 임포터 세팅에서 받아올 수 있게 확장 가능)
	ECompressFlags flags = ECompressFlags::None;

	// 2. 압축 수행 및 데이터 추출
	bool bSuccess = compressor->GetRawCompressedData(sourcePath.wstring(), rawData, texHeader, Engine::ETextureFormat::BC3_RGBA_UNORM, flags);

	// 압축기 메모리 해제
	delete compressor;

	if (!bSuccess)
	{
		return EResult::Fail;
	}

	// 3. BinaryArchive를 이용해 .bamtex 파일로 직렬화(저장)
	//JsonArchive jsonArchive(EArchiveMode::Write);
	BinaryArchive archive(EArchiveMode::Write);

	tagResourceBinaryHeader resourceHeader;
	resourceHeader.ResourceType = EResourceType::Texture;

	archive.Process("AssetHeader", resourceHeader);
	archive.Process("TextureHeader", texHeader);
	archive.ProcessRaw("PixelData", rawData.data(), rawData.size());

	//archive.Process("TextureHeader", texHeader);
	//archive.ProcessRaw("PixelData", rawData.data(), rawData.size());

	return archive.SaveToFile(outputPath.string()) ? EResult::Success : EResult::Fail;
}