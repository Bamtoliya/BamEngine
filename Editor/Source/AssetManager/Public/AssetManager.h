#pragma once

#include "Editor_Includes.h"
#include "ImporterInterface.h"
#include "ExporterInterface.h"

BEGIN(Editor)
class AssetManager : public Base
{
	DECLARE_SINGLETON(AssetManager)

#pragma region Constructor&Destructor
private:
	AssetManager() = default;
	virtual ~AssetManager() = default;
	virtual EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Codec
public:
	EResult Import(const filesystem::path & sourcePath, const filesystem::path & destDir);
	EResult Export(const filesystem::path & sourcePath, const filesystem::path & destDir);
#pragma endregion


#pragma region Member Variables
private:
	unordered_map<string, ImporterInterface*> m_Importers;
	unordered_map<string, ExporterInterface*> m_Exporters;
#pragma endregion
};
END