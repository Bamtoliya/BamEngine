#pragma once

#include "HotReloaderInterface.h"
#include "ShaderCompiler.h"
#include <efsw/efsw.hpp>
#include <unordered_map>

BEGIN(Editor)

class ShaderHotReloadManager final : public IHotReloader
{
#pragma region Constructor&Destructor
private:
	ShaderHotReloadManager() = default;
	virtual ~ShaderHotReloadManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static ShaderHotReloadManager* Create();
	void Free() override;
#pragma endregion

public:
	void Update() override;
	void RequestRecompileAll() { m_PendingRecompileAll = true; }
	void RecompileAll();
	void RecompileFile(const std::filesystem::path& hlslPath) { ProcessModifiedFile(hlslPath); }

	// 레거시 호환용 단일 경로 게터
	[[nodiscard]] std::filesystem::path GetWatchDirectory() const { return GetWatchPaths().empty() ? std::filesystem::path() : GetWatchPaths().front(); }

protected:
	void OnWatchPathAdded(const std::filesystem::path& resolvedPath) override;
	void OnWatchPathRemoved(const std::filesystem::path& resolvedPath) override;

private:
	void ProcessModifiedFile(const std::filesystem::path& hlslPath);
	void RegisterDefaultCandidates();

private:
	efsw::FileWatcher* m_FileWatcher = nullptr;
	efsw::FileWatchListener* m_FileListener = nullptr;
	std::unordered_map<std::filesystem::path, efsw::WatchID> m_WatchIdMap;
	std::atomic<bool> m_PendingRecompileAll = false;
};

END
