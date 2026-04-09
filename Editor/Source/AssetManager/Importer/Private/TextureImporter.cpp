#pragma once

#include "TextureImporter.h"
#include "SerializationHelper.h"
#include "TextureConverter.h"
#include "ResourceManager.h"
#include "Texture.h"

EResult TextureImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC

	filesystem::path outputPath = destDir;
	outputPath.replace_extension(".bamtex");

	// 1. 팩토리에서 압축기 생성
	Tools::TextureCompressorInterface* compressor = Tools::TextureCompressorFactory::Create();
	if (!compressor) return EResult::Fail;

	std::vector<uint8> rawData;
	Engine::tagTextureBinaryHeader texHeader;

	// 2. 압축 수행 및 데이터 추출
	bool bSuccess = compressor->GetRawCompressedData(sourcePath.wstring(), rawData, texHeader, desc->TargetFormat, desc->CompressFlags);

	delete compressor;

	if (!bSuccess)
		return EResult::Fail;

	// 3. CPU 데이터 전용 임시 Texture 객체 생성 후 Serialize로 .bamtex 저장
	//    저장 포맷(AssetHeader + TextureHeader + PixelData)이 Texture::Serialize 내부에서 일관되게 관리됩니다.
	Engine::Texture* texture = Engine::Texture::Create(texHeader, rawData);
	if (!texture) return EResult::Fail;

	EResult result = ResourceManager::Get().SaveToBinaryFile(texture, outputPath.wstring());
	ResourceManager::Get().DestroyResource(texture); // 저장 후 임시 Texture 객체 해제

	return result;
}