#pragma once

#include "SpriteImporter.h"
#include "SerializationHelper.h"
#include "Sprite.h"

EResult SpriteImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	TODO("SpriteImporter");
	return EResult::NotImplemented;
	//wstring extension = sourcePath.extension().wstring();
	//std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
	//
	//if (extension == L".png" || extension == L".jpg" || extension == L".jpeg" || extension == L".bmp" || extension == L".tga")
	//{
	//
	//}
	//else if (extension == L".dds" || extension == L".bamtex")
	//{
	//
	//}
	//else
	//{
	//	return EResult::InvalidArgument;
	//}
	//
	//wstring fileName = sourcePath.stem().wstring();
	//
	//
	//filesystem::path destTexturePath = destDir / (fileName + L".texture");
	//filesystem::path destSpritePath = destDir / (fileName + L".sprite");
	//
	//filesystem::path outputSpritePath = destDir;
	//outputSpritePath.replace_extension(".bamsprite");
	//
	//// 3. Sprite 메타데이터(.sprite) 생성 및 직렬화
	//tagSpriteCreateDesc desc;
	//desc.Key = fileName + L"_Sprite";
	//desc.TexturePath = destTexturePath.wstring(); // 엔진은 .texture 파일을 바라봄
	//
	//Engine::Sprite* newSprite = Engine::Sprite::Create(&desc);
	//if (!newSprite) return EResult::Fail;
	//
	//// 기존 SerializationHelper를 이용한 저장
	//
	//BinaryArchive archive(EArchiveMode::Write);
	//tagResourceBinaryHeader resourceHeader;
	//resourceHeader.ResourceType = EResourceType::Sprite;
	//
	//archive.Process("AssetHeader", resourceHeader);
	//
	//newSprite->Free();
	//return archive.SaveToFile(outputSpritePath.string()) ? EResult::Success : EResult::Fail;
}