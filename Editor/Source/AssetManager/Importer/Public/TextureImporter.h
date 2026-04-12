#pragma once

#include "ImporterInterface.h"
#include "TextureCompressorInterface.h"

struct tagTextureImportDesc
{
	Engine::ETextureFormat TargetFormat = Engine::ETextureFormat::BC3_RGBA_UNORM;
	ECompressFlags CompressFlags = ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality;
};

BEGIN(Editor)
class TextureImporter : public ImporterInterface
{
#pragma region Constructor&Destructor
private:
	using DESC = tagTextureImportDesc;
	TextureImporter() = default;
	virtual ~TextureImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
public:
	static TextureImporter* Create() { return new TextureImporter(); }
	virtual void Free() override { ImporterInterface::Free(); }
#pragma endregion

public:
	virtual EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr) override;

#pragma region Member Variables
private:

#pragma endregion
};
END