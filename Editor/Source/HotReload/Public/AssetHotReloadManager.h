#pragma once

#include "Editor_Includes.h"
#include <efsw/efsw.hpp>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>

BEGIN(Editor)

class AssetHotReloadManager final : public Base
{
	DECLARE_SINGLETON(AssetHotReloadManager)

private:
	AssetHotReloadManager() = default;
	virtual ~AssetHotReloadManager() = default;
	EResult Initialize(void* arg = nullptr);

public:
	void Free() override;

	// 매 프레임 메인 루프에서 변경 큐를 디바운스 확인 후 핫스왑
	void Update();

	void NotifyFileModified(const std::filesystem::path& filePath);

	[[nodiscard]] bool IsAutoReloadEnabled() const { return m_AutoReload; }
	void SetAutoReload(bool enable) { m_AutoReload = enable; }
	[[nodiscard]] bool& GetAutoReloadRef() { return m_AutoReload; }

private:
	void ProcessModifiedTexture(const std::filesystem::path& texturePath);
	void ProcessModifiedMaterial(const std::filesystem::path& materialPath);
	void ProcessModifiedLocalization(const std::filesystem::path& locPath);
	std::filesystem::path ResolveTextureDirectory();

private:
	efsw::FileWatcher* m_FileWatcher = nullptr;
	efsw::FileWatchListener* m_FileListener = nullptr;
	efsw::WatchID m_WatchID = 0;
	std::filesystem::path m_WatchDirectory;

	std::mutex m_QueueMutex;
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_PendingFiles;
	bool m_AutoReload = true;
};

END