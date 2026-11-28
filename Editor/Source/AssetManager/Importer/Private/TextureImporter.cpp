#pragma once

#include "TextureImporter.h"
#include "TextureConverter.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "AssetWorkerHost.h"

#include <memory>
#include <Windows.h>

tagTextureImportDesc TextureImporter::GetDefaultImportDesc(const filesystem::path& sourcePath)
{
	tagTextureImportDesc desc{};
	const std::string stem = sourcePath.stem().string();
	if (stem.find("_Normal") != std::string::npos || stem.find("_normal") != std::string::npos ||
		stem.find("_Norm") != std::string::npos || stem.find("_norm") != std::string::npos ||
		stem.find("_nrm") != std::string::npos || stem.find("_N") != std::string::npos)
	{
		desc.TargetFormat = Engine::ETextureFormat::BC5_RG_UNORM;
		desc.CompressFlags = ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality | ECompressFlags::NormalMap;
	}
	else
	{
		desc.TargetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM;
		desc.CompressFlags = ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality | ECompressFlags::SRGB;
	}
	return desc;
}

filesystem::path TextureImporter::ResolveOutputPath(const filesystem::path& sourcePath, const filesystem::path& destination)
{
	if (destination.empty()) return sourcePath.parent_path() / sourcePath.filename();
	std::error_code ec;
	const bool exists = filesystem::exists(destination, ec);
	if (ec)
	{
		BAM_LOG(Error, "Editor", "Asset", "Failed to inspect texture destination: {}", ec.message());
		return {};
	}
	if (exists && filesystem::is_directory(destination, ec)) return destination / sourcePath.filename();
	if (ec)
	{
		BAM_LOG(Error, "Editor", "Asset", "Failed to inspect texture destination: {}", ec.message());
		return {};
	}
	if (_wcsicmp(destination.extension().c_str(), sourcePath.extension().c_str()) == 0) return destination;
	return destination / sourcePath.filename();
}

EResult TextureImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	const tagTextureImportDesc desc = arg
		? *static_cast<tagTextureImportDesc*>(arg)
		: GetDefaultImportDesc(sourcePath);

	filesystem::path outputPath = ResolveOutputPath(sourcePath, destDir);
	if (outputPath.empty()) return EResult::Fail;

	filesystem::path bamtexPath = outputPath;
	bamtexPath.replace_extension(".bamtex");

	filesystem::path ddsPath = outputPath;
	ddsPath.replace_extension(".dds");

	// 1. 팩토리에서 압축기 생성
	std::unique_ptr<Tools::TextureCompressorInterface> compressor(Tools::TextureCompressorFactory::Create());
	if (!compressor) return EResult::Fail;

	std::vector<uint8> rawData;
	Engine::TextureBinaryHeader texHeader{};

	// 2. 압축 수행 및 데이터 추출
	const bool rawDataSuccess = compressor->GetRawCompressedData(
		sourcePath.wstring(), rawData, texHeader, desc.TargetFormat, desc.CompressFlags);
	if (!rawDataSuccess)
		return EResult::Fail;

	struct StagedOutputs
	{
		filesystem::path bamtex;
		filesystem::path dds;
		~StagedOutputs()
		{
			std::error_code ec;
			if (!bamtex.empty()) filesystem::remove(bamtex, ec);
			ec.clear();
			if (!dds.empty()) filesystem::remove(dds, ec);
		}
	} staged;
	if (!AssetWorkerHost::CreateTextureStagingPath(bamtexPath, staged.bamtex) ||
		!AssetWorkerHost::CreateTextureStagingPath(ddsPath, staged.dds))
	{
		BAM_LOG(Error, "Editor", "Asset", "Failed to create texture staging files.");
		return EResult::Fail;
	}

	const bool ddsSuccess = compressor->SaveCompressedDataToDDS(staged.dds.wstring(), rawData, texHeader);
	if (!ddsSuccess)
		return EResult::Fail;

	// 4. CPU 데이터 전용 임시 Texture 객체 생성 후 Serialize로 .bamtex 저장
	const auto destroyTexture = [](Engine::Texture* value)
		{
			if (value)
				Engine::ResourceManager::DestroyUnregisteredResource(value);
		};
	std::unique_ptr<Engine::Texture, decltype(destroyTexture)> texture(
		Engine::Texture::Create(texHeader, rawData, bamtexPath.wstring()), destroyTexture);
	if (!texture) return EResult::Fail;

	const EResult result = ResourceManager::Get().SaveToBinaryFile(texture.get(), staged.bamtex.wstring());
	if (result != EResult::Success) return result;
	return AssetWorkerHost::CommitTextureOutputs(staged.bamtex, staged.dds, bamtexPath, ddsPath)
		? EResult::Success : EResult::Fail;
}
