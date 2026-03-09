#pragma once

#include "ImporterInterface.h"

BEGIN(Editor)
class TextureImporter : public ImporterInterface
{
#
#pragma region Constructor&Destructor
private:
	TextureImporter() = default;
	virtual ~TextureImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
public:
	static TextureImporter* Create() { return new TextureImporter(); }
	virtual void Free() override { ImporterInterface::Free(); }
#pragma endregion

public:
	virtual EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir) override;

#pragma region Member Variables
private:

#pragma endregion
};
END