#pragma once

#include "ImporterInterface.h"

BEGIN(Editor)
class MaterialImporter : public ImporterInterface
{
#pragma region Constructor&Destructor
public:
    MaterialImporter() = default;
    virtual ~MaterialImporter() = default;
    virtual EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	static MaterialImporter* Create() { return new MaterialImporter(); }
    virtual void Free() override { ImporterInterface::Free(); }
#pragma endregion

public:
    virtual EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir);

#pragma region Member Variables
private:
#pragma endregion
};
END
