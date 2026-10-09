#pragma once

#include "Editor_Includes.h"
#include "ImporterInterface.h"
#include "ExporterInterface.h"
#include "AssetCache.h"
#include <mutex>
#include "AssetTaskMetrics.h"

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
	void ExecuteAsync(std::function<EResult()> task);
	EResult Import(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr);
	void ImportAsync(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr);
	EResult Export(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr);
	void ExportAsync(const filesystem::path& sourcePath, const filesystem::path& destDir = {}, void* arg = nullptr);
public:
	Engine::MulticastDelegate<> GetAsyncDelegate() { return m_OnAsyncDelegate; }
#pragma endregion

#pragma region Metrics
	AssetTaskMetrics GetTaskMetrics() const;
	size_t GetActiveTaskCount() const;
#pragma endregion



#pragma region Member Variables
private:
	// 각 확장자 항목은 임포터 참조 하나를 소유한다.
	unordered_map<string, ImporterInterface*> m_Importers;
	unordered_map<string, ExporterInterface*> m_Exporters;

	vector<future<EResult>> m_PendingTasks;
	vector<future<EResult>> m_ActiveTasks;
	Engine::MulticastDelegate<> m_OnAsyncDelegate;
private:
	AssetCache* m_AssetCache = nullptr;
private:
	mutable std::mutex m_PendingMutex;

	Engine::uint64 m_SucceededTasks = 0;
	Engine::uint64 m_FailedTasks = 0;
#pragma endregion
};
END