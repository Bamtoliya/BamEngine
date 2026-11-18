#pragma once

#include "TextureImporter.h"
#include "TextureConverter.h"
#include "ResourceManager.h"
#include "Texture.h"

EResult TextureImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	tagTextureImportDesc defaultDesc{};
	tagTextureImportDesc* desc = arg ? static_cast<tagTextureImportDesc*>(arg) : &defaultDesc;

	// 인자가 지정되지 않은 기본 임포트인 경우 파일명 기반 스마트 기본값 설정
	if (!arg)
	{
		const std::string stem = sourcePath.stem().string();
		if (stem.find("_Normal") != std::string::npos || stem.find("_normal") != std::string::npos ||
			stem.find("_Norm") != std::string::npos || stem.find("_norm") != std::string::npos ||
			stem.find("_nrm") != std::string::npos || stem.find("_N") != std::string::npos)
		{
			desc->TargetFormat = Engine::ETextureFormat::BC5_RG_UNORM;
			desc->CompressFlags = ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality | ECompressFlags::NormalMap;
		}
		else
		{
			desc->TargetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM;
			desc->CompressFlags = ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality | ECompressFlags::SRGB;
		}
	}

	filesystem::path outputDir = destDir.empty() ? sourcePath.parent_path() : destDir;
	filesystem::path outputPath = outputDir / sourcePath.filename();

	filesystem::path bamtexPath = outputPath;
	bamtexPath.replace_extension(".bamtex");

	filesystem::path ddsPath = outputPath;
	ddsPath.replace_extension(".dds");

	// 1. 팩토리에서 압축기 생성
	Tools::TextureCompressorInterface* compressor = Tools::TextureCompressorFactory::Create();
	if (!compressor) return EResult::Fail;

	std::vector<uint8> rawData;
	Engine::TextureBinaryHeader texHeader{};

	// 2. 압축 수행 및 데이터 추출
	bool bSuccess = compressor->GetRawCompressedData(sourcePath.wstring(), rawData, texHeader, desc->TargetFormat, desc->CompressFlags);

	// 3. 표준 .dds 파일로도 베이킹
	compressor->CompressTexture(sourcePath.wstring(), ddsPath.wstring(), desc->TargetFormat, desc->CompressFlags);

	delete compressor;

	if (!bSuccess)
		return EResult::Fail;

	// 4. CPU 데이터 전용 임시 Texture 객체 생성 후 Serialize로 .bamtex 저장
	Engine::Texture* texture = Engine::Texture::Create(texHeader, rawData);
	if (!texture) return EResult::Fail;

	texture->SetPath(bamtexPath.wstring());
	texture->SetKey(bamtexPath.wstring());

	EResult result = ResourceManager::Get().SaveToBinaryFile(texture, bamtexPath.wstring());
	ResourceManager::Get().DestroyResource(texture); // 저장 후 임시 Texture 객체 해제

	return result;
}
