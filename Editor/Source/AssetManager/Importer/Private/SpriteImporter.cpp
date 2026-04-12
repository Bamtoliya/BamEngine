#pragma once

#include "SpriteImporter.h"
#include "SerializationHelper.h"
#include "ResourceManager.h"
#include "Sprite.h"

EResult SpriteImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	wstring extension = sourcePath.extension().wstring();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

	if (extension != L".bamtex")
	{
		return EResult::InvalidArgument;
	}

	filesystem::path outputPath = destDir.empty() ? sourcePath.parent_path() : destDir;
	outputPath.replace_extension(L".bamsprite");

	tagSpriteCreateDesc desc;
	desc.TexturePath = sourcePath.wstring();
	desc.Key = sourcePath.stem().wstring() + L"_Sprite";

	Engine::Sprite* sprite = Engine::Sprite::Create(&desc);
	if (!sprite) return EResult::Fail;

	EResult result = ResourceManager::Get().SaveToBinaryFile(sprite, outputPath.wstring());
	ResourceManager::Get().DestroyResource(sprite); // 저장 후 임시 Sprite 객체 해제

	return EResult::Success;
}