#pragma once

#include "ImporterInterface.h"

struct tagShaderImportDesc
{

};

BEGIN(Engine)
class ShaderImporter : public ImporterInterface
{
#pragma region Constructor& Destructor
private:
	using DESC = tagShaderImportDesc;
	ShaderImporter() = default;
	virtual ~ShaderImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
public:
	static ShaderImporter* Create() { return new ShaderImporter(); }
	virtual void Free() override { ImporterInterface::Free(); }
#pragma endregion

public:
	virtual EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr) override;

#pragma region Member Variables
private:

#pragma endregion
};
END