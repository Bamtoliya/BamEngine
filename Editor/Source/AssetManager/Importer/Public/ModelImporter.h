#pragma once

#include "ImporterInterface.h"

BEGIN(Editor)
class ModelImporter : public ImporterInterface
{
#pragma region Contructor&Destructor
private:
	ModelImporter() = default;
	virtual ~ModelImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
public:
	static ModelImporter* Create() { return new ModelImporter(); }
	virtual void Free() override { ImporterInterface::Free(); }
#pragma endregion
public:
	virtual EResult Import(const filesystem::path & sourcePath, const filesystem::path & destDir) override;


#pragma region Member Variables
private:
#pragma endregion

};
END