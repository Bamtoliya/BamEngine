#pragma once

#include "ImporterInterface.h"

BEGIN(Editor)
class SpriteImporter : public IImporter
{
#pragma region Constructor&Destructor
private:
	SpriteImporter() = default;
	virtual ~SpriteImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
public:
	static SpriteImporter* Create() { return new SpriteImporter(); }
	virtual void Free() override { IImporter::Free(); }
#pragma endregion

public:
	virtual EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr) override;

#pragma region Member Variables
private:

#pragma endregion
};
END