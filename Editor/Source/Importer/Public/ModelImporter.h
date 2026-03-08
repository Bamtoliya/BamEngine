#pragma once

#include "Importer.h"

BEGIN(Editor)
class ModelImporter : public Importer
{
private:
	ModelImporter() = default;
	virtual ~ModelImporter() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
public:
	virtual void Free() override { Importer::Free(); }
public:
	virtual EResult Import(const filesystem::path& filePath) override;
};
END