#pragma once

#include "Editor_Includes.h"
#include "ImporterInterface.h"
#include "ExporterInterface.h"
#include "AssetCache.h"

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

#pragma region Loop
	void Update(f32 dt);
#pragma endregion


#pragma region Codec
public:
	EResult Import(const filesystem::path & sourcePath, const filesystem::path & destDir, void* arg = nullptr);
	void ImportAsync(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg = nullptr);
	EResult Export(const filesystem::path & sourcePath, const filesystem::path & destDir, void* arg = nullptr);
	void ExportAsync(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg = nullptr);
public:
	Engine::MulticastDelegate<> GetAsyncDelegate() { return m_OnAsyncDelegate; }
	size_t GetActiveTaskCount() const { return m_ActiveTasks.size(); }
#pragma endregion


#pragma region Member Variables
private:
	unordered_map<string, ImporterInterface*> m_Importers;
	unordered_map<string, ExporterInterface*> m_Exporters;
	vector<future<EResult>> m_ActiveTasks;
	Engine::MulticastDelegate<> m_OnAsyncDelegate;
private:
	AssetCache* m_AssetCache = nullptr;
#pragma endregion
};
END